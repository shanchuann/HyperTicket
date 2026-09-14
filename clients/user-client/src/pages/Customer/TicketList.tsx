import { useState, useEffect } from 'react';
import { Search, Calendar, MapPin, ChevronRight, Sparkles, Heart, Minus, Plus } from 'lucide-react';
import { ticketApi } from '../../api/tickets';
import { orderApi } from '../../api/orders';
import { toChineseError } from '../../api/errors';
import type { Ticket } from '../../types';
import Toast from '../../components/Toast';
import SeatPicker from '../../components/SeatPicker';
import './TicketList.css';

const TicketList = () => {
  const [tickets, setTickets] = useState<Ticket[]>([]);
  const [loading, setLoading] = useState(true);
  const [loadError, setLoadError] = useState('');
  const [searchQuery, setSearchQuery] = useState('');
  const [quantities, setQuantities] = useState<Record<number, number>>({});
  const [favoriteIds, setFavoriteIds] = useState<Set<number>>(new Set());
  const [orderMsg, setOrderMsg] = useState('');
  const [orderSucceeded, setOrderSucceeded] = useState(false);
  const [selectedTicket, setSelectedTicket] = useState<Ticket | null>(null);
  const [city, setCity] = useState('');
  const [category, setCategory] = useState('');
  const [cities, setCities] = useState<string[]>([]);

  const categories = [
    { id: '', label: '全部' },
    { id: 'concert', label: '演唱会' },
    { id: 'sports', label: '体育' },
    { id: 'theater', label: '话剧歌剧' },
    { id: 'exhibition', label: '展览' },
    { id: 'movie', label: '电影' },
  ];

  const loadTickets = async (ct = city, cat = category) => {
    setLoading(true);
    setLoadError('');
    try {
      const data = await ticketApi.getTickets({ city: ct, category: cat });
      setTickets(data);
    } catch (err) {
      setLoadError(toChineseError(err, '加载票务失败，请重试'));
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    // 首次加载顺带提取城市集合
    ticketApi.getTickets().then(data => {
      setCities([...new Set(data.map(t => t.city).filter(Boolean))]);
    }).catch(() => {});
    const token = localStorage.getItem('token');
    if (token) {
      ticketApi.getFavorites(token).then(items => setFavoriteIds(new Set(items.map(item => item.id)))).catch(() => {});
      orderApi.resumePendingOrder(token).then(result => {
        if (result?.order_status === 'PENDING') {
          setOrderSucceeded(true);
          setOrderMsg('上次排队订单已锁定库存，请到“我的订单”完成支付');
        }
      }).catch(err => {
        setOrderSucceeded(false);
        setOrderMsg(toChineseError(err, '上次订单状态查询失败'));
      });
    }
  }, []);

  useEffect(() => { loadTickets(city, category); }, [city, category]);

  const beginOrder = (ticket: Ticket) => {
    if (!localStorage.getItem('token')) {
      setOrderSucceeded(false);
      setOrderMsg('请先登录');
      return;
    }
    setSelectedTicket(ticket);
  };

  const toggleFavorite = async (ticket: Ticket) => {
    const token = localStorage.getItem('token');
    if (!token) return;
    const active = favoriteIds.has(ticket.id);
    setFavoriteIds(prev => { const next = new Set(prev); active ? next.delete(ticket.id) : next.add(ticket.id); return next; });
    try { await ticketApi.favorite(token, ticket.id, active ? 'remove' : 'add'); }
    catch { setFavoriteIds(prev => { const next = new Set(prev); active ? next.add(ticket.id) : next.delete(ticket.id); return next; }); }
  };

  const filteredTickets = tickets.filter(ticket => {
    const matchesSearch = ticket.title.toLowerCase().includes(searchQuery.toLowerCase()) ||
                         ticket.venue.toLowerCase().includes(searchQuery.toLowerCase());
    return matchesSearch;
  });

  const formatDate = (dateString: string) => {
    const date = new Date(dateString);
    return date.toLocaleDateString('zh-CN', {
      month: 'short',
      day: 'numeric',
      weekday: 'short'
    });
  };

  if (loading) {
    return (
      <div className="ticket-list-container">
        <div className="ticket-list-loading">
          <div className="loading-spinner"></div>
          <p>加载中...</p>
        </div>
      </div>
    );
  }

  return (
    <div className="ticket-list-container">
      <Toast
        message={loadError || orderMsg}
        tone={loadError || !orderSucceeded ? 'error' : 'success'}
        onClose={() => loadError ? setLoadError('') : setOrderMsg('')}
        actionLabel={loadError ? '重新加载' : undefined}
        onAction={loadError ? () => void loadTickets() : undefined}
      />
      {selectedTicket && <SeatPicker
        ticket={selectedTicket}
        quantity={quantities[selectedTicket.id] ?? 1}
        onClose={() => setSelectedTicket(null)}
        onSuccess={message => {
          setOrderSucceeded(true);
          setOrderMsg(message);
          void loadTickets();
        }}
        onError={message => { setOrderSucceeded(false); setOrderMsg(message); }}
      />}
      <div className="ticket-list-header">
        <div className="ticket-list-heading-row">
          <div>
            <div className="ticket-list-kicker"><Sparkles size={14} /> 精选现场</div>
            <h1 className="ticket-list-title">找到值得奔赴的现场</h1>
            <p className="ticket-list-subtitle">按城市、类型或场馆筛选，三步完成预订。</p>
          </div>
          <div className="ticket-list-count" aria-live="polite">
            <strong>{filteredTickets.length}</strong>
            <span>场可预订</span>
          </div>
        </div>

        <div className="ticket-list-filters">
          <div className="ticket-search">
            <Search size={18} className="ticket-search-icon" />
            <input
              type="text"
              placeholder="搜索演出、场馆..."
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              className="ticket-search-input"
            />
          </div>

          <div className="ticket-categories">
            {categories.map(cat => (
              <button
                key={cat.id}
                className={`ticket-category-btn ${category === cat.id ? 'active' : ''}`}
                onClick={() => setCategory(cat.id)}
              >
                {cat.label}
              </button>
            ))}
          </div>
          {cities.length > 0 && (
            <div className="ticket-categories">
              <button className={`ticket-category-btn ${city === '' ? 'active' : ''}`}
                onClick={() => setCity('')}>全部城市</button>
              {cities.map(c => (
                <button key={c} className={`ticket-category-btn ${city === c ? 'active' : ''}`}
                  onClick={() => setCity(c)}>{c}</button>
              ))}
            </div>
          )}
        </div>
      </div>

      <div className="ticket-grid">
        {filteredTickets.map(ticket => (
          <div key={ticket.id} className="ticket-card">
            <div className="ticket-card-header">
              <span className="ticket-category">
                {categories.find(c => c.id === ticket.category)?.label || '演出'}
                {ticket.city ? ` · ${ticket.city}` : ''}
              </span>
              <span className="ticket-date">
                <Calendar size={14} />
                {formatDate(ticket.event_date)}
              </span>
              <button className={`ticket-favorite ${favoriteIds.has(ticket.id) ? 'active' : ''}`} aria-label={favoriteIds.has(ticket.id) ? '取消收藏' : '收藏票务'} onClick={() => toggleFavorite(ticket)}>
                <Heart size={17} fill={favoriteIds.has(ticket.id) ? 'currentColor' : 'none'} />
              </button>
            </div>

            <h3 className="ticket-title">{ticket.title}</h3>

            <div className="ticket-venue">
              <MapPin size={14} />
              <span>{ticket.venue}</span>
            </div>

            <div className="ticket-seats">
              <div className="ticket-seats-bar">
                <div
                  className="ticket-seats-fill"
                  style={{
                    transform: `scaleX(${ticket.total_seats > 0
                      ? (ticket.total_seats - ticket.available_seats) / ticket.total_seats
                      : 0})`
                  }}
                />
              </div>
              <span className="ticket-seats-text">
                剩余 {ticket.available_seats} 张
              </span>
            </div>

            <div className="ticket-footer">
              <div className="ticket-price">
                <span className="ticket-price-symbol">￥</span>
                <span className="ticket-price-value">{ticket.price}</span>
                <span className="ticket-price-unit">起</span>
              </div>
              <div className="ticket-purchase">
                <div className="quantity-stepper" aria-label="购票数量">
                  <button aria-label="减少数量" disabled={(quantities[ticket.id] ?? 1) <= 1} onClick={() => setQuantities(q => ({...q, [ticket.id]: Math.max(1, (q[ticket.id] ?? 1) - 1)}))}><Minus size={14}/></button>
                  <span>{quantities[ticket.id] ?? 1}</span>
                  <button aria-label="增加数量" disabled={(quantities[ticket.id] ?? 1) >= Math.min(6, ticket.available_seats)} onClick={() => setQuantities(q => ({...q, [ticket.id]: Math.min(6, ticket.available_seats, (q[ticket.id] ?? 1) + 1)}))}><Plus size={14}/></button>
                </div>
                <button className="ticket-action" disabled={ticket.available_seats <= 0} onClick={() => beginOrder(ticket)}>
                  {ticket.available_seats <= 0 ? '已售罄' : '选座预订'} {ticket.available_seats > 0 && <ChevronRight size={16} />}
                </button>
              </div>
            </div>
          </div>
        ))}
      </div>

      {filteredTickets.length === 0 && !loading && (
        <div className="ticket-list-empty">
          <p>没有找到匹配的票务</p>
          <button
            className="ticket-clear-filter"
            onClick={() => setSearchQuery('')}
          >
            清除搜索
          </button>
        </div>
      )}
    </div>
  );
};

export default TicketList;
