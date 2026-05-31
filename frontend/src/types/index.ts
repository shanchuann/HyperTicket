// HyperTicket API 类型定义

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

// 基础请求接口
export interface BaseRequest {
  type: OperationType;
  token?: string;
}

// 基础响应接口
export interface BaseResponse {
  success: boolean;
  message: string;
  data?: unknown;
}

// 用户相关
export interface LoginRequest extends BaseRequest {
  type: 1;
  tel: string;
  password: string;
}

export interface RegisterRequest extends BaseRequest {
  type: 2;
  tel: string;
  username: string;
  password: string;
}

export interface AuthResponse extends BaseResponse {
  data?: {
    token: string;
    username: string;
    tel: string;
  };
}

// 票务相关
export interface Ticket {
  id: number;
  title: string;
  venue: string;
  total_seats: number;
  available_seats: number;
  event_date: string;
  status: number;
  price?: number;
  description?: string;
  category?: string;
}

export interface ViewTicketsRequest extends BaseRequest {
  type: 4;
}

export interface ViewTicketsResponse extends BaseResponse {
  data?: {
    tickets: Ticket[];
  };
}

// 订单相关
export interface Order {
  id: number;
  user_id: number;
  ticket_id: number;
  quantity: number;
  status: 'PENDING' | 'CONFIRMED' | 'CANCELLED' | 'EXPIRED';
  created_at: string;
  ticket?: Ticket;
}

export interface OrderRequest extends BaseRequest {
  type: 5;
  token: string;
  ticket_id: number;
  quantity: number;
}

export interface ViewMyOrdersRequest extends BaseRequest {
  type: 6;
  token: string;
}

export interface ViewMyOrdersResponse extends BaseResponse {
  data?: {
    orders: Order[];
  };
}

export interface CancelOrderRequest extends BaseRequest {
  type: 7;
  token: string;
  reservation_id: number;
}

// 用户信息
export interface User {
  tel: string;
  username: string;
  token?: string;
}

// 主题类型
export type Theme = 'light' | 'dark';
