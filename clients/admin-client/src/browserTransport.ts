type BackendResponse = { status: 'OK' | 'ERR'; reason?: string; [key: string]: unknown };
type PendingRequest = {
  resolve: (response: BackendResponse) => void;
  reject: (error: Error) => void;
  timer: ReturnType<typeof setTimeout>;
  timedOut: boolean;
};

class AdminBrowserTransport {
  private socket: WebSocket | null = null;
  private connecting: Promise<void> | null = null;
  private pending: PendingRequest[] = [];

  private url() {
    if (import.meta.env.VITE_WS_URL) return import.meta.env.VITE_WS_URL;
    const isTauri = typeof window !== 'undefined' && '__TAURI_INTERNALS__' in window;
    if (import.meta.env.DEV || isTauri) return 'ws://localhost:8080/ws';
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    return `${protocol}//${window.location.host}/ws`;
  }

  private connect(): Promise<void> {
    if (this.socket?.readyState === WebSocket.OPEN) return Promise.resolve();
    if (this.connecting) return this.connecting;

    this.connecting = new Promise((resolve, reject) => {
      const socket = new WebSocket(this.url());
      this.socket = socket;
      socket.onopen = () => { this.connecting = null; resolve(); };
      socket.onerror = () => { this.connecting = null; reject(new Error('无法连接票务服务')); };
      socket.onclose = () => {
        this.connecting = null;
        this.socket = null;
        for (const request of this.pending.splice(0)) {
          clearTimeout(request.timer);
          request.reject(new Error('网络连接已断开'));
        }
      };
      socket.onmessage = event => {
        const request = this.pending.shift();
        if (!request) return;
        clearTimeout(request.timer);
        if (request.timedOut) return;
        try {
          request.resolve(JSON.parse(String(event.data)) as BackendResponse);
        } catch {
          request.reject(new Error('服务端响应格式错误'));
        }
      };
    });
    return this.connecting;
  }

  async send<T extends BackendResponse>(payload: object): Promise<T> {
    await this.connect();
    return new Promise<T>((resolve, reject) => {
      const request: PendingRequest = {
        resolve: response => resolve(response as T),
        reject,
        timedOut: false,
        timer: window.setTimeout(() => {
          request.timedOut = true;
          reject(new Error('请求超时，请检查网络连接'));
        }, 15000),
      };
      this.pending.push(request);
      this.socket?.send(JSON.stringify(payload));
    });
  }
}

const transport = new AdminBrowserTransport();
export const browserSend = <T extends BackendResponse>(payload: object) => transport.send<T>(payload);
