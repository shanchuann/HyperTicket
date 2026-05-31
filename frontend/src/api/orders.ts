// 订单相关 API

import wsClient from './client';
import type {
  OrderRequest,
  ViewMyOrdersRequest,
  ViewMyOrdersResponse,
  CancelOrderRequest,
  BaseResponse,
} from '../types';

export const orderApi = {
  // 创建订单
  async createOrder(token: string, ticketId: number, quantity: number): Promise<BaseResponse> {
    const request: OrderRequest = {
      type: 5,
      token,
      ticket_id: ticketId,
      quantity,
    };
    return wsClient.send<BaseResponse>(request);
  },

  // 查看我的订单
  async getMyOrders(token: string): Promise<ViewMyOrdersResponse> {
    const request: ViewMyOrdersRequest = {
      type: 6,
      token,
    };
    return wsClient.send<ViewMyOrdersResponse>(request);
  },

  // 取消订单
  async cancelOrder(token: string, reservationId: number): Promise<BaseResponse> {
    const request: CancelOrderRequest = {
      type: 7,
      token,
      reservation_id: reservationId,
    };
    return wsClient.send<BaseResponse>(request);
  },
};
