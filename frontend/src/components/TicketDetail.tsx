import { useState, useEffect } from 'react';
import { X, Calendar, MapPin, Heart, Minus, Plus, Loader2, User } from 'lucide-react';
import { ticketApi } from '../api/tickets';
import { orderApi } from '../api/orders';
import type { Ticket } from '../types';
import { catchError } from '../utils/errors';
import './TicketDetail.css';

interface Props {
  ticketId: number;
  isFavorite: boolean;
  onClose: () => void;
  onOrdered: (msg: string, pending: boolean) => void;
  onFavoriteChange: (ticketId: number, fav: boolean) => void;
  onOpenSeatMap: (ticket: Ticket) => void;
}

const CATEGORY_LABEL: Record<string, string> = {
  concert: '演唱会', sports: '体育赛事', movie: '电影', theater: '话剧歌剧', exhibition: '展览休闲',
};

const TicketDetail = ({ ticketId, isFavorite, onClose, onOrdered, onFavoriteChange, onOpenSeatMap }: Props) => {
  const [ticket, setTicket] = useState<Ticket | null>(null);
  const [loading, setLoading] = useState(true);
  const [quantity, setQuantity] = useState(1);
  const [ordering, setOrdering] = useState(false);
  const [favBusy, setFavBusy] = useState(false);
  const [error, setError] = useState('');
  const [tab, setTab] = useState<'desc' | 'notice'>('desc');

  useEffect(() => {
    let cancelled = false;
    ticketApi.getDetail(ticketId)
      .then(t => { if (!cancelled) setTicket(t); })
      .catch(err => { if (!cancelled) setError(catchError(err, '加载详情失败')); })
      .finally(() => { if (!cancelled) setLoading(false); });
    return () => { cancelled = true; };
  }, [ticketId]);

  const handleFavorite = async () => {
    const token = localStorage.getItem('token');
    if (!token || favBusy) return;
    setFavBusy(true);
    try {
      await ticketApi.favorite(token, ticketId, isFavorite ? 'remove' : 'add');
      onFavoriteChange(ticketId, !isFavorite);
    } catch (err) {
      setError(catchError(err, '收藏操作失败'));
    } finally {
      setFavBusy(false);
    }
  };

  const handleOrder = async () => {
    const token = localStorage.getItem('token');
    if (!token) { setError('请先登录'); return; }
    if (!ticket) return;
    setOrdering(true);
    setError('');
    try {
      const resp = await orderApi.createOrder(token, ticket.id, undefined, quantity);
      const pending = resp.order_status === 'PENDING';
      onOrdered(
        pending
          ? `已锁定「${ticket.title}」x${quantity}，请在 ${resp.pay_deadline_minutes ?? 15} 分钟内完成支付`
          : `成功预订「${ticket.title}」x${quantity}`,
        pending,
      );
      onClose();
    } catch (err) {
      setError(catchError(err, '预订失败'));
    } finally {
      setOrdering(false);
    }
  };

  const maxQty = Math.min(6, ticket?.available_seats ?? 1);

  return (
    <div className="tkdetail-overlay" onClick={e => e.target === e.currentTarget && onClose()}>
      <div className="tkdetail-card">
        <button className="tkdetail-close" onClick={onClose} aria-label="关闭"><X size={20} /></button>

        {loading && (
          <div className="tkdetail-loading"><Loader2 className="tkdetail-spin" size={28} /><p>加载详情中...</p></div>
        )}

        {!loading && ticket && (
          <>
            <div className="tkdetail-hero">
              {ticket.cover_image
                ? <img src={ticket.cover_image} alt={ticket.title} className="tkdetail-cover" />
                : <div className="tkdetail-cover tkdetail-cover-placeholder">{CATEGORY_LABEL[ticket.category] || '演出'}</div>}
              <div className="tkdetail-head">
                <div className="tkdetail-tags">
                  <span className="tkdetail-tag">{CATEGORY_LABEL[ticket.category] || ticket.category}</span>
                  {ticket.city && <span className="tkdetail-tag tkdetail-tag-city">{ticket.city}</span>}
                </div>
                <h2 className="tkdetail-title">{ticket.title}</h2>
                {ticket.artist && (
                  <div className="tkdetail-meta"><User size={14} /><span>{ticket.artist}</span></div>
                )}
                <div className="tkdetail-meta"><Calendar size={14} /><span>{ticket.event_date}</span></div>
                <div className="tkdetail-meta"><MapPin size={14} /><span>{ticket.venue}</span></div>
                <div className="tkdetail-priceline">
                  <span className="tkdetail-price">￥{ticket.price}</span>
                  <span className="tkdetail-price-note">起 · 剩余 {ticket.available_seats} 张</span>
                </div>
              </div>
            </div>

            <div className="tkdetail-tabs">
              <button className={`tkdetail-tab ${tab === 'desc' ? 'active' : ''}`} onClick={() => setTab('desc')}>演出详情</button>
              <button className={`tkdetail-tab ${tab === 'notice' ? 'active' : ''}`} onClick={() => setTab('notice')}>购票须知</button>
            </div>
            <div className="tkdetail-body">
              {tab === 'desc'
                ? <p className="tkdetail-text">{ticket.description || '暂无详情介绍。'}</p>
                : <p className="tkdetail-text">{ticket.notice || '暂无购票须知。'}</p>}
            </div>

            {error && <div className="tkdetail-error">{error}</div>}

            <div className="tkdetail-footer">
              <button className={`tkdetail-fav ${isFavorite ? 'active' : ''}`}
                onClick={handleFavorite} disabled={favBusy} title={isFavorite ? '取消收藏' : '收藏'}>
                <Heart size={18} fill={isFavorite ? 'currentColor' : 'none'} />
                <span>{isFavorite ? '已想看' : '想看'}</span>
              </button>

              <div className="tkdetail-qty">
                <button onClick={() => setQuantity(q => Math.max(1, q - 1))}
                  disabled={quantity <= 1} aria-label="减少数量"><Minus size={14} /></button>
                <span>{quantity} 张</span>
                <button onClick={() => setQuantity(q => Math.min(maxQty, q + 1))}
                  disabled={quantity >= maxQty} aria-label="增加数量"><Plus size={14} /></button>
              </div>

              <button className="tkdetail-order tkdetail-order-seat"
                disabled={ticket.available_seats <= 0}
                onClick={() => { onOpenSeatMap(ticket); onClose(); }}>
                选座购买
              </button>
              <button className="tkdetail-order"
                disabled={ordering || ticket.available_seats <= 0}
                onClick={handleOrder}>
                {ordering ? '提交中...' : ticket.available_seats <= 0 ? '已售罄' : '立即购买'}
              </button>
            </div>
          </>
        )}

        {!loading && !ticket && (
          <div className="tkdetail-loading"><p>{error || '票品不存在或已下架'}</p></div>
        )}
      </div>
    </div>
  );
};

export default TicketDetail;
