import { useState, useEffect, useCallback, useRef } from 'react';
import { Search, Calendar, MapPin, ChevronRight, Heart, Flame, User } from 'lucide-react';
import { ticketApi } from '../../api/tickets';
import type { Ticket } from '../../types';
import { catchError } from '../../utils/errors';
import { reportStateAnomaly } from '../../utils/monitor';
import { useToast } from '../../components/ToastContext';
import SeatMap from '../../components/SeatMap';
import TicketDetail from '../../components/TicketDetail';
import './TicketList.css';

const CATEGORIES = [
  { key: '', label: '全部' },
  { key: 'concert', label: '演唱会' },
  { key: 'sports', label: '体育' },
  { key: 'theater', label: '话剧歌剧' },
  { key: 'exhibition', label: '展览' },
  { key: 'movie', label: '电影' },
];

const TicketList = () => {
  const toast = useToast();
  const [tickets, setTickets] = useState<Ticket[]>([]);
  const [hotList, setHotList] = useState<Ticket[]>([]);
  const [cities, setCities] = useState<string[]>([]);
  const [favoriteIds, setFavoriteIds] = useState<Set<number>>(new Set());
  const [loading, setLoading] = useState(true);
  const [searchQuery, setSearchQuery] = useState('');
  const [city, setCity] = useState('');
  const [category, setCategory] = useState('');
  const [seatTicket, setSeatTicket] = useState<Ticket | null>(null);
  const [detailId, setDetailId] = useState<number | null>(null);
  const searchTimer = useRef<ReturnType<typeof setTimeout> | null>(null);

  const loadTickets = useCallback(async (kw: string, ct: string, cat: string) => {
    setLoading(true);
    try {
      const list = await ticketApi.getTickets({ keyword: kw, city: ct, category: cat });
      setTickets(list);
      // 状态异常上报：无过滤条件却拿到空列表，通常是数据侧问题（全部下架/过期）
      if (list.length === 0 && !kw && !ct && !cat) {
        reportStateAnomaly('TicketList.tsx', '票务列表接口返回成功但列表为空（num=0）', {
          api: 'VIEW(type=4)',
          hint: '检查 tickets 表 status 字段与 offlineExpired 定时任务是否误下架未过期票',
        });
      }
    } catch (err) {
      toast.error(catchError(err, '加载票务失败，请重试'));
    } finally {
      setLoading(false);
    }
  }, []); // eslint-disable-line react-hooks/exhaustive-deps

  // 首次加载：全量列表提取城市集合 + 热门榜 + 我的收藏
  useEffect(() => {
    ticketApi.getTickets().then(list => {
      setCities([...new Set(list.map(t => t.city).filter(Boolean))]);
    }).catch(() => {});
    ticketApi.getHotTickets(5).then(setHotList).catch(() => {});
    const token = localStorage.getItem('token');
    if (token) {
      ticketApi.getFavorites(token)
        .then(list => setFavoriteIds(new Set(list.map(t => t.id))))
        .catch(() => {});
    }
  }, []);

  // 关键词防抖 300ms；城市/分类切换立即查询（含首次加载）
  useEffect(() => {
    if (searchTimer.current) clearTimeout(searchTimer.current);
    searchTimer.current = setTimeout(() => {
      loadTickets(searchQuery.trim(), city, category);
    }, searchQuery ? 300 : 0);
    return () => { if (searchTimer.current) clearTimeout(searchTimer.current); };
  }, [searchQuery, city, category, loadTickets]);

  const refresh = () => loadTickets(searchQuery.trim(), city, category);

  const handleOrdered = (msg: string, pending: boolean) => {
    if (pending) toast.info(msg);
    else toast.success(msg);
    refresh();
    ticketApi.getHotTickets(5).then(setHotList).catch(() => {});
  };

  const handleFavToggle = async (ticket: Ticket, e?: React.MouseEvent) => {
    e?.stopPropagation();
    const token = localStorage.getItem('token');
    if (!token) { toast.error('请先登录'); return; }
    const isFav = favoriteIds.has(ticket.id);
    try {
      await ticketApi.favorite(token, ticket.id, isFav ? 'remove' : 'add');
      setFavoriteIds(prev => {
        const next = new Set(prev);
        if (isFav) next.delete(ticket.id); else next.add(ticket.id);
        return next;
      });
      toast.success(isFav ? '已取消收藏' : '已加入想看');
    } catch (err) {
      toast.error(catchError(err, '收藏操作失败'));
    }
  };

  const applyFavChange = (id: number, fav: boolean) => {
    setFavoriteIds(prev => {
      const next = new Set(prev);
      if (fav) next.add(id); else next.delete(id);
      return next;
    });
  };

  const formatDate = (dateString: string) =>
    new Date(dateString).toLocaleDateString('zh-CN', { month: 'short', day: 'numeric', weekday: 'short' });

  return (
    <div className="ticket-list-container">
      {seatTicket && (
        <SeatMap
          key={seatTicket.id}
          ticket={seatTicket}
          onClose={() => setSeatTicket(null)}
          onSuccess={msg => { setSeatTicket(null); handleOrdered(msg, true); }}
        />
      )}
      {detailId !== null && (
        <TicketDetail
          key={detailId}
          ticketId={detailId}
          isFavorite={favoriteIds.has(detailId)}
          onClose={() => setDetailId(null)}
          onOrdered={handleOrdered}
          onFavoriteChange={applyFavChange}
          onOpenSeatMap={t => setSeatTicket(t)}
        />
      )}

      <div className="ticket-list-header">
        <h1 className="ticket-list-title">票务浏览</h1>

        <div className="ticket-list-filters">
          <div className="ticket-search">
            <Search size={18} className="ticket-search-icon" />
            <input type="text" placeholder="搜索演出、场馆、艺人..."
              value={searchQuery} onChange={e => setSearchQuery(e.target.value)}
              className="ticket-search-input" />
          </div>
          <div className="ticket-categories">
            {CATEGORIES.map(c => (
              <button key={c.key}
                className={`ticket-category-btn ${category === c.key ? 'active' : ''}`}
                onClick={() => setCategory(c.key)}>
                {c.label}
              </button>
            ))}
          </div>
          {cities.length > 0 && (
            <div className="ticket-cities">
              <button className={`ticket-city-btn ${city === '' ? 'active' : ''}`}
                onClick={() => setCity('')}>全部城市</button>
              {cities.map(c => (
                <button key={c} className={`ticket-city-btn ${city === c ? 'active' : ''}`}
                  onClick={() => setCity(c)}>{c}</button>
              ))}
            </div>
          )}
        </div>
      </div>

      {/* 热门榜（无过滤条件时展示） */}
      {hotList.length > 0 && !searchQuery && !city && !category && (
        <div className="ticket-hot-section">
          <div className="ticket-hot-header"><Flame size={16} /><span>热门推荐</span></div>
          <div className="ticket-hot-list">
            {hotList.map((t, i) => (
              <button key={t.id} className="ticket-hot-item" onClick={() => setDetailId(t.id)}>
                <span className={`ticket-hot-rank rank-${i + 1}`}>{i + 1}</span>
                <span className="ticket-hot-title">{t.title}</span>
                <span className="ticket-hot-city">{t.city}</span>
                {typeof t.hot === 'number' && t.hot > 0 && (
                  <span className="ticket-hot-count">{t.hot} 人已订</span>
                )}
              </button>
            ))}
          </div>
        </div>
      )}

      {loading ? (
        <div className="ticket-list-loading">
          <div className="loading-spinner" /><p>加载中...</p>
        </div>
      ) : (
        <>
          <div className="ticket-grid">
            {tickets.map(ticket => (
              <div key={ticket.id} className="ticket-card ticket-card-clickable"
                onClick={() => setDetailId(ticket.id)}>
                {ticket.cover_image && (
                  <div className="ticket-cover">
                    <img src={ticket.cover_image} alt={ticket.title} className="ticket-cover-img" />
                  </div>
                )}
                <div className="ticket-card-body">
                  <div className="ticket-card-header">
                    <span className="ticket-category">
                      {CATEGORIES.find(c => c.key === ticket.category)?.label || '演出'}
                      {ticket.city ? ` · ${ticket.city}` : ''}
                    </span>
                    <button className={`ticket-fav-btn ${favoriteIds.has(ticket.id) ? 'active' : ''}`}
                      onClick={e => handleFavToggle(ticket, e)}
                      title={favoriteIds.has(ticket.id) ? '取消收藏' : '想看'}>
                      <Heart size={16} fill={favoriteIds.has(ticket.id) ? 'currentColor' : 'none'} />
                    </button>
                  </div>
                  <h3 className="ticket-title">{ticket.title}</h3>
                  {ticket.artist && (
                    <div className="ticket-venue"><User size={14} /><span>{ticket.artist}</span></div>
                  )}
                  <div className="ticket-venue"><MapPin size={14} /><span>{ticket.venue}</span></div>
                  <div className="ticket-venue"><Calendar size={14} /><span>{formatDate(ticket.event_date)}</span></div>
                  <div className="ticket-seats">
                    <div className="ticket-seats-bar">
                      <div className="ticket-seats-fill" style={{
                        width: ticket.total_seats > 0
                          ? `${((ticket.total_seats - ticket.available_seats) / ticket.total_seats) * 100}%`
                          : '0%'
                      }} />
                    </div>
                    <span className="ticket-seats-text">剩余 {ticket.available_seats} 张</span>
                  </div>
                  <div className="ticket-footer">
                    <div className="ticket-price">
                      <span className="ticket-price-symbol">￥</span>
                      <span className="ticket-price-value">{ticket.price}</span>
                      <span className="ticket-price-unit">起</span>
                    </div>
                    <button className="ticket-action"
                      disabled={ticket.available_seats <= 0}
                      onClick={e => { e.stopPropagation(); setDetailId(ticket.id); }}>
                      {ticket.available_seats <= 0 ? '已售罄' : '立即购买'}
                      {ticket.available_seats > 0 && <ChevronRight size={16} />}
                    </button>
                  </div>
                </div>
              </div>
            ))}
          </div>

          {tickets.length === 0 && (
            <div className="ticket-list-empty">
              <p>没有找到匹配的票务</p>
              <button className="ticket-clear-filter"
                onClick={() => { setSearchQuery(''); setCity(''); setCategory(''); }}>
                清除筛选
              </button>
            </div>
          )}
        </>
      )}
    </div>
  );
};

export default TicketList;
