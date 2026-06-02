import { useState, useEffect } from 'react';
import { Search, Calendar, MapPin, ChevronRight } from 'lucide-react';
import { ticketApi } from '../../api/tickets';
import { orderApi } from '../../api/orders';
import type { Ticket } from '../../types';
import './TicketList.css';

const TicketList = () => {
  const [tickets, setTickets] = useState<Ticket[]>([]);
  const [loading, setLoading] = useState(true);
  const [loadError, setLoadError] = useState('');
  const [searchQuery, setSearchQuery] = useState('');
  const [orderingId, setOrderingId] = useState<number | null>(null);
  const [orderMsg, setOrderMsg] = useState('');

  const categories = [
    { id: 'all', label: '全部' },
  ];

  const loadTickets = async () => {
    setLoading(true);
    setLoadError('');
    try {
      const data = await ticketApi.getTickets();
      setTickets(data);
    } catch (err) {
      setLoadError(err instanceof Error ? err.message : '加载票务失败，请重试');
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    loadTickets();
  }, []);

  const handleOrder = async (ticket: Ticket) => {
    const token = localStorage.getItem('token');
    if (!token) {
      setOrderMsg('请先登录');
      return;
    }
    setOrderingId(ticket.id);
    setOrderMsg('');
    try {
      await orderApi.createOrder(token, ticket.id);
      setOrderMsg(`成功预订「${ticket.title}」`);
      // 刷新票务列表更新剩余数量
      const data = await ticketApi.getTickets();
      setTickets(data);
    } catch (err) {
      setOrderMsg(err instanceof Error ? err.message : '预订失败');
    } finally {
      setOrderingId(null);
    }
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
      <div className="ticket-list-header">
        <h1 className="ticket-list-title">票务浏览</h1>

        {loadError && (
          <div className="order-msg error">
            {loadError}
            <button
              style={{ marginLeft: '12px', cursor: 'pointer', background: 'none', border: 'none', color: 'inherit', textDecoration: 'underline' }}
              onClick={loadTickets}
            >重试</button>
          </div>
        )}

        {orderMsg && (
          <div className={`order-msg ${orderMsg.includes('成功') ? 'success' : 'error'}`}>
            {orderMsg}
          </div>
        )}

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
                className="ticket-category-btn active"
              >
                {cat.label}
              </button>
            ))}
          </div>
        </div>
      </div>

      <div className="ticket-grid">
        {filteredTickets.map(ticket => (
          <div key={ticket.id} className="ticket-card">
            <div className="ticket-card-header">
              <span className="ticket-category">票务</span>
              <span className="ticket-date">
                <Calendar size={14} />
                {formatDate(ticket.event_date)}
              </span>
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
                    width: ticket.total_seats > 0
                      ? `${((ticket.total_seats - ticket.available_seats) / ticket.total_seats) * 100}%`
                      : '0%'
                  }}
                />
              </div>
              <span className="ticket-seats-text">
                剩余 {ticket.available_seats} 张
              </span>
            </div>

            <div className="ticket-footer">
              <div className="ticket-price">
                <span className="ticket-price-symbol">剩余</span>
                <span className="ticket-price-value">{ticket.available_seats}</span>
                <span className="ticket-price-unit">张</span>
              </div>
              <button
                className="ticket-action"
                disabled={ticket.available_seats <= 0 || orderingId === ticket.id}
                onClick={() => handleOrder(ticket)}
              >
                {orderingId === ticket.id ? '预订中...' : (ticket.available_seats <= 0 ? '已售罄' : '预订')}
                {orderingId !== ticket.id && ticket.available_seats > 0 && <ChevronRight size={16} />}
              </button>
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
