// HyperTicket API 类型定义（与后端真实协议对齐）

// 操作类型
export type OperationType = 1 | 2 | 3 | 4 | 5 | 6 | 7;

export const OperationType = {
  LOGIN: 1 as 1,
  REGISTER: 2 as 2,
  EXIT: 3 as 3,
  VIEW: 4 as 4,
  ORDER: 5 as 5,
  VIEW_MY: 6 as 6,
  CANCEL: 7 as 7,
};

// 基础响应（后端统一用 status: "OK" | "ERR"）
export interface BackendResponse {
  status: 'OK' | 'ERR';
  reason?: string;
  [key: string]: unknown;
}

// 认证
export interface LoginRequest {
  type: 1;
  usertel: string;
  passward: string; // 后端拼写
}

export interface RegisterRequest {
  type: 2;
  usertel: string;
  passward: string;
  username: string;
}

export interface AuthBackendResponse extends BackendResponse {
  token?: string;
  username?: string;
}

// 票务（后端字段）
export interface BackendTicket {
  tk_id: string;
  title: string;
  addr: string;
  max: string;
  num: string;
  use_date: string;
  status: string;
  category?: string;
  price?: number;
  city?: string;
  artist?: string;
  description?: string;
  notice?: string;
  hot?: number;
}

// 票务（前端展示用，规范化后的字段）
export interface Ticket {
  id: number;
  title: string;
  venue: string;
  total_seats: number;
  available_seats: number;
  event_date: string;
  status: number;
  category?: string;
  price: number;
  city: string;
  artist: string;
  description?: string;
  notice?: string;
  hot?: number;
}

export interface ViewTicketsBackendResponse extends BackendResponse {
  arr?: BackendTicket[];
  num?: number;
}

// 订单（后端字段）
export interface BackendOrder {
  reservation_id: string;
  tk_id: string;
  title?: string;
  addr?: string;
  num: string;
  status: string;
  use_date?: string;
  expire_at?: string;
  ticket_price?: number;
  created_at?: string;
  category?: string;
  seat_label?: string;
  seat_tier?: string;
  seat_price?: number;
  order_no?: string;
}

// 订单（前端展示用）
export interface Order {
  id: number;
  ticket_id: number;
  title: string;
  venue: string;
  quantity: number;
  status: 'PENDING' | 'CONFIRMED' | 'CANCELLED' | 'EXPIRED';
  event_date: string;
  expire_at: string;
  ticket_price: number;
  created_at: string;
  category: string;
  seat_label: string;
  seat_tier: string;
  seat_price: number;
  order_no: string;
}

export interface ViewMyOrdersBackendResponse extends BackendResponse {
  arr?: BackendOrder[];
}

export interface Seat {
  id: number;
  label: string;
  row: string;
  col: number;
  tier: 'VIP' | 'Standard' | 'Economy';
  price: number;
  status: 'AVAILABLE' | 'SOLD';
}

export interface ViewSeatsBackendResponse extends BackendResponse {
  has_seats: boolean;
  arr?: Seat[];
  num?: number;
}

// 下单请求
export interface OrderRequest {
  type: 5;
  token: string;
  index: string; // 后端用 index 而非 ticket_id
}

// 取消请求
export interface CancelRequest {
  type: 7;
  token: string;
  index: string; // 后端用 index 表示 reservation_id
}

// 用户信息
export interface User {
  tel: string;
  username: string;
  token: string;
}

// 主题类型
export type Theme = 'light' | 'dark';

// 工具函数：将后端票务格式转换为前端格式
export function normalizeTicket(t: BackendTicket): Ticket {
  return {
    id: parseInt(t.tk_id),
    title: t.title,
    venue: t.addr,
    total_seats: parseInt(t.max),
    available_seats: parseInt(t.num),
    event_date: t.use_date,
    status: parseInt(t.status),
    category: t.category,
    price: t.price ?? 0,
    city: t.city || '',
    artist: t.artist || '',
    description: t.description,
    notice: t.notice,
    hot: t.hot,
  };
}

// 工具函数：将后端订单格式转换为前端格式
export function normalizeOrder(o: BackendOrder): Order {
  const statusMap: Record<string, Order['status']> = {
    PENDING: 'PENDING',
    CONFIRMED: 'CONFIRMED',
    CANCELLED: 'CANCELLED',
    EXPIRED: 'EXPIRED',
    '0': 'CANCELLED',
    '1': 'CONFIRMED',
  };
  return {
    id: parseInt(o.reservation_id),
    ticket_id: parseInt(o.tk_id),
    title: o.title || `票务 #${o.tk_id}`,
    venue: o.addr || '',
    quantity: parseInt(o.num),
    status: statusMap[o.status] || 'PENDING',
    event_date: o.use_date || '',
    expire_at: o.expire_at || '',
    ticket_price: o.ticket_price ?? 0,
    created_at: o.created_at || '',
    category: o.category || 'concert',
    seat_label: o.seat_label || '',
    seat_tier: o.seat_tier || '',
    seat_price: o.seat_price ?? 0,
    order_no: o.order_no || '',
  };
}
