import wsClient from './client';
import type { ViewMyOrdersBackendResponse, ViewSeatsBackendResponse, BackendResponse, Order, Seat } from '../types';
import { normalizeOrder } from '../types';
import { toChineseError } from './errors';

export type CreateOrderResponse = BackendResponse & {
  request_id?: string;
  reservation_id?: string;
  order_status?: string;
  pay_deadline_minutes?: number;
  total_price?: number;
};

export type OrderQueueResponse = BackendResponse & {
  request_id: string;
  reservation_id?: string;
  order_status: 'QUEUED' | 'DUPLICATE' | 'PENDING' | 'FAILED';
};

type PendingOrderRequest = {
  requestId: string;
  createdAt: number;
};

const pendingOrderKey = 'pendingOrderRequest';
const pendingOrderMaxAgeMs = 30 * 60 * 1000;

const savePendingOrder = (requestId: string) => {
  const pending: PendingOrderRequest = { requestId, createdAt: Date.now() };
  localStorage.setItem(pendingOrderKey, JSON.stringify(pending));
};

const readPendingOrder = (): PendingOrderRequest | null => {
  const raw = localStorage.getItem(pendingOrderKey);
  if (!raw) return null;
  try {
    const value = JSON.parse(raw) as Partial<PendingOrderRequest>;
    if (typeof value.requestId === 'string' && typeof value.createdAt === 'number') {
      return value as PendingOrderRequest;
    }
  } catch {
    // 兼容早期版本直接保存 request_id 的格式。
    if (/^[A-Za-z0-9_-]{8,64}$/.test(raw)) return { requestId: raw, createdAt: Date.now() };
  }
  localStorage.removeItem(pendingOrderKey);
  return null;
};

const clearPendingOrder = () => localStorage.removeItem(pendingOrderKey);

const createRequestId = () => {
  const suffix = globalThis.crypto?.randomUUID?.().replaceAll('-', '') ??
    `${Date.now()}_${Math.random().toString(36).slice(2)}`;
  return `checkout_${suffix}`.slice(0, 64);
};

export type PaymentResponse = BackendResponse & {
  payment_no?: string;
  payment_status?: 'PROCESSING' | 'SUCCESS' | 'FAILED' | 'REFUNDED';
  amount?: number;
  method?: string;
  order_status?: string;
};

export const orderApi = {
  async createOrder(token: string, ticketId: number, quantity = 1, requestId = createRequestId(), seatId?: number): Promise<CreateOrderResponse> {
    return wsClient.send<CreateOrderResponse>({
      type: 5,
      token,
      index: ticketId,       // 后端期望整数
      ...(quantity > 1 ? { quantity } : {}),
      request_id: requestId,
      ...(seatId ? { seat_id: seatId } : {}),
    });
  },

  async queryOrder(token: string, requestId: string): Promise<OrderQueueResponse> {
    return wsClient.send<OrderQueueResponse>({ type: 25, token, request_id: requestId });
  },

  async waitForQueuedOrder(token: string, requestId: string, timeoutMs = 20000): Promise<OrderQueueResponse> {
    const deadline = Date.now() + timeoutMs;
    while (Date.now() < deadline) {
      const status = await this.queryOrder(token, requestId);
      if (status.order_status === 'PENDING') {
        clearPendingOrder();
        return status;
      }
      if (status.order_status === 'FAILED') {
        clearPendingOrder();
        throw new Error(toChineseError(status.reason, '库存竞争失败，请重新选择场次'));
      }
      await new Promise(resolve => setTimeout(resolve, 500));
    }
    throw new Error('订单仍在排队处理中，可稍后在订单列表查看');
  },

  async createOrderAndWait(token: string, ticketId: number, quantity = 1, timeoutMs = 20000): Promise<OrderQueueResponse> {
    const requestId = createRequestId();
    savePendingOrder(requestId);
    const queued = await this.createOrder(token, ticketId, quantity, requestId);
    if (!queued.request_id) throw new Error('服务端未返回下单请求编号');
    return this.waitForQueuedOrder(token, queued.request_id, timeoutMs);
  },

  async createSeatOrderAndWait(token: string, ticketId: number, seatId: number, timeoutMs = 20000): Promise<OrderQueueResponse> {
    const requestId = createRequestId();
    savePendingOrder(requestId);
    const queued = await this.createOrder(token, ticketId, 1, requestId, seatId);
    if (!queued.request_id) throw new Error('服务端未返回下单请求编号');
    return this.waitForQueuedOrder(token, queued.request_id, timeoutMs);
  },

  async resumePendingOrder(token: string, timeoutMs = 8000): Promise<OrderQueueResponse | null> {
    const pending = readPendingOrder();
    if (!pending) return null;
    if (Date.now() - pending.createdAt > pendingOrderMaxAgeMs) {
      clearPendingOrder();
      return null;
    }
    try {
      return await this.waitForQueuedOrder(token, pending.requestId, timeoutMs);
    } catch (error) {
      if (error instanceof Error && error.message === '未找到对应订单') clearPendingOrder();
      throw error;
    }
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

  async getSeats(ticketId: number): Promise<{ hasSeats: boolean; seats: Seat[] }> {
    const resp = await wsClient.send<ViewSeatsBackendResponse>({ type: 17, index: ticketId });
    return { hasSeats: resp.has_seats ?? false, seats: resp.arr || [] };
  },
};
