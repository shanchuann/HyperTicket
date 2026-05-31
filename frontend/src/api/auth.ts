// 认证相关 API

import wsClient from './client';
import type {
  LoginRequest,
  RegisterRequest,
  AuthResponse,
} from '../types';

export const authApi = {
  // 登录
  async login(tel: string, password: string): Promise<AuthResponse> {
    const request: LoginRequest = {
      type: 1,
      tel,
      password,
    };
    return wsClient.send<AuthResponse>(request);
  },

  // 注册
  async register(tel: string, username: string, password: string): Promise<AuthResponse> {
    const request: RegisterRequest = {
      type: 2,
      tel,
      username,
      password,
    };
    return wsClient.send<AuthResponse>(request);
  },

  // 退出
  async logout(token: string): Promise<void> {
    const request = {
      type: 3 as const,
      token,
    };
    await wsClient.send(request);
    // 清除本地存储
    localStorage.removeItem('token');
    localStorage.removeItem('user');
  },
};
