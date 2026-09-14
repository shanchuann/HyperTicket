// HyperTicket API 类型定义（与后端真实协议对齐）

export const OperationType = {
  LOGIN: 1, REGISTER: 2, EXIT: 3, VIEW: 4, ORDER: 5,
  VIEW_MY: 6, CANCEL: 7, DELETE_ORDER: 16, VIEW_SEATS: 17,
} as const;

export interface BackendResponse {
  status: 'OK' | 'ERR';
  reason?: string;
  [key: string]: unknown;
}

export interface AuthBackendResponse extends BackendResponse {
  token?: string;
  username?: string;
}

// ── Tickets ──────────────────────────────────────────────────────────────────

export interface BackendTicket {
  tk_id: string;
  title: string;
  addr: string;
  max: string;
  num: string;
  use_date: string;
  status: string;
  tk_status?: string; // 详情接口用此字段（status 被协议字段 OK/ERR 占用）
  cover_image?: string;
  category?: string;
  price?: number;
  city?: string;
  artist?: string;
  description?: string;
  notice?: string;
  hot?: number;
}

export type TicketCategory = 'concert' | 'sports' | 'movie' | 'theater' | 'exhibition';

export interface Ticket {
  id: number;
  title: string;
  venue: string;
  total_seats: number;
  available_seats: number;
  event_date: string;
  status: number;
  cover_image?: string;
  category: TicketCategory;
  price: number; // yuan
  city: string;
  artist: string;
  description?: string; // 详情接口才返回
  notice?: string;
  hot?: number;         // 热门榜有效订单数
}

export interface ViewTicketsBackendResponse extends BackendResponse {
  arr?: BackendTicket[];
  num?: number;
}

// ── Orders ────────────────────────────────────────────────────────────────────

export interface BackendOrder {
  reservation_id: string;
  tk_id: string;
  title?: string;
  addr?: string;
  num: string;
  status: string;
  use_date?: string;
  created_at?: string;
  category?: string;
  seat_label?: string;
  seat_tier?: string;
  seat_price?: number;
  order_no?: string;
  expire_at?: string;
  ticket_price?: number;
}

export interface Order {
  id: number;
  ticket_id: number;
  title: string;
  venue: string;
  quantity: number;
  status: 'PENDING' | 'CONFIRMED' | 'CANCELLED' | 'EXPIRED';
  event_date: string;
  created_at: string;
  category: string;
  seat_label: string;
  seat_tier: string;
  seat_price: number;
  order_no: string;
  expire_at: string;    // PENDING 支付截止时间
  ticket_price: number; // 票面单价
}

export interface ViewMyOrdersBackendResponse extends BackendResponse {
  arr?: BackendOrder[];
}

// ── Seats ─────────────────────────────────────────────────────────────────────

export interface Seat {
  id: number;
  label: string;   // e.g. "A1"
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

// ── User / Theme ──────────────────────────────────────────────────────────────

export interface User {
  tel: string;
  username: string;
  token: string;
}

export type Theme = 'light' | 'dark';

// ── Normalizers ───────────────────────────────────────────────────────────────

export function normalizeTicket(t: BackendTicket): Ticket {
  return {
    id: parseInt(t.tk_id),
    title: t.title,
    venue: t.addr,
    total_seats: parseInt(t.max),
    available_seats: parseInt(t.num),
    event_date: t.use_date,
    // 详情接口的票状态在 tk_status（status 为协议字段 OK/ERR）；列表接口仍是 status
    status: parseInt(t.tk_status ?? t.status),
    cover_image: t.cover_image,
    category: (t.category as TicketCategory) || 'concert',
    price: t.price ?? 0,
    city: t.city || '',
    artist: t.artist || '',
    description: t.description,
    notice: t.notice,
    hot: t.hot,
  };
}

export function normalizeOrder(o: BackendOrder): Order {
  const statusMap: Record<string, Order['status']> = {
    PENDING: 'PENDING', CONFIRMED: 'CONFIRMED',
    CANCELLED: 'CANCELLED', EXPIRED: 'EXPIRED',
    '0': 'CANCELLED', '1': 'CONFIRMED',
  };
  return {
    id: parseInt(o.reservation_id),
    ticket_id: parseInt(o.tk_id),
    title: o.title || `票务 #${o.tk_id}`,
    venue: o.addr || '',
    quantity: parseInt(o.num),
    status: statusMap[o.status] || 'PENDING',
    event_date: o.use_date || '',
    created_at: o.created_at || '',
    category: o.category || 'concert',
    seat_label: o.seat_label || '',
    seat_tier: o.seat_tier || '',
    seat_price: o.seat_price ?? 0,
    order_no: o.order_no || '',
    expire_at: o.expire_at || '',
    ticket_price: o.ticket_price ?? 0,
  };
}
