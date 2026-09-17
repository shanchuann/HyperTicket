import type { BackendResponse } from '../types';
import { toChineseError } from './errors';

type PendingRequest = {
  resolve: (response: BackendResponse) => void;
  reject: (error: Error) => void;
  timer: ReturnType<typeof setTimeout>;
  timedOut: boolean;
};

class BrowserWebSocketClient {
  private socket: WebSocket | null = null;
  private connecting: Promise<void> | null = null;
  private requests: PendingRequest[] = [];
  private readonly url = import.meta.env.VITE_WS_URL || BrowserWebSocketClient.defaultUrl();

  private static defaultUrl() {
    const isTauri = typeof window !== 'undefined' && '__TAURI_INTERNALS__' in window;
    if (import.meta.env.DEV || isTauri) return 'ws://localhost:8080/ws';
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    return `${protocol}//${window.location.host}/ws`;
  }

  private connect(): Promise<void> {
    if (this.socket?.readyState === WebSocket.OPEN) return Promise.resolve();
    if (this.connecting) return this.connecting;
    this.connecting = new Promise((resolve, reject) => {
      const socket = new WebSocket(this.url);
      this.socket = socket;
      socket.onopen = () => { this.connecting = null; resolve(); };
      socket.onerror = () => { this.connecting = null; reject(new Error('无法连接票务服务')); };
      socket.onclose = () => {
        this.connecting = null;
        this.socket = null;
        const pending = this.requests.splice(0);
        for (const request of pending) {
          clearTimeout(request.timer);
          request.reject(new Error('网络连接已断开'));
        }
      };
      socket.onmessage = event => {
        const request = this.requests.shift();
        if (!request) return;
        clearTimeout(request.timer);
        if (request.timedOut) return;
        try {
          const response = JSON.parse(String(event.data)) as BackendResponse;
          if (response.status !== 'OK') request.reject(new Error(toChineseError(response.reason)));
          else request.resolve(response);
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
      const pending: PendingRequest = {
        resolve: response => resolve(response as T),
        reject,
        timedOut: false,
        timer: window.setTimeout(() => {
          pending.timedOut = true;
          reject(new Error('请求超时，请检查网络连接'));
        }, 15000),
      };
      this.requests.push(pending);
      this.socket?.send(JSON.stringify(payload));
    });
  }

  async isConnected(): Promise<boolean> {
    try { await this.connect(); return this.socket?.readyState === WebSocket.OPEN; }
    catch { return false; }
  }
}

export const wsClient = new BrowserWebSocketClient();
export default wsClient;
