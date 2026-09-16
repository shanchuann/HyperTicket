import { useCallback, useEffect, useMemo, useState } from 'react';
import { Activity, AlertTriangle, Bell, Building2, CalendarDays, CheckCircle2, Copy, Database, Info, Layers3, LogIn, LogOut, Moon, Plus, RefreshCw, Search, Sun, Ticket, UserRoundX, Users, X } from 'lucide-react';
import { adminApi, type AdminSession, type CatalogPayload, type ReminderRow, type Stats, type TicketRow, type UserRow } from './api';
import catLogo from './assets/hyperticket-cat.png';

type Tab = 'overview' | 'catalog' | 'reminders' | 'users';
const errorText: Record<string, string> = { ADMIN_UNAUTHORIZED: '登录已过期，请重新登录', ADMIN_INVALID_CREDENTIALS: '管理员账号或密码错误', DB_UNAVAILABLE: '数据服务暂时不可用，请稍后重试', DB_QUERY: '读取数据失败，请稍后重试', DB_INSERT: '保存数据失败，请检查内容后重试', DB_UPDATE: '更新数据失败，请稍后重试', INVALID_INPUT: '提交内容有误，请检查表单', ALREADY_IN_STATE: '该用户已经处于目标状态', TICKET_NOT_FOUND: '票务不存在或已被删除', USER_NOT_FOUND: '用户不存在', PASSWORD_TOO_WEAK: '密码需包含大小写字母和数字', PASSWORD_SAME_AS_OLD: '新密码不能与默认密码相同', RATE_LIMITED: '操作过于频繁，请稍后重试' };
const explain = (error: unknown) => { const raw = error instanceof Error ? error.message : typeof error === 'string' ? error : ''; if (errorText[raw]) return errorText[raw]; if (/^[A-Z0-9_]+$/.test(raw) || !/[\u3400-\u9fff]/.test(raw)) return '操作失败，请稍后重试'; return raw; };

