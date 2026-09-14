import { useState, useEffect } from 'react';
import { useNavigate } from 'react-router-dom';
import { Calendar, MapPin, Heart, ChevronRight } from 'lucide-react';
import { ticketApi } from '../../api/tickets';
import type { Ticket } from '../../types';
import { catchError } from '../../utils/errors';
import { useToast } from '../../components/ToastContext';
import TicketDetail from '../../components/TicketDetail';
import SeatMap from '../../components/SeatMap';
import './TicketList.css';

// 我的想看（收藏列表），复用 TicketList 的卡片样式
const FavoriteList = () => {
  const navigate = useNavigate();
  const toast = useToast();
  const [favorites, setFavorites] = useState<Ticket[]>([]);
  const [loading, setLoading] = useState(true);
  const [detailId, setDetailId] = useState<number | null>(null);
  const [seatTicket, setSeatTicket] = useState<Ticket | null>(null);

  useEffect(() => {
    const token = localStorage.getItem('token');
    if (!token) {
      navigate('/auth/login', { replace: true });
      return;
    }

    let cancelled = false;
    ticketApi.getFavorites(token)
      .then(data => { if (!cancelled) setFavorites(data); })
      .catch(err => { if (!cancelled) toast.error(catchError(err, '加载收藏失败')); })
      .finally(() => { if (!cancelled) setLoading(false); });
    return () => { cancelled = true; };
  }, [navigate, toast]);

  const handleRemove = async (ticket: Ticket, e: React.MouseEvent) => {
    e.stopPropagation();
    const token = localStorage.getItem('token');
    if (!token) return;
    try {
      await ticketApi.favorite(token, ticket.id, 'remove');
      setFavorites(prev => prev.filter(t => t.id !== ticket.id));
      toast.success('已取消收藏');
    } catch (err) {
      toast.error(catchError(err, '操作失败'));
    }
  };

  const formatDate = (d: string) =>
    new Date(d).toLocaleDateString('zh-CN', { month: 'short', day: 'numeric', weekday: 'short' });

  if (loading) {
    return (
      <div className="ticket-list-container">
        <div className="ticket-list-loading"><div className="loading-spinner" /><p>加载中...</p></div>
      </div>
    );
  }

  return (
    <div className="ticket-list-container">
      {seatTicket && (
        <SeatMap key={seatTicket.id} ticket={seatTicket} onClose={() => setSeatTicket(null)}
          onSuccess={msg => { setSeatTicket(null); toast.info(msg); }} />
      )}
      {detailId !== null && (
        <TicketDetail
          key={detailId}
          ticketId={detailId}
          isFavorite={favorites.some(t => t.id === detailId)}
          onClose={() => setDetailId(null)}
          onOrdered={(msg, pending) => (pending ? toast.info(msg) : toast.success(msg))}
          onFavoriteChange={(id, fav) => { if (!fav) setFavorites(prev => prev.filter(t => t.id !== id)); }}
          onOpenSeatMap={t => setSeatTicket(t)}
        />
      )}

      <div className="ticket-list-header">
        <h1 className="ticket-list-title">我的想看</h1>
      </div>

      {favorites.length === 0 ? (
        <div className="ticket-list-empty">
          <p>还没有收藏的演出</p>
          <button className="ticket-clear-filter" onClick={() => navigate('/customer')}>去逛逛</button>
        </div>
      ) : (
        <div className="ticket-grid">
          {favorites.map(ticket => (
            <div key={ticket.id} className="ticket-card ticket-card-clickable"
              onClick={() => setDetailId(ticket.id)}>
              <div className="ticket-card-body">
                <div className="ticket-card-header">
                  <span className="ticket-category">{ticket.city || '演出'}{ticket.status !== 1 ? ' · 已下架' : ''}</span>
                  <button className="ticket-fav-btn active" onClick={e => handleRemove(ticket, e)} title="取消收藏">
                    <Heart size={16} fill="currentColor" />
                  </button>
                </div>
                <h3 className="ticket-title">{ticket.title}</h3>
                <div className="ticket-venue"><MapPin size={14} /><span>{ticket.venue}</span></div>
                <div className="ticket-venue"><Calendar size={14} /><span>{formatDate(ticket.event_date)}</span></div>
                <div className="ticket-footer">
                  <div className="ticket-price">
                    <span className="ticket-price-symbol">￥</span>
                    <span className="ticket-price-value">{ticket.price}</span>
                    <span className="ticket-price-unit">起</span>
                  </div>
                  <button className="ticket-action"
                    disabled={ticket.status !== 1 || ticket.available_seats <= 0}
                    onClick={e => { e.stopPropagation(); setDetailId(ticket.id); }}>
                    {ticket.status !== 1 ? '已下架' : ticket.available_seats <= 0 ? '已售罄' : '立即购买'}
                    {ticket.status === 1 && ticket.available_seats > 0 && <ChevronRight size={16} />}
                  </button>
                </div>
              </div>
            </div>
          ))}
        </div>
      )}
    </div>
  );
};

export default FavoriteList;
