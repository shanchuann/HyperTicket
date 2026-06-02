import wsClient from './client';
import type { ViewMyOrdersBackendResponse, BackendResponse, Order } from '../types';
import { normalizeOrder } from '../types';

export const orderApi = {
  async createOrder(token: string, ticketId: number): Promise<BackendResponse> {
    return wsClient.send<BackendResponse>({
      type: 5,
      token,
      index: ticketId,       // 后端期望整数
    });
  },

  async getMyOrders(token: string): Promise<Order[]> {
    const resp = await wsClient.send<ViewMyOrdersBackendResponse>({
      type: 6,
      token,
    });
    return (resp.arr || []).map(normalizeOrder);
  },

  async cancelOrder(token: string, reservationId: number): Promise<BackendResponse> {
    return wsClient.send<BackendResponse>({
      type: 7,
      token,
      index: reservationId,  // 后端期望整数
    });
  },
};
