// 票务相关 API

import wsClient from './client';
import type {
  ViewTicketsRequest,
  ViewTicketsResponse,
} from '../types';

export const ticketApi = {
  // 查看所有票务
  async getTickets(): Promise<ViewTicketsResponse> {
    const request: ViewTicketsRequest = {
      type: 4,
    };
    return wsClient.send<ViewTicketsResponse>(request);
  },
};