export default function App() {
  const [session, setSession] = useState<AdminSession | null>(null);
  const [tab, setTab] = useState<Tab>('overview');
  const [dark, setDark] = useState(localStorage.getItem('adminTheme') === 'dark');
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState('');
  const [notice, setNotice] = useState('');
  const [loginNotice, setLoginNotice] = useState('');
  const [stats, setStats] = useState<Stats>({ user_count: 0, ticket_count: 0, order_count: 0, today_orders: 0 });
  const [tickets, setTickets] = useState<TicketRow[]>([]);
  const [users, setUsers] = useState<UserRow[]>([]);
  const [catalog,setCatalog]=useState<CatalogPayload>({events:[],venues:[],halls:[],sessions:[],tiers:[]});
  const [reminders,setReminders]=useState<ReminderRow[]>([]);

  useEffect(() => { localStorage.setItem('adminTheme', dark ? 'dark' : 'light'); }, [dark]);
  const load = useCallback(async (target: Tab) => {
    if (!session) return;
    setBusy(true); setError('');
    try {
      if (target === 'overview') { const [nextStats, nextTickets] = await Promise.all([adminApi.stats(session.token), adminApi.tickets(session.token)]); setStats(nextStats); setTickets(nextTickets); }
      if (target === 'catalog') setCatalog(await adminApi.catalog(session.token));
      if (target === 'reminders') setReminders(await adminApi.reminders(session.token));
      if (target === 'users') setUsers(await adminApi.users(session.token));
    } catch (e) { setError(explain(e)); } finally { setBusy(false); }
  }, [session]);
  useEffect(() => { if (session && !session.mustChangePassword) void load(tab); }, [session, tab, load]);

  if (!session) return <Login dark={dark} setDark={setDark} notice={loginNotice} clearNotice={() => setLoginNotice('')} onLogin={setSession}/>;
  if (session.mustChangePassword) return <PasswordReset dark={dark} session={session} onDone={() => { setSession(null); setLoginNotice('密码修改成功，请使用新密码重新登录'); }}/>;

  const nav = [{ id: 'overview' as const, label: '运营概览', icon: Activity }, { id: 'catalog' as const, label: '活动目录', icon: Layers3 }, { id: 'reminders' as const, label: '开售提醒', icon: Bell }, { id: 'users' as const, label: '用户管理', icon: Users }];
  return <main className={`admin-shell ${dark ? 'dark' : ''}`}>
    <aside><div className="brand"><img className="brand-mark small" src={catLogo} alt=""/><span><b>HyperTicket</b><small>运营控制台</small></span></div><p className="nav-label">工作区</p><nav>{nav.map(({id,label,icon:Icon}) => <button key={id} className={tab === id ? 'active' : ''} onClick={() => setTab(id)}><Icon size={18}/>{label}</button>)}</nav><div className="account"><img className="avatar" src={catLogo} alt={`${session.username} 管理员头像`}/><div><b>{session.username}</b><small>{session.role}</small></div><button aria-label="退出登录" onClick={() => setSession(null)}><LogOut size={17}/></button></div></aside>
    <section className="workspace"><header><div><h1>{nav.find(n => n.id === tab)?.label}</h1><p className="muted">保持库存、订单和用户状态准确。</p></div><div className="header-actions"><span className="online"><CheckCircle2 size={15}/>服务在线</span><button aria-label="切换主题" onClick={() => setDark(!dark)}>{dark ? <Sun size={18}/> : <Moon size={18}/>}</button></div></header>
      <div className="content">{error && <ToastNotice kind="error" text={error} onClose={() => setError('')}/>} {notice && <ToastNotice kind="success" text={notice} onClose={() => setNotice('')}/>} {busy && <div className="progress"/>}
        {tab === 'overview' && (
          <Overview stats={stats} tickets={tickets} onRefresh={() => load('overview')}/>
        )}
        {tab === 'catalog' && (
          <CatalogPage token={session.token} catalog={catalog} onRefresh={() => load('catalog')} notify={setNotice} fail={e => setError(explain(e))}/>
        )}
        {tab === 'reminders' && (
          <RemindersPage rows={reminders} onRefresh={()=>load('reminders')}/>
        )}
        {tab === 'users' && (
          <UsersPage token={session.token} users={users} onRefresh={() => load('users')} notify={setNotice} fail={e => setError(explain(e))}/>
        )}
      </div>
    </section>
  </main>;
}

function Login({dark,setDark,notice,clearNotice,onLogin}:{dark:boolean;setDark:(v:boolean)=>void;notice:string;clearNotice:()=>void;onLogin:(s:AdminSession)=>void}) {
  const [busy,setBusy]=useState(false);
  const [error,setError]=useState('');
  const submit=async(e:React.FormEvent<HTMLFormElement>)=>{e.preventDefault();setBusy(true);setError('');clearNotice();const data=new FormData(e.currentTarget);try{onLogin(await adminApi.login(String(data.get('username')),String(data.get('password'))));}catch(err){setError(explain(err));}finally{setBusy(false);}};
  return <main className={`admin-login ${dark?'dark':''}`}>
    {error && (
      <ToastNotice kind="error" text={error} onClose={() => setError('')}/>
    )}
    {notice && (
      <ToastNotice kind="success" text={notice} onClose={clearNotice}/>
    )}
    <section className="login-panel"><img className="brand-mark" src={catLogo} alt=""/><p className="product-name">HyperTicket 运营控制台</p><h1>欢迎回来</h1><p className="muted">登录后管理票务库存与用户状态。</p><form onSubmit={submit}><label>管理员账号<input name="username" autoComplete="username" required placeholder="输入账号"/></label><label>密码<input name="password" type="password" autoComplete="current-password" required placeholder="输入密码"/></label><button className="primary" disabled={busy}><LogIn size={17}/>{busy?'验证中…':'进入控制台'}</button></form><button className="theme-link" onClick={()=>setDark(!dark)}>{dark?<Sun size={16}/>:<Moon size={16}/>}切换主题</button></section>
  </main>;
}

