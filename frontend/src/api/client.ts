// WebSocket 客户端 - 连接桥接服务器，转发至后端 TCP

import type { BackendResponse } from '../types';
import { reportEvent } from '../utils/monitor';

type Callback = {
  resolve: (r: BackendResponse) => void;
  reject: (e: Error) => void;
  timeout: ReturnType<typeof setTimeout>;
  timedOut: boolean;  // 超时标记，不从队列移除（保持响应顺序对齐）
};

const SENSITIVE_FIELDS = new Set([
  'passward', 'password', 'new_password', 'token', 'admin_token',
  'reset_token', 'verification_token', 'code',
]);

function safePayload(payload: object): string {
  const redacted = Object.fromEntries(
    Object.entries(payload).map(([key, value]) => [key, SENSITIVE_FIELDS.has(key) ? '[REDACTED]' : value]),
  );
  return JSON.stringify(redacted).slice(0, 300);
}

function safeResponseText(raw: string): string {
  try {
    return safePayload(JSON.parse(raw) as Record<string, unknown>);
  } catch {
    return '[non-json response]';
  }
}

class WebSocketClient {
  private ws: WebSocket | null = null;
  private readonly url: string;
  private queue: Callback[] = [];
  private reconnectCount = 0;
  private readonly maxReconnect = 5;
  private pendingMessages: string[] = [];
  private connectingPromise: Promise<void> | null = null;

  constructor(url: string) {
    this.url = url;
    this.connect().catch(() => {});
  }

  connect(): Promise<void> {
    if (this.ws?.readyState === WebSocket.OPEN) {
      return Promise.resolve();
    }
    // 已有连接正在进行中，复用同一个 Promise，避免创建多个 WebSocket
    if (this.connectingPromise) {
      return this.connectingPromise;
    }

    this.connectingPromise = new Promise((resolve, reject) => {
      let settled = false;
      const settle = () => { settled = true; this.connectingPromise = null; };

      const ws = new WebSocket(this.url);
      this.ws = ws;

      ws.onopen = () => {
        console.log('[WS] Connected to bridge');
        this.reconnectCount = 0;
        settle();
        for (const msg of this.pendingMessages) {
          ws.send(msg);
        }
        this.pendingMessages = [];
        resolve();
      };

      ws.onmessage = (event) => {
        const text = event.data as string;
        console.log('[WS] Received:', safeResponseText(text));

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
            const reason = resp.reason || '请求失败';
            // 登录过期自动跳转
            if (reason === 'UNAUTHORIZED') {
              localStorage.removeItem('token');
              localStorage.removeItem('user');
              window.location.replace('/auth/login');
              return;
            }
            if (reason === 'ADMIN_UNAUTHORIZED') {
              localStorage.removeItem('admin_token');
              localStorage.removeItem('admin_user');
              window.location.replace('/admin/login');
              return;
            }
            cb.reject(new Error(reason));
          }
        } catch {
          reportEvent({
            level: 'ERROR',
            source: 'ws-client',
            message: '后端响应 JSON 解析失败',
            context: { raw: text.slice(0, 300) },
          });
          cb.reject(new Error('响应解析失败'));
        }
      };

      ws.onerror = () => {
        reportEvent({
          level: 'ERROR',
          source: 'ws-client',
          message: `WebSocket 连接错误: ${this.url}`,
          context: { reconnectCount: this.reconnectCount, queueLength: this.queue.length },
        });
        if (!settled) {
          settle();
          reject(new Error('WebSocket 连接错误'));
        }
      };

      ws.onclose = () => {
        console.log('[WS] Connection closed');
        if (!settled) {
          settle();
          reject(new Error('连接已关闭'));
        } else {
          this.connectingPromise = null;
        }
        const pending = [...this.queue];
        this.queue = [];
        if (pending.some((cb) => !cb.timedOut)) {
          reportEvent({
            level: 'ERROR',
            source: 'ws-client',
            message: `WebSocket 连接断开，${pending.filter((cb) => !cb.timedOut).length} 个未完成请求被拒绝`,
            context: { reconnectCount: this.reconnectCount },
          });
        }
        for (const cb of pending) {
          clearTimeout(cb.timeout);
          if (!cb.timedOut) cb.reject(new Error('网络连接已断开，请刷新重试'));
        }
        this.scheduleReconnect();
      };
    });

    return this.connectingPromise;
  }

  private scheduleReconnect() {
    if (this.reconnectCount >= this.maxReconnect) {
      reportEvent({
        level: 'FATAL',
        source: 'ws-client',
        message: `WebSocket 重连 ${this.maxReconnect} 次全部失败，已停止重连: ${this.url}`,
      });
      return;
    }
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
          reportEvent({
            level: 'ERROR',
            source: 'ws-client',
            message: '请求超时（15s 未收到后端响应）',
            context: { payload: safePayload(payload), wsState: this.ws?.readyState ?? -1 },
          });
          reject(new Error('请求超时，请检查网络连接'));
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

const WS_URL = import.meta.env.VITE_WS_URL || 'ws://localhost:8080';
export const wsClient = new WebSocketClient(WS_URL);
export default wsClient;
