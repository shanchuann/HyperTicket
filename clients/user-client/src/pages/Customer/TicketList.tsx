import { useEffect, useMemo, useState } from 'react';
import { Bell, CalendarDays, ChevronRight, Heart, LocateFixed, MapPin, Search, X } from 'lucide-react';
import { catalogApi, type CatalogEvent, type EventDetail, type EventSession, type HomeSection } from '../../api/catalog';
import type { Ticket } from '../../types';
import CustomSelect from '../../components/CustomSelect';
import SeatPicker from '../../components/SeatPicker';
import Toast from '../../components/Toast';
import './TicketList.css';

const categories=[['','全部类型'],['movie','电影'],['concert','演唱会'],['performance','演出'],['comedy','脱口秀'],['exhibition','展览'],['esports','电竞赛事'],['sports','体育赛事']];
const sections:{id:HomeSection;title:string;detail:string}[]=[
  {id:'RECOMMENDED',title:'为你推荐',detail:'根据收藏、浏览与购票偏好整理'},
  {id:'MUST_SEE',title:'近期必看',detail:'热度与口碑都在上升'},
  {id:'COMING_SOON',title:'即将开售',detail:'先订提醒，开售邮件会准时送达'},
  {id:'NOW_SHOWING',title:'正在热映与演出',detail:'今天就能出发的选择'},
  {id:'HOT_SPORTS',title:'热门电竞与体育',detail:'把关键一局留给现场'},
  {id:'CITY_PICKS',title:'城市精选',detail:'在熟悉的城市发现新现场'},
];
const labels=Object.fromEntries(categories);
const formatDate=(value:string)=>new Date(value.replace(' ','T')).toLocaleString('zh-CN',{month:'short',day:'numeric',weekday:'short',hour:'2-digit',minute:'2-digit'});