function PasswordReset({dark,session,onDone}:{dark:boolean;session:AdminSession;onDone:()=>void}) {
  const [error,setError]=useState('');
  const [busy,setBusy]=useState(false);
  const submit=async(e:React.FormEvent<HTMLFormElement>)=>{e.preventDefault();const data=new FormData(e.currentTarget);const a=String(data.get('password')),b=String(data.get('confirm'));if(a!==b){setError('两次输入的密码不一致');return;}setBusy(true);try{await adminApi.changePassword(session.token,a);onDone();}catch(err){setError(explain(err));}finally{setBusy(false);}};
  return <main className={`admin-login ${dark?'dark':''}`}>{error&&<ToastNotice kind="error" text={error} onClose={()=>setError('')}/>}<section className="login-panel"><img className="brand-mark" src={catLogo} alt=""/><h1>设置安全密码</h1><p className="muted">首次登录需要修改默认密码，需包含大小写字母和数字。保存后请使用新密码重新登录。</p><form onSubmit={submit}><label>新密码<input name="password" type="password" autoComplete="new-password" minLength={6} maxLength={16} required/></label><label>确认密码<input name="confirm" type="password" autoComplete="new-password" minLength={6} maxLength={16} required/></label><button className="primary" disabled={busy}>{busy?'保存中…':'保存并重新登录'}</button></form></section></main>;
}

function ToastNotice({kind,text,onClose}:{kind:'error'|'success'|'info';text:string;onClose:()=>void}) { useEffect(()=>{const timer=window.setTimeout(onClose,4500);return()=>window.clearTimeout(timer);},[text,onClose]); const Icon=kind==='success'?CheckCircle2:kind==='error'?AlertTriangle:Info; return <div className={`toast-notice ${kind}`} role={kind==='error'?'alert':'status'}><Icon size={20}/><div><b>{kind==='success'?'操作成功':kind==='error'?'需要处理':'提示'}</b><span>{text}</span></div><button onClick={onClose} aria-label="关闭通知"><X size={17}/></button><i/></div>; }
function Overview({stats,tickets,onRefresh}:{stats:Stats;tickets:TicketRow[];onRefresh:()=>void}) { return <><PageHead title="今天的运营状态" detail="核心数据与最近票务动态" action={<button className="secondary" onClick={onRefresh}><RefreshCw size={15}/>刷新数据</button>}/><div className="stat-grid"><Metric icon={Ticket} label="在售票务" value={stats.ticket_count}/><Metric icon={Users} label="注册用户" value={stats.user_count}/><Metric icon={Database} label="累计订单" value={stats.order_count}/><Metric icon={Activity} label="今日订单" value={stats.today_orders}/></div><TicketTable rows={tickets.slice(0,6)}/></>; }
function Metric({label,value,icon:Icon}:{label:string;value:number;icon:typeof Activity}) { return <div className="stat"><Icon size={18}/><span>{label}</span><strong>{value.toLocaleString()}</strong></div>; }
function PageHead({title,detail,action}:{title:string;detail:string;action?:React.ReactNode}) { return <div className="page-tools"><div><h2>{title}</h2><p className="muted">{detail}</p></div>{action}</div>; }

