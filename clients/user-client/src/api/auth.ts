import wsClient from './client';
import type { AuthBackendResponse } from '../types';

export const authApi = {
  async login(tel: string, password: string): Promise<AuthBackendResponse> {
    return wsClient.send<AuthBackendResponse>({
      type: 1,
      usertel: tel,
      passward: password,  // 后端拼写
    });
  },

  async register(tel: string, username: string, password: string): Promise<AuthBackendResponse> {
    return wsClient.send<AuthBackendResponse>({
      type: 2,
      usertel: tel,
      passward: password,
      username,
    });
  },

  async logout(token: string): Promise<void> {
    await wsClient.send({ type: 3, token }).catch(() => {});
    localStorage.removeItem('token');
    localStorage.removeItem('user');
  },
};