export default function TicketList(){
  const [events,setEvents]=useState<CatalogEvent[]>([]),[loading,setLoading]=useState(true),[error,setError]=useState('');
  const [query,setQuery]=useState(''),[category,setCategory]=useState(''),[city,setCity]=useState('');
  const [detail,setDetail]=useState<EventDetail|null>(null),[selectedSession,setSelectedSession]=useState<EventSession|null>(null);
  const [toast,setToast]=useState(''); const [location,setLocation]=useState<{lat:number;lng:number}|null>(null);
  const load=()=>{setLoading(true);setError('');catalogApi.home().then(setEvents).catch(e=>setError(e instanceof Error?e.message:'目录加载失败')).finally(()=>setLoading(false));};
  useEffect(load,[]);
  const cities=useMemo(()=>[...new Set(events.map(e=>e.city))], [events]);
  const distance=(e:CatalogEvent)=>{if(!location)return null;const p=Math.PI/180,a=0.5-Math.cos((e.latitude-location.lat)*p)/2+Math.cos(location.lat*p)*Math.cos(e.latitude*p)*(1-Math.cos((e.longitude-location.lng)*p))/2;return 12742*Math.asin(Math.sqrt(a));};
  const filtered=useMemo(()=>events.filter(e=>(!category||e.category===category)&&(!city||e.city===city)&&`${e.title}${e.artist}${e.venue_name}`.toLowerCase().includes(query.toLowerCase())).sort((a,b)=>location?(distance(a)??9999)-(distance(b)??9999):b.ranking_weight-a.ranking_weight),[events,category,city,query,location]);
  const open=async(e:CatalogEvent)=>{try{setDetail(await catalogApi.detail(e.event_id));}catch(err){setError(err instanceof Error?err.message:'活动详情加载失败');}};
  const toggleFavorite=async(e:CatalogEvent)=>{const next=!e.favorite;setEvents(rows=>rows.map(x=>x.event_id===e.event_id?{...x,favorite:next}:x));try{await catalogApi.favorite(e.event_id,next);}catch{setEvents(rows=>rows.map(x=>x.event_id===e.event_id?{...x,favorite:!next}:x));}};
  const remind=async(e:CatalogEvent)=>{try{await catalogApi.addReminder(e.event_id);setToast('开售提醒已设置，开售时将发送邮件');}catch(err){setError(err instanceof Error?err.message:'请先在个人中心验证邮箱');}};
  const locate=()=>navigator.geolocation?.getCurrentPosition(p=>setLocation({lat:p.coords.latitude,lng:p.coords.longitude}),()=>setToast('未获得定位权限，可继续按城市筛选'));
  const ticketFrom=(d:EventDetail,s:EventSession):Ticket=>({id:s.ticket_id,title:d.title,venue:`${s.venue_name} · ${s.hall_name}`,total_seats:s.tiers.reduce((n,t)=>n+t.inventory,0),available_seats:s.available_inventory,event_date:s.starts_at.slice(0,10),status:1,category:d.category,price:Math.round(Math.min(...s.tiers.map(t=>t.price_minor))/100),city:s.city,artist:d.artist,description:d.description,notice:d.notice});
  return <div className="catalog-page">
    <Toast message={error||toast} tone={error?'error':'success'} onClose={()=>{setError('');setToast('');}} actionLabel={error?'重试':undefined} onAction={error?load:undefined}/>
    {selectedSession&&detail&&<SeatPicker ticket={ticketFrom(detail,selectedSession)} quantity={1} onClose={()=>setSelectedSession(null)} onSuccess={m=>{setSelectedSession(null);setDetail(null);setToast(m);load();}} onError={setError}/>}
    <header className="catalog-head"><div><span>HyperTicket 城市票务</span><h1>今天想去哪里？</h1></div><p className="catalog-count"><strong>{filtered.length}</strong><span>场可预订</span></p></header>
    <div className="catalog-toolbar"><label className="catalog-search"><Search size={18}/><input value={query} onChange={e=>setQuery(e.target.value)} placeholder="搜索活动、艺人或场馆"/></label><CustomSelect label="活动类型" value={category} options={categories.map(([value,label])=>({value,label}))} onChange={setCategory}/><CustomSelect label="举办地点" value={city} options={[{value:'',label:'全部城市'},...cities.map(x=>({value:x,label:x}))]} onChange={setCity}/><button className="locate-button" onClick={locate} title="按距离排序"><LocateFixed size={18}/></button></div>
    {loading?<div className="catalog-loading">正在整理本地票务目录...</div>:sections.map((section,index)=>{const rows=filtered.filter(e=>e.home_section===section.id);if(!rows.length)return null;return <section className={`catalog-section section-${index%3}`} key={section.id}><header><div><h2>{section.title}</h2><p>{section.detail}</p></div><span>{rows.length} 项</span></header><div className={index%3===0?'feature-strip':index%3===1?'editorial-grid':'compact-rail'}>{rows.map((e,i)=><article className={`event-tile ${i===0&&index%3===0?'featured':''}`} key={e.event_id} onClick={()=>open(e)}><img src={e.cover_path} alt=""/><div className="event-copy"><span>{labels[e.category]} · {e.city}</span><h3>{e.title}</h3><p>{e.artist||e.subtitle}</p><div><CalendarDays size={14}/>{formatDate(e.next_session_at)}<MapPin size={14}/>{e.venue_name}</div><footer><b>¥{Math.round(e.min_price_minor/100)} 起</b>{distance(e)!=null&&<small>{distance(e)!.toFixed(1)} km</small>}<button onClick={x=>{x.stopPropagation();void toggleFavorite(e)}} aria-label={e.favorite?'取消收藏':'收藏'}><Heart size={17} fill={e.favorite?'currentColor':'none'}/></button>{section.id==='COMING_SOON'&&<button onClick={x=>{x.stopPropagation();void remind(e)}} aria-label="开售提醒"><Bell size={17}/></button>}<ChevronRight size={18}/></footer></div></article>)}</div></section>})}
    {!loading&&!filtered.length&&<div className="catalog-empty"><p>没有符合条件的活动</p><button onClick={()=>{setQuery('');setCategory('');setCity('');}}>清除筛选</button></div>}
    {detail&&<div className="event-sheet-backdrop" onClick={()=>setDetail(null)}><aside className="event-sheet" onClick={e=>e.stopPropagation()}><button className="sheet-close" onClick={()=>setDetail(null)} aria-label="关闭"><X/></button><img className="sheet-cover" src={detail.cover_path} alt=""/><div className="sheet-body"><span>{labels[detail.category]} · {detail.city}</span><h2>{detail.title}</h2><p>{detail.description}</p><h3>选择场次</h3>{detail.sessions.map(s=><button className="session-row" key={s.session_id} disabled={s.available_inventory<=0||new Date(s.sale_start_at)>new Date()} onClick={()=>setSelectedSession(s)}><span><b>{formatDate(s.starts_at)}</b><small>{s.venue_name} · {s.hall_name} · {s.hall_format}</small></span><span><strong>{s.available_inventory}</strong> 张可售<ChevronRight size={16}/></span></button>)}<div className="sheet-notice"><h3>购票须知</h3><p>{detail.notice}</p></div><a className="amap-link" target="_blank" rel="noreferrer" href={`https://uri.amap.com/marker?position=${detail.sessions[0]?.longitude},${detail.sessions[0]?.latitude}&name=${encodeURIComponent(detail.sessions[0]?.venue_name||'场馆')}`}><MapPin size={16}/>使用高德地图导航</a></div></aside></div>}
  </div>;
}
