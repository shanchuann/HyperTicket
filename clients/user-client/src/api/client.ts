// Tauri 版 TCP 客户端：通过 invoke 调用 Rust send_request，
// 接口与 web 端 WebSocketClient.send() 保持一致，无需修改 auth.ts/tickets.ts/orders.ts。
import { invoke } from '@tauri-apps/api/core';
import type { BackendResponse } from '../types';
import { toChineseError } from './errors';

const tauriClient = {
  send<T extends BackendResponse>(payload: object): Promise<T> {
    return invoke<BackendResponse>('send_request', { payload }).then(resp => {
      if (resp.status !== 'OK') {
        throw new Error(toChineseError(resp.reason));
      }
      return resp as T;
    });
  },

  isConnected(): Promise<boolean> {
    return invoke<boolean>('check_connection');
  },
};

// 与 web 端相同的导出名，auth.ts/tickets.ts/orders.ts 无需修改
export const wsClient = tauriClient;
export default tauriClient;
