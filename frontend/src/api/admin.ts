import wsClient from './client';
import type { BackendResponse } from '../types';

const getAdminToken = () => localStorage.getItem('admin_token') || '';

export interface AdminTicket {
  ticket_id: number;
  title: string;
  venue: string;
  event_date: string;
  total_seats: number;
  available_seats: number;
  status: number;
}

export interface AdminUser {
  user_id: number;
  tel: string;
  username: string;
  status: number; // 1 正常 0 黑名单
}

export interface AdminStats {
  user_count: number;
  ticket_count: number;
  order_count: number;
  today_orders: number;
}

type AdminLoginResponse = BackendResponse & { admin_token: string; username: string; role: string };
type ListTicketsResponse = BackendResponse & { arr: AdminTicket[] };
type ListUsersResponse  = BackendResponse & { arr: AdminUser[] };
type StatsResponse      = BackendResponse & AdminStats;

export const adminApi = {
  async login(username: string, password: string): Promise<AdminLoginResponse> {
    return wsClient.send<AdminLoginResponse>({
      type: 8,
      username,
      passward: password, // 保持与后端历史拼写一致
    });
  },

  async listTickets(): Promise<AdminTicket[]> {
    const resp = await wsClient.send<ListTicketsResponse>({
      type: 9,
      admin_token: getAdminToken(),
    });
    return resp.arr || [];
  },

  async addTicket(title: string, venue: string, eventDate: string, totalSeats: number): Promise<void> {
    await wsClient.send<BackendResponse>({
      type: 10,
      admin_token: getAdminToken(),
      title,
      venue,
      event_date: eventDate,
      total_seats: totalSeats,
    });
  },

  async deleteTicket(ticketId: number): Promise<void> {
    await wsClient.send<BackendResponse>({
      type: 11,
      admin_token: getAdminToken(),
      ticket_id: ticketId,
    });
  },

  async listUsers(): Promise<AdminUser[]> {
    const resp = await wsClient.send<ListUsersResponse>({
      type: 12,
      admin_token: getAdminToken(),
    });
    return resp.arr || [];
  },

  async stats(): Promise<AdminStats> {
    const resp = await wsClient.send<StatsResponse>({
      type: 13,
      admin_token: getAdminToken(),
    });
    return {
      user_count:   resp.user_count   ?? 0,
      ticket_count: resp.ticket_count ?? 0,
      order_count:  resp.order_count  ?? 0,
      today_orders: resp.today_orders ?? 0,
    };
  },

  async blacklist(tel: string, action: 'add' | 'remove'): Promise<void> {
    await wsClient.send<BackendResponse>({
      type: 14,
      admin_token: getAdminToken(),
      tel,
      action,
    });
  },
};
