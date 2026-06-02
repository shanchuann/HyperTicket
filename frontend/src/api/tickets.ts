import wsClient from './client';
import type { ViewTicketsBackendResponse, Ticket } from '../types';
import { normalizeTicket } from '../types';

export const ticketApi = {
  async getTickets(): Promise<Ticket[]> {
    const resp = await wsClient.send<ViewTicketsBackendResponse>({ type: 4 });
    return (resp.arr || []).map(normalizeTicket);
  },
};
