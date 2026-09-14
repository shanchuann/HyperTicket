import { StrictMode, useState } from 'react';
import { createRoot } from 'react-dom/client';
import { Activity, CalendarDays, CheckCircle2, LogOut, Moon, Search, Sun, Ticket, Users } from 'lucide-react';
import { invoke } from '@tauri-apps/api/core';
import './styles.css';

type Resp = { status: 'OK' | 'ERR'; reason?: string; [key: string]: unknown };
const request = (payload: object) => invoke<Resp>('send_request', { payload });

function App() {
  const [token, setToken] = useState('');
  const [user, setUser] = useState('');
  const [tab, setTab] = useState<'overview' | 'tickets' | 'users'>('overview');
  const [dark, setDark] = useState(false);
  const [error, setError] = useState('');
  const [loginBusy, setLoginBusy] = useState(false);
  const [stats, setStats] = useState({ users: 0, tickets: 0, orders: 0, today: 0 });
  const [tickets, setTickets] = useState<Record<string, unknown>[]>([]);
  const [users, setUsers] = useState<Record<string, unknown>[]>([]);

  const load = async (next: typeof tab = tab) => {
    if (!token) return;
    setError('');
    try {
      if (next === 'overview') {
        const [s, t] = await Promise.all([request({ type: 13, admin_token: token }), request({ type: 9, admin_token: token })]);
        if (s.status !== 'OK' || t.status !== 'OK') throw new Error(s.reason || t.reason || '加载失败');
        setStats({ users: Number(s.user_count || 0), tickets: Number(s.ticket_count || 0), orders: Number(s.order_count || 0), today: Number(s.today_orders || 0) });
        setTickets((t.arr as Record<string, unknown>[]) || []);
      } else if (next === 'tickets') {
        const r = await request({ type: 9, admin_token: token }); if (r.status !== 'OK') throw new Error(r.reason || '加载票务失败'); setTickets((r.arr as Record<string, unknown>[]) || []);
      } else {
        const r = await request({ type: 12, admin_token: token }); if (r.status !== 'OK') throw new Error(r.reason || '加载用户失败'); setUsers((r.arr as Record<string, unknown>[]) || []);
      }
    } catch (e) { setError(e instanceof Error ? e.message : '请求失败'); }
  };

  const login = async (e: React.FormEvent<HTMLFormElement>) => {
    e.preventDefault(); setLoginBusy(true); setError(''); const data = new FormData(e.currentTarget);
    try { const r = await request({ type: 8, username: data.get('username'), passward: data.get('password') }); if (r.status !== 'OK') throw new Error('账号或密码错误'); setToken(String(r.admin_token || '')); setUser(String(r.username || data.get('username'))); }
    catch (err) { setError(err instanceof Error ? err.message : '登录失败'); } finally { setLoginBusy(false); }
  };

  if (!token) return <main className={`admin-login ${dark ? 'dark' : ''}`}><section className="login-panel"><div className="brand-mark">H</div><p className="eyebrow">HyperTicket / OPERATIONS</p><h1>欢迎回到运营控制台</h1><p className="muted">用清晰的数据，保持每一场现场顺利发生。</p><form onSubmit={login}><label>管理员账号<input name="username" autoComplete="username" required placeholder="输入账号" /></label><label>密码<input name="password" type="password" autoComplete="current-password" required placeholder="输入密码" /></label>{error && <p className="error">{error}</p>}<button className="primary" disabled={loginBusy}>{loginBusy ? '验证中…' : '进入控制台'}</button></form><button className="theme-link" onClick={() => setDark(!dark)}>{dark ? <Sun size={16}/> : <Moon size={16}/>}切换主题</button></section></main>;

  const nav = [{ id: 'overview' as const, label: '运营概览', icon: Activity }, { id: 'tickets' as const, label: '票务管理', icon: Ticket }, { id: 'users' as const, label: '用户管理', icon: Users }];
  return <main className={`admin-shell ${dark ? 'dark' : ''}`}><aside><div className="brand"><span className="brand-mark small">H</span><span><b>HyperTicket</b><small>运营控制台</small></span></div><p className="nav-label">工作区</p><nav>{nav.map(({ id, label, icon: Icon }) => <button key={id} className={tab === id ? 'active' : ''} onClick={() => { setTab(id); load(id); }}><Icon size={18}/>{label}</button>)}</nav><div className="account"><div className="avatar">{user.slice(0, 1).toUpperCase()}</div><div><b>{user}</b><small>管理员</small></div><button aria-label="退出登录" onClick={() => setToken('')}><LogOut size={17}/></button></div></aside><section className="workspace"><header><div><h1>{nav.find(n => n.id === tab)?.label}</h1><p className="muted">保持库存、订单和用户状态准确。</p></div><div className="header-actions"><span className="online"><CheckCircle2 size={15}/>在线</span><button aria-label="切换主题" onClick={() => setDark(!dark)}>{dark ? <Sun size={18}/> : <Moon size={18}/>}</button></div></header><div className="content">{error && <p className="error banner">{error}</p>}{tab === 'overview' && <Overview stats={stats} tickets={tickets} onRefresh={() => load('overview')}/>} {tab === 'tickets' && <List title="票务列表" icon={<CalendarDays size={18}/>} rows={tickets} columns={['title', 'venue', 'event_date', 'available_seats']} onRefresh={() => load('tickets')}/>} {tab === 'users' && <List title="用户列表" icon={<Users size={18}/>} rows={users} columns={['username', 'tel', 'status']} onRefresh={() => load('users')}/>}</div></section></main>;
}

function Overview({ stats, tickets, onRefresh }: { stats: Record<string, number>; tickets: Record<string, unknown>[]; onRefresh: () => void }) { const cards = [['在售票务', stats.tickets, Ticket], ['注册用户', stats.users, Users], ['累计订单', stats.orders, CalendarDays], ['今日订单', stats.today, Activity]] as const; return <><div className="page-tools"><div><h2>今天的运营状态</h2><p className="muted">核心数据与最近票务动态</p></div><button className="secondary" onClick={onRefresh}>刷新数据</button></div><div className="stat-grid">{cards.map(([label, value, Icon]) => <div className="stat" key={label}><Icon size={18}/><span>{label}</span><strong>{value.toLocaleString()}</strong></div>)}</div><List title="最近票务" icon={<Ticket size={18}/>} rows={tickets.slice(0, 6)} columns={['title', 'venue', 'event_date', 'available_seats']} onRefresh={onRefresh}/></>; }
function List({ title, icon, rows, columns, onRefresh }: { title: string; icon: React.ReactNode; rows: Record<string, unknown>[]; columns: string[]; onRefresh: () => void }) { return <section className="data-section"><div className="section-head"><h2>{icon}{title}</h2><div><button className="icon-button" aria-label="搜索"><Search size={17}/></button><button className="secondary" onClick={onRefresh}>刷新</button></div></div>{rows.length === 0 ? <p className="empty">暂无数据</p> : <div className="table-wrap"><table><thead><tr>{columns.map(c => <th key={c}>{c === 'available_seats' ? '剩余座位' : c}</th>)}</tr></thead><tbody>{rows.map((row, i) => <tr key={i}>{columns.map(c => <td key={c}>{String(row[c] ?? '-')}</td>)}</tr>)}</tbody></table></div>}</section>; }
createRoot(document.getElementById('root')!).render(<StrictMode><App /></StrictMode>);
