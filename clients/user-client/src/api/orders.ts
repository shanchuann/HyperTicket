import wsClient from './client';
import type { ViewMyOrdersBackendResponse, BackendResponse, Order } from '../types';
import { normalizeOrder } from '../types';

export type CreateOrderResponse = BackendResponse & {
  reservation_id?: string;
  order_status?: string;
  pay_deadline_minutes?: number;
  total_price?: number;
};

export type PaymentResponse = BackendResponse & {
  payment_no?: string;
  payment_status?: 'PROCESSING' | 'SUCCESS' | 'FAILED' | 'REFUNDED';
  amount?: number;
  method?: string;
  order_status?: string;
};

export const orderApi = {
  async createOrder(token: string, ticketId: number, quantity = 1): Promise<CreateOrderResponse> {
    return wsClient.send<CreateOrderResponse>({
      type: 5,
      token,
      index: ticketId,       // 后端期望整数
      ...(quantity > 1 ? { quantity } : {}),
    });
  },

  // v3 异步支付：发起支付创建流水，由后端模拟网关异步结算
  async payOrder(token: string, reservationId: number, method = 'MOCK'): Promise<PaymentResponse> {
    return wsClient.send<PaymentResponse>({ type: 20, token, index: String(reservationId), method });
  },

  async queryPayment(token: string, reservationId: number): Promise<PaymentResponse> {
    return wsClient.send<PaymentResponse>({ type: 24, token, index: String(reservationId) });
  },

  // 发起支付并轮询至终态；超时抛错，由调用方兜底刷新订单
  async payAndWait(token: string, reservationId: number, method = 'MOCK', timeoutMs = 15000): Promise<PaymentResponse> {
    const first = await this.payOrder(token, reservationId, method);
    if (first.payment_status && first.payment_status !== 'PROCESSING') return first;
    const deadline = Date.now() + timeoutMs;
    while (Date.now() < deadline) {
      await new Promise(r => setTimeout(r, 800));
      const p = await this.queryPayment(token, reservationId);
      if (p.payment_status && p.payment_status !== 'PROCESSING') return p;
    }
    throw new Error('支付结果确认超时，请稍后在订单列表查看');
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
