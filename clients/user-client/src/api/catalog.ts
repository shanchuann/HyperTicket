import client from './client';
import type { BackendResponse } from '../types';

export type Category = 'movie'|'concert'|'performance'|'comedy'|'exhibition'|'esports'|'sports';
export type HomeSection = 'RECOMMENDED'|'MUST_SEE'|'COMING_SOON'|'NOW_SHOWING'|'HOT_SPORTS'|'CITY_PICKS';
export type CatalogEvent = { event_id:number; title:string; subtitle:string; category:Category; city:string; artist:string; cover_path:string; home_section:HomeSection; ranking_weight:number; real_name_required:boolean; next_session_at:string; sale_start_at:string; min_price_minor:number; available_inventory:number; session_count:number; venue_name:string; longitude:number; latitude:number; favorite:boolean; popularity:number };
export type TicketTier = { tier_id:number; name:string; price_minor:number; currency:string; inventory:number; available_inventory:number; purchase_limit:number; seat_mode:'RESERVED'|'GENERAL' };
export type EventSession = { session_id:number; ticket_id:number; name:string; sale_start_at:string; sale_end_at:string; starts_at:string; ends_at:string; status:string; venue_id:number; venue_name:string; city:string; address:string; longitude:number; latitude:number; hall_id:number; hall_name:string; hall_format:string; available_inventory:number; tiers:TicketTier[] };
export type EventDetail = CatalogEvent & { organizer:string; description:string; notice:string; sessions:EventSession[] };
export type Profile = { user_id:number; username:string; tel:string; email:string; email_verified:boolean; phone_verified:boolean; display_name:string; avatar_path:string; gender:string; birthday:string; city:string; bio:string };
export type Attendee = { attendee_id:number; name:string; id_type:string; id_number_masked:string; phone_masked:string; is_default:boolean };
export type Reminder = { reminder_id:number; event_id:number; title:string; cover_path:string; remind_at:string; status:string; attempt_count:number; last_error:string };
export type HistoryItem = { event_id:number; title:string; category:string; cover_path:string; view_count:number; last_viewed_at:string };

const token = () => localStorage.getItem('token') || '';
export const catalogApi = {
  async home() { const r=await client.send<BackendResponse&{events:CatalogEvent[]}>({type:34,token:token()}); return r.events||[]; },
  async detail(eventId:number) { const r=await client.send<BackendResponse&{event:EventDetail}>({type:35,event_id:eventId,token:token()}); return r.event; },
  async favorite(eventId:number,active:boolean){return client.send({type:43,token:token(),event_id:eventId,action:active?'add':'remove'});},
  async profile(){const r=await client.send<BackendResponse&{profile:Profile}>({type:36,token:token()});return r.profile;},
  async updateProfile(profile:Partial<Profile>){const r=await client.send<BackendResponse&{profile:Profile}>({type:37,token:token(),...profile});return r.profile;},
  async attendees(){const r=await client.send<BackendResponse&{attendees:Attendee[]}>({type:38,token:token()});return r.attendees||[];},
  async addAttendee(value:{name:string;id_type:string;id_number:string;phone:string;is_default:boolean}){const r=await client.send<BackendResponse&{attendees:Attendee[]}>({type:39,token:token(),action:'add',...value});return r.attendees||[];},
  async deleteAttendee(id:number){const r=await client.send<BackendResponse&{attendees:Attendee[]}>({type:39,token:token(),action:'delete',attendee_id:id});return r.attendees||[];},
  async history(){const r=await client.send<BackendResponse&{history:HistoryItem[]}>({type:40,token:token()});return r.history||[];},
  async reminders(){const r=await client.send<BackendResponse&{reminders:Reminder[]}>({type:41,token:token()});return r.reminders||[];},
  async addReminder(eventId:number){const r=await client.send<BackendResponse&{reminders:Reminder[]}>({type:42,token:token(),action:'add',event_id:eventId});return r.reminders||[];},
  async cancelReminder(id:number){const r=await client.send<BackendResponse&{reminders:Reminder[]}>({type:42,token:token(),action:'cancel',reminder_id:id});return r.reminders||[];},
};