function TicketsPage({token,tickets,onRefresh,notify,fail}:{token:string;tickets:TicketRow[];onRefresh:()=>void;notify:(s:string)=>void;fail:(e:unknown)=>void}) { const [query,setQuery]=useState(''); const [adding,setAdding]=useState(false); const rows=useMemo(()=>tickets.filter(t=>`${t.title}${t.venue}${t.city}`.toLowerCase().includes(query.toLowerCase())),[tickets,query]); const offline=async(t:TicketRow)=>{if(!confirm(`确认下架“${t.title}”？`))return;try{await adminApi.offlineTicket(token,t.ticket_id);notify('票务已下架，缓存库存已同步失效');onRefresh();}catch(e){fail(e);}}; return <><PageHead title="票务与库存" detail="维护场次信息，观察实时可售库存" action={<div className="toolbar"><SearchBox value={query} setValue={setQuery} placeholder="搜索名称、场馆或城市"/><button className="primary compact" onClick={()=>setAdding(!adding)}><Plus size={16}/>新增票务</button></div>}/>{adding&&<AddTicket token={token} onCancel={()=>setAdding(false)} onDone={()=>{setAdding(false);notify('票务已创建并完成库存预热');onRefresh();}} fail={fail}/>}<TicketTable rows={rows} onOffline={offline}/></>; }
function TicketTable({rows,onOffline}:{rows:TicketRow[];onOffline?:(t:TicketRow)=>void}) { return <section className="data-section"><div className="section-head"><h2><CalendarDays size={18}/>票务列表</h2><span className="muted">{rows.length} 条</span></div>{rows.length===0?<p className="empty">暂无匹配票务，请调整搜索条件。</p>:<div className="table-wrap"><table><thead><tr><th>票务</th><th>日期</th><th>库存</th><th>状态</th>{onOffline&&<th>操作</th>}</tr></thead><tbody>{rows.map(t=><tr key={t.ticket_id}><td><b>{t.title}</b><small>{t.city} · {t.venue}</small></td><td>{t.event_date}</td><td><b>{t.available_seats}</b> / {t.total_seats}</td><td><Status ok={t.status===1} yes="在售" no="已下架"/></td>{onOffline&&<td><button className="text-danger" disabled={t.status!==1} onClick={()=>onOffline(t)}>下架</button></td>}</tr>)}</tbody></table></div>}</section>; }

function AddTicket({token,onCancel,onDone,fail}:{token:string;onCancel:()=>void;onDone:()=>void;fail:(e:unknown)=>void}) { const [busy,setBusy]=useState(false); const submit=async(e:React.FormEvent<HTMLFormElement>)=>{e.preventDefault();const d=new FormData(e.currentTarget);setBusy(true);try{await adminApi.addTicket(token,{title:String(d.get('title')),venue:String(d.get('venue')),event_date:String(d.get('event_date')),total_seats:Number(d.get('total_seats')),category:String(d.get('category')),city:String(d.get('city')),price:Number(d.get('price')),artist:String(d.get('artist')),cover_image:String(d.get('cover_image')),description:String(d.get('description')),notice:String(d.get('notice'))});onDone();}catch(err){fail(err);}finally{setBusy(false);}}; return <form className="inline-form" onSubmit={submit}><div className="form-head"><div><h3>新增票务</h3><p className="muted">创建后将同步初始化 MySQL 与 Redis 库存。</p></div><button type="button" className="icon-button" onClick={onCancel}><X size={18}/></button></div><div className="form-grid"><label>票务名称<input name="title" required/></label><label>艺人/主办方<input name="artist"/></label><label>城市<input name="city" required defaultValue="北京"/></label><label>场馆<input name="venue" required/></label><label>活动日期<input name="event_date" type="date" required/></label><label>类别<select name="category" defaultValue="concert"><option value="concert">演唱会</option><option value="sports">体育</option><option value="esports">电竞</option><option value="theater">话剧歌剧</option><option value="exhibition">展览</option><option value="movie">电影</option></select></label><label>总座位数<input name="total_seats" type="number" min="1" required/></label><label>基础票价<input name="price" type="number" min="0" required/></label><label>封面图片地址<input name="cover_image" type="url" placeholder="https://…"/></label><label>活动简介<input name="description"/></label><label>购票须知<input name="notice"/></label></div><div className="form-actions"><button type="button" className="secondary" onClick={onCancel}>取消</button><button className="primary compact" disabled={busy}>{busy?'创建中…':'确认创建'}</button></div></form>; }

