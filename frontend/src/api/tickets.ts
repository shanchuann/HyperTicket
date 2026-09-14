import wsClient from './client';
import type { ViewTicketsBackendResponse, BackendTicket, BackendResponse, Ticket } from '../types';
import { normalizeTicket } from '../types';

export interface TicketFilters {
  keyword?: string;
  city?: string;
  category?: string;
}

type DetailResponse = BackendResponse & BackendTicket;

export const ticketApi = {
  async getTickets(filters: TicketFilters = {}): Promise<Ticket[]> {
    const req: Record<string, unknown> = { type: 4 };
    if (filters.keyword) req.keyword = filters.keyword;
    if (filters.city) req.city = filters.city;
    if (filters.category) req.category = filters.category;
    const resp = await wsClient.send<ViewTicketsBackendResponse>(req as { type: number });
    return (resp.arr || []).map(normalizeTicket);
  },

  async getDetail(ticketId: number): Promise<Ticket> {
    const resp = await wsClient.send<DetailResponse>({ type: 19, index: String(ticketId) });
    return normalizeTicket(resp);
  },

  async getHotTickets(limit = 10): Promise<Ticket[]> {
    const resp = await wsClient.send<ViewTicketsBackendResponse>({ type: 23, limit });
    return (resp.arr || []).map(normalizeTicket);
  },

  async favorite(token: string, ticketId: number, action: 'add' | 'remove'): Promise<BackendResponse> {
    return wsClient.send<BackendResponse>({ type: 21, token, index: String(ticketId), action });
  },

  async getFavorites(token: string): Promise<Ticket[]> {
    const resp = await wsClient.send<ViewTicketsBackendResponse>({ type: 22, token });
    return (resp.arr || []).map(normalizeTicket);
  },
};
