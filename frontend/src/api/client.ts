// WebSocket 客户端 - 桥接 TCP + JSON 协议

import type { BaseRequest, BaseResponse } from '../types';

// 存储待处理请求的回调类型
type RequestCallback = {
  resolve: (value: BaseResponse) => void;
  reject: (reason: any) => void;
  timeout: ReturnType<typeof setTimeout>;
};

class WebSocketClient {
  private ws: WebSocket | null = null;
  private url: string;
  private reconnectAttempts = 0;
  private maxReconnectAttempts = 5;
  private reconnectDelay = 3000;
  private messageQueue: Array<{ request: BaseRequest; callback: RequestCallback }> = [];
  private pendingRequests = new Map<string, RequestCallback>();

  constructor(url: string = 'ws://localhost:8080') {
    this.url = url;
  }

  connect(): Promise<void> {
    return new Promise((resolve, reject) => {
      try {
        this.ws = new WebSocket(this.url);

        this.ws.onopen = () => {
          console.log('WebSocket 连接已建立');
          this.reconnectAttempts = 0;

          // 处理队列中的消息
          while (this.messageQueue.length > 0) {
            const item = this.messageQueue.shift();
            if (item) {
              this.sendWithCallback(item.request, item.callback);
            }
          }

          resolve();
        };

        this.ws.onmessage = (event) => {
          try {
            const response: BaseResponse = JSON.parse(event.data);
            this.handleResponse(response);
          } catch (error) {
            console.error('解析响应失败:', error);
          }
        };

        this.ws.onerror = (error) => {
          console.error('WebSocket 错误:', error);
          reject(error);
        };

        this.ws.onclose = () => {
          console.log('WebSocket 连接已关闭');
          this.handleReconnect();
        };
      } catch (error) {
        reject(error);
      }
    });
  }

  private handleReconnect() {
    if (this.reconnectAttempts < this.maxReconnectAttempts) {
      this.reconnectAttempts++;
      console.log(`尝试重连 (${this.reconnectAttempts}/${this.maxReconnectAttempts})...`);

      setTimeout(() => {
        this.connect().catch((error) => {
          console.error('重连失败:', error);
        });
      }, this.reconnectDelay);
    } else {
      console.error('达到最大重连次数，放弃重连');
      // 清理所有待处理的请求
      this.pendingRequests.forEach(({ reject, timeout }) => {
        clearTimeout(timeout);
        reject(new Error('WebSocket 连接已断开'));
      });
      this.pendingRequests.clear();
    }
  }

  private handleResponse(response: BaseResponse) {
    const requestId = this.getRequestId(response);
    const pending = this.pendingRequests.get(requestId);

    if (pending) {
      clearTimeout(pending.timeout);
      this.pendingRequests.delete(requestId);

      if (response.success) {
        pending.resolve(response);
      } else {
        pending.reject(new Error(response.message || '请求失败'));
      }
    }
  }

  private getRequestId(response: BaseResponse): string {
    return JSON.stringify(response).substring(0, 50);
  }

  private sendWithCallback(request: BaseRequest, callback: RequestCallback): void {
    if (!this.ws || this.ws.readyState !== WebSocket.OPEN) {
      this.messageQueue.push({ request, callback });

      if (!this.ws || this.ws.readyState === WebSocket.CLOSED) {
        this.connect().catch((err) => callback.reject(err));
      }
      return;
    }

    try {
      const message = JSON.stringify(request) + '\n';
      this.ws.send(message);

      const requestId = JSON.stringify(request).substring(0, 50);
      this.pendingRequests.set(requestId, callback);
    } catch (error) {
      callback.reject(error);
    }
  }

  send<T extends BaseResponse>(request: BaseRequest): Promise<T> {
    return new Promise((resolve, reject) => {
      const timeout = setTimeout(() => {
        const requestId = JSON.stringify(request).substring(0, 50);
        this.pendingRequests.delete(requestId);
        reject(new Error('请求超时'));
      }, 30000);

      const callback: RequestCallback = {
        resolve: (response: BaseResponse) => resolve(response as T),
        reject,
        timeout,
      };

      this.sendWithCallback(request, callback);
    });
  }

  disconnect() {
    if (this.ws) {
      this.ws.close();
      this.ws = null;
    }

    this.pendingRequests.forEach(({ reject, timeout }) => {
      clearTimeout(timeout);
      reject(new Error('连接已主动断开'));
    });
    this.pendingRequests.clear();
    this.messageQueue = [];
  }

  isConnected(): boolean {
    return this.ws !== null && this.ws.readyState === WebSocket.OPEN;
  }
}

// 单例模式
export const wsClient = new WebSocketClient(
  import.meta.env.VITE_WS_URL || 'ws://localhost:8080'
);

export default wsClient;