type CatalogKind='events'|'venues'|'halls'|'sessions'|'tiers';
const cell=(row:Record<string,string>,index:number)=>row[String(index)]||'';
function CatalogPage({token,catalog,onRefresh,notify,fail}:{token:string;catalog:CatalogPayload;onRefresh:()=>void;notify:(s:string)=>void;fail:(e:unknown)=>void}){
  const [kind,setKind]=useState<CatalogKind>('events');const [adding,setAdding]=useState(false);
  const labels:Record<CatalogKind,string>={events:'活动',venues:'场馆',halls:'厅馆',sessions:'场次',tiers:'票档'};
  const submit=async(e:React.FormEvent<HTMLFormElement>)=>{e.preventDefault();const data=Object.fromEntries(new FormData(e.currentTarget).entries());try{await adminApi.mutateCatalog(token,data);notify(`${labels[kind]}已保存`);setAdding(false);onRefresh();}catch(err){fail(err);}};
  return <><PageHead title="活动目录" detail="按活动、场馆、厅馆、场次和票档维护完整售票模型" action={<button className="primary compact" onClick={()=>setAdding(v=>!v)}><Plus size={16}/>新增{labels[kind]}</button>}/><div className="catalog-tabs">{(Object.keys(labels) as CatalogKind[]).map(k=><button className={kind===k?'active':''} onClick={()=>{setKind(k);setAdding(false)}} key={k}>{labels[k]} <small>{catalog[k].length}</small></button>)}</div>{adding&&<CatalogForm kind={kind} catalog={catalog} onSubmit={submit} onCancel={()=>setAdding(false)}/>}<CatalogTable kind={kind} catalog={catalog} token={token} onRefresh={onRefresh} notify={notify} fail={fail}/></>;
}
function CatalogForm({kind,catalog,onSubmit,onCancel}:{kind:CatalogKind;catalog:CatalogPayload;onSubmit:(e:React.FormEvent<HTMLFormElement>)=>void;onCancel:()=>void}){
 const field=(label:string,name:string,type='text',required=true)=><label>{label}<input name={name} type={type} required={required}/></label>;
 return <form className="inline-form" onSubmit={onSubmit}><input type="hidden" name="action" value={`${kind.slice(0,-1)}_${kind==='sessions'?'create':'upsert'}`}/><div className="form-head"><h3>新增{({events:'活动',venues:'场馆',halls:'厅馆',sessions:'场次',tiers:'票档'} as const)[kind]}</h3><button type="button" className="icon-button" onClick={onCancel}><X size={18}/></button></div><div className="form-grid">
 {kind==='events'&&<>{field('活动名称','title')}<label>类别<select name="category"><option value="movie">电影</option><option value="concert">演唱会</option><option value="performance">演出</option><option value="comedy">脱口秀</option><option value="exhibition">展览</option><option value="esports">电竞赛事</option><option value="sports">体育赛事</option></select></label>{field('城市','city')}{field('艺人/团体','artist','text',false)}{field('副标题','subtitle','text',false)}{field('主办方','organizer','text',false)}{field('本地封面路径','cover_path','text',false)}<label>首页板块<select name="home_section"><option value="RECOMMENDED">为你推荐</option><option value="MUST_SEE">近期必看</option><option value="COMING_SOON">即将开售</option><option value="NOW_SHOWING">正在上映</option><option value="HOT_SPORTS">热门赛事</option><option value="CITY_PICKS">城市精选</option></select></label>{field('排序权重','ranking_weight','number')}<label>实名制<select name="real_name_required"><option value="0">否</option><option value="1">是</option></select></label>{field('活动简介','description','text',false)}{field('购票须知','notice','text',false)}<input type="hidden" name="event_status" value="PUBLISHED"/></>}
 {kind==='venues'&&<>{field('场馆名称','name')}<label>类型<select name="venue_type"><option value="VENUE">场馆</option><option value="CINEMA">影院</option></select></label>{field('城市','city')}{field('详细地址','address')}{field('经度','longitude','number')}{field('纬度','latitude','number')}</>}
 {kind==='halls'&&<>{field('场馆 ID','venue_id','number')}{field('厅馆名称','name')}{field('厅馆规格','hall_format')}<label>座位模式<select name="seat_mode"><option value="RESERVED">选座</option><option value="GENERAL">不选座</option><option value="MIXED">混合</option></select></label>{field('容量','capacity','number')}</>}
 {kind==='sessions'&&<>{field('活动 ID','event_id','number')}{field('场馆 ID','venue_id','number')}{field('厅馆 ID','hall_id','number')}{field('场次名称','name')}{field('开售时间','sale_start_at','datetime-local')}{field('售票截止','sale_end_at','datetime-local')}{field('活动开始','starts_at','datetime-local')}{field('活动结束','ends_at','datetime-local',false)}{field('容量','capacity','number')}{field('基础票价（元）','base_price','number')}<input type="hidden" name="seat_mode" value="RESERVED"/></>}
 {kind==='tiers'&&<>{field('场次 ID','session_id','number')}{field('票档名称','name')}{field('价格（分）','price_minor','number')}{field('库存','inventory','number')}{field('可售库存','available_inventory','number')}{field('限购','purchase_limit','number')}<label>座位模式<select name="seat_mode"><option value="RESERVED">选座</option><option value="GENERAL">不选座</option></select></label>{field('排序','sort_order','number')}<input type="hidden" name="currency" value="CNY"/></>}
 </div><div className="form-actions"><button type="button" className="secondary" onClick={onCancel}>取消</button><button className="primary compact">保存</button></div></form>;
}
function CatalogTable({kind,catalog,token,onRefresh,notify,fail}:{kind:CatalogKind;catalog:CatalogPayload;token:string;onRefresh:()=>void;notify:(s:string)=>void;fail:(e:unknown)=>void}){
 const heads:Record<CatalogKind,string[]>={events:['ID / 活动','分类','城市','场次','最低价'],venues:['ID / 场馆','类型','城市','地址','坐标'],halls:['ID / 厅馆','场馆 ID','规格','座位模式','容量'],sessions:['ID / 场次','活动 ID','场馆 / 厅馆','售票窗口','开始时间'],tiers:['ID / 票档','场次 ID','价格','库存','模式']};
 const rows=kind==='events'?catalog.events:catalog[kind];
 const copy=async(id:string)=>{const start=new Date(Date.now()+14*864e5).toISOString().slice(0,16);try{await adminApi.mutateCatalog(token,{action:'copy_session',source_session_id:Number(id),sale_start_at:new Date().toISOString().slice(0,16),sale_end_at:new Date(Date.now()+13*864e5).toISOString().slice(0,16),starts_at:start});notify('场次配置已复制');onRefresh();}catch(e){fail(e)}};
 return <section className="data-section"><div className="table-wrap"><table><thead><tr>{heads[kind].map(x=><th key={x}>{x}</th>)}{kind==='sessions'&&<th>操作</th>}</tr></thead><tbody>{rows.map((r:any,i)=>kind==='events'?<tr key={r.event_id}><td><b>{r.event_id} · {r.title}</b><small>{r.artist||r.subtitle}</small></td><td>{r.category}</td><td>{r.city}</td><td>{r.session_count}</td><td>¥{Math.round(Number(r.min_price_minor)/100)}</td></tr>:<tr key={cell(r,String(i) as any)||i}>{kind==='venues'&&<><td><b>{cell(r,0)} · {cell(r,1)}</b></td><td>{cell(r,2)}</td><td>{cell(r,3)}</td><td>{cell(r,4)}</td><td>{cell(r,5)}, {cell(r,6)}</td></>}{kind==='halls'&&<><td><b>{cell(r,0)} · {cell(r,2)}</b></td><td>{cell(r,1)}</td><td>{cell(r,3)}</td><td>{cell(r,4)}</td><td>{cell(r,5)}</td></>}{kind==='sessions'&&<><td><b>{cell(r,0)} · {cell(r,5)}</b></td><td>{cell(r,1)}</td><td>{cell(r,2)} / {cell(r,3)}</td><td>{cell(r,6)}<small>至 {cell(r,7)}</small></td><td>{cell(r,8)}</td><td><button className="text-action" onClick={()=>copy(cell(r,0))}><Copy size={14}/>复制</button></td></>}{kind==='tiers'&&<><td><b>{cell(r,0)} · {cell(r,2)}</b></td><td>{cell(r,1)}</td><td>¥{(Number(cell(r,3))/100).toFixed(2)}</td><td>{cell(r,6)} / {cell(r,5)}</td><td>{cell(r,8)}</td></>}</tr>)}</tbody></table></div></section>;
}
function RemindersPage({rows,onRefresh}:{rows:ReminderRow[];onRefresh:()=>void}){return <><PageHead title="开售提醒" detail="邮件发送、重试与失败原因审计" action={<button className="secondary" onClick={onRefresh}><RefreshCw size={15}/>刷新</button>}/><section className="data-section"><div className="table-wrap"><table><thead><tr><th>活动</th><th>接收邮箱</th><th>提醒时间</th><th>状态</th><th>重试</th><th>错误</th></tr></thead><tbody>{rows.map(r=><tr key={r.reminder_id}><td><b>{r.title}</b><small>#{r.reminder_id}</small></td><td>{r.email}</td><td>{r.remind_at}</td><td><span className={`status ${r.status==='SENT'?'ok':r.status==='FAILED'?'blocked':''}`}>{r.status}</span></td><td>{r.attempt_count}</td><td>{r.last_error||'—'}</td></tr>)}</tbody></table></div></section></>}

function UsersPage({token,users,onRefresh,notify,fail}:{token:string;users:UserRow[];onRefresh:()=>void;notify:(s:string)=>void;fail:(e:unknown)=>void}) { const [query,setQuery]=useState(''); const rows=useMemo(()=>users.filter(u=>`${u.username}${u.tel}`.toLowerCase().includes(query.toLowerCase())),[users,query]); const toggle=async(u:UserRow)=>{const block=u.status===1;if(!confirm(`确认${block?'封禁':'解封'}用户“${u.username}”？`))return;try{await adminApi.setBlacklist(token,u.tel,block);notify(`用户已${block?'封禁':'解封'}`);onRefresh();}catch(e){fail(e);}}; return <><PageHead title="用户与访问状态" detail="按账号或手机号查找，并管理黑名单" action={<SearchBox value={query} setValue={setQuery} placeholder="搜索用户名或手机号"/>}/><section className="data-section"><div className="section-head"><h2><Users size={18}/>用户列表</h2><span className="muted">{rows.length} 人</span></div>{rows.length===0?<p className="empty">没有找到符合条件的用户。</p>:<div className="table-wrap"><table><thead><tr><th>用户</th><th>手机号</th><th>状态</th><th>操作</th></tr></thead><tbody>{rows.map(u=><tr key={u.user_id}><td><b>{u.username}</b><small>用户 ID {u.user_id}</small></td><td>{u.tel.slice(0,3)}****{u.tel.slice(-4)}</td><td><Status ok={u.status===1} yes="正常" no="已封禁"/></td><td><button className={u.status===1?'text-danger':'text-action'} onClick={()=>toggle(u)}><UserRoundX size={15}/>{u.status===1?'封禁':'解封'}</button></td></tr>)}</tbody></table></div>}</section></>; }
function SearchBox({value,setValue,placeholder}:{value:string;setValue:(s:string)=>void;placeholder:string}) { return <label className="search-box"><Search size={16}/><span className="sr-only">搜索</span><input value={value} onChange={e=>setValue(e.target.value)} placeholder={placeholder}/>{value&&<button type="button" onClick={()=>setValue('')} aria-label="清除搜索"><X size={14}/></button>}</label>; }
function Status({ok,yes,no}:{ok:boolean;yes:string;no:string}) { return <span className={`status ${ok?'ok':'blocked'}`}>{ok?<CheckCircle2 size={14}/>:<UserRoundX size={14}/>} {ok?yes:no}</span>; }
