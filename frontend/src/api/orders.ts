import wsClient from './client';
import type { ViewMyOrdersBackendResponse, ViewSeatsBackendResponse, BackendResponse, Order, Seat } from '../types';
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
  async createOrder(token: string, ticketId: number, seatId?: number, quantity = 1): Promise<CreateOrderResponse> {
    return wsClient.send<CreateOrderResponse>({
      type: 5,
      token,
      index: ticketId,
      ...(quantity > 1 ? { quantity } : {}),
      ...(seatId ? { seat_id: seatId } : {}),
    });
  },

  // v3 异步支付：发起支付创建流水（PROCESSING），由后端模拟网关异步结算
  async payOrder(token: string, reservationId: number, method = 'MOCK'): Promise<PaymentResponse> {
    return wsClient.send<PaymentResponse>({ type: 20, token, index: String(reservationId), method });
  },

  // 轮询支付结果，直到 payment_status 变为 SUCCESS/FAILED/REFUNDED
  async queryPayment(token: string, reservationId: number): Promise<PaymentResponse> {
    return wsClient.send<PaymentResponse>({ type: 24, token, index: String(reservationId) });
  },

  // 发起支付并轮询至终态；超时（网关长时间未结算）抛错，由调用方兜底刷新订单
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
    const resp = await wsClient.send<ViewMyOrdersBackendResponse>({ type: 6, token });
    return (resp.arr || []).map(normalizeOrder);
  },

  async cancelOrder(token: string, reservationId: number): Promise<BackendResponse> {
    return wsClient.send<BackendResponse>({ type: 7, token, index: reservationId });
  },

  async deleteOrder(token: string, reservationId: number): Promise<BackendResponse> {
    return wsClient.send<BackendResponse>({ type: 16, token, index: reservationId });
  },

  async getSeats(ticketId: number): Promise<{ hasSeats: boolean; seats: Seat[] }> {
    const resp = await wsClient.send<ViewSeatsBackendResponse>({ type: 17, index: ticketId });
    return {
      hasSeats: resp.has_seats ?? false,
      seats: (resp.arr || []) as Seat[],
    };
  },
};
