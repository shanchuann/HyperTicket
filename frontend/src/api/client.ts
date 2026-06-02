// WebSocket 客户端 - 连接桥接服务器，转发至后端 TCP

import type { BackendResponse } from '../types';

type Callback = {
  resolve: (r: BackendResponse) => void;
  reject: (e: Error) => void;
  timeout: ReturnType<typeof setTimeout>;
  timedOut: boolean;  // 超时标记，不从队列移除（保持响应顺序对齐）
};

class WebSocketClient {
  private ws: WebSocket | null = null;
  private readonly url: string;
  private queue: Callback[] = [];
  private reconnectCount = 0;
  private readonly maxReconnect = 5;
  private pendingMessages: string[] = [];

  constructor(url: string) {
    this.url = url;
    this.connect();
  }

  connect(): Promise<void> {
    return new Promise((resolve, reject) => {
      if (this.ws?.readyState === WebSocket.OPEN) {
        resolve();
        return;
      }

      try {
        this.ws = new WebSocket(this.url);

        this.ws.onopen = () => {
          console.log('[WS] Connected to bridge');
          this.reconnectCount = 0;
          for (const msg of this.pendingMessages) {
            this.ws!.send(msg);
          }
          this.pendingMessages = [];
          resolve();
        };

        this.ws.onmessage = (event) => {
          const text = event.data as string;
          console.log('[WS] Received:', text.substring(0, 150));

          // 按顺序消费队列；跳过已超时的槽位（其 reject 已触发），继续消费直到找到有效回调
          while (this.queue.length > 0 && this.queue[0].timedOut) {
            this.queue.shift();
          }

          const cb = this.queue.shift();
          if (!cb) return;

          clearTimeout(cb.timeout);
          try {
            const resp: BackendResponse = JSON.parse(text);
            if (resp.status === 'OK') {
              cb.resolve(resp);
            } else {
              cb.reject(new Error(resp.reason || '请求失败'));
            }
          } catch {
            cb.reject(new Error('响应解析失败'));
          }
        };

        this.ws.onerror = () => {
          reject(new Error('WebSocket 连接错误'));
        };

        this.ws.onclose = () => {
          console.log('[WS] Connection closed');
          const pending = [...this.queue];
          this.queue = [];
          for (const cb of pending) {
            clearTimeout(cb.timeout);
            if (!cb.timedOut) cb.reject(new Error('连接已断开'));
          }
          this.scheduleReconnect();
        };
      } catch (e) {
        reject(e);
      }
    });
  }

  private scheduleReconnect() {
    if (this.reconnectCount >= this.maxReconnect) return;
    this.reconnectCount++;
    const delay = Math.min(1000 * this.reconnectCount, 5000);
    console.log(`[WS] Reconnecting in ${delay}ms (${this.reconnectCount}/${this.maxReconnect})`);
    setTimeout(() => this.connect().catch(console.error), delay);
  }

  send<T extends BackendResponse>(payload: object): Promise<T> {
    return new Promise((resolve, reject) => {
      const cb: Callback = {
        resolve: (r) => resolve(r as T),
        reject,
        timedOut: false,
        timeout: setTimeout(() => {
          cb.timedOut = true;
          reject(new Error('请求超时'));
        }, 15000),
      };
      this.queue.push(cb);

      const msg = JSON.stringify(payload);
      if (this.ws?.readyState === WebSocket.OPEN) {
        this.ws.send(msg);
      } else {
        this.pendingMessages.push(msg);
        this.connect().catch(console.error);
      }
    });
  }

  isConnected(): boolean {
    return this.ws?.readyState === WebSocket.OPEN;
  }

  disconnect() {
    this.ws?.close();
    this.ws = null;
  }
}

const WS_URL = (import.meta as any).env?.VITE_WS_URL || 'ws://localhost:8080';
export const wsClient = new WebSocketClient(WS_URL);
export default wsClient;
