import { invoke } from '@tauri-apps/api/core';

export type Response = { status: 'OK' | 'ERR'; reason?: string; [key: string]: unknown };
export type AdminSession = { token: string; username: string; role: string; mustChangePassword: boolean };
export type TicketRow = { ticket_id: number; title: string; venue: string; event_date: string; total_seats: number; available_seats: number; status: number; category: string; city: string; price: number; artist: string; cover_image?: string; description?: string; notice?: string };
export type UserRow = { user_id: number; username: string; tel: string; status: number };
export type Stats = { user_count: number; ticket_count: number; order_count: number; today_orders: number };
export type CatalogPayload = { events:Array<Record<string,unknown>>; venues:Array<Record<string,string>>; halls:Array<Record<string,string>>; sessions:Array<Record<string,string>>; tiers:Array<Record<string,string>> };
export type ReminderRow = { reminder_id:number; title:string; email:string; remind_at:string; status:string; attempt_count:number; last_error:string; updated_at:string };

const send = async <T extends Response>(payload: object) => {
  const response = await invoke<T>('send_request', { payload });
  if (response.status !== 'OK') throw new Error(response.reason || '请求失败');
  return response;
};

export const adminApi = {
  async login(username: string, password: string): Promise<AdminSession> {
    const r = await send<Response>({ type: 8, username, passward: password });
    return { token: String(r.admin_token), username: String(r.username || username), role: String(r.role || 'admin'), mustChangePassword: Boolean(r.is_default_password) };
  },
  changePassword: (token: string, password: string) => send({ type: 15, admin_token: token, new_password: password }),
  async stats(token: string) { const r = await send<Response>({ type: 13, admin_token: token }); return { user_count: Number(r.user_count), ticket_count: Number(r.ticket_count), order_count: Number(r.order_count), today_orders: Number(r.today_orders) } as Stats; },
  async tickets(token: string) { const r = await send<Response>({ type: 9, admin_token: token }); return (r.arr || []) as TicketRow[]; },
  async users(token: string) { const r = await send<Response>({ type: 12, admin_token: token }); return (r.arr || []) as UserRow[]; },
  addTicket: (token: string, ticket: Omit<TicketRow, 'ticket_id' | 'available_seats' | 'status'>) => send({ type: 10, admin_token: token, ...ticket }),
  offlineTicket: (token: string, ticketId: number) => send({ type: 11, admin_token: token, ticket_id: ticketId }),
  setBlacklist: (token: string, tel: string, blocked: boolean) => send({ type: 14, admin_token: token, tel, action: blocked ? 'add' : 'remove' }),
  async catalog(token:string){const r=await send<Response&CatalogPayload>({type:44,admin_token:token});return r as CatalogPayload;},
  mutateCatalog:(token:string,payload:Record<string,unknown>)=>send({type:45,admin_token:token,...payload}),
  async reminders(token:string){const r=await send<Response&{reminders:ReminderRow[]}>({type:46,admin_token:token});return r.reminders||[];},
};
