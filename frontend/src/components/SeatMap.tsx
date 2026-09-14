import { useState, useEffect } from 'react';
import { X, Loader2 } from 'lucide-react';
import { orderApi } from '../api/orders';
import type { Seat, Ticket } from '../types';
import { catchError } from '../utils/errors';
import './SeatMap.css';

const TIER_LABEL: Record<string, string> = { VIP: 'VIP', Standard: '标准', Economy: '经济' };
const TIER_ORDER: Record<string, number> = { VIP: 0, Standard: 1, Economy: 2 };

interface Props {
  ticket: Ticket;
  onClose: () => void;
  onSuccess: (msg: string) => void;
}

const SeatMap = ({ ticket, onClose, onSuccess }: Props) => {
  const [seats, setSeats] = useState<Seat[]>([]);
  const [loading, setLoading] = useState(true);
  const [selected, setSelected] = useState<Seat | null>(null);
  const [ordering, setOrdering] = useState(false);
  const [error, setError] = useState('');

  useEffect(() => {
    let cancelled = false;
    orderApi.getSeats(ticket.id).then(({ hasSeats, seats: s }: { hasSeats: boolean; seats: Seat[] }) => {
      if (!cancelled) {
        setSeats(hasSeats ? s : []);
        setLoading(false);
      }
    }).catch(() => {
      if (!cancelled) setLoading(false);
    });
    return () => { cancelled = true; };
  }, [ticket.id]);

  // Group seats by row
  const rows = seats.reduce<Record<string, Seat[]>>((acc, s) => {
    (acc[s.row] = acc[s.row] || []).push(s);
    return acc;
  }, {});
  const rowKeys = Object.keys(rows).sort();

  const tierGroups = [...new Set(seats.map(s => s.tier))].sort(
    (a, b) => (TIER_ORDER[a] ?? 9) - (TIER_ORDER[b] ?? 9)
  );

  const handleOrder = async () => {
    const token = localStorage.getItem('token');
    if (!token) { setError('请先登录'); return; }
    // 有座位图时必须选座；无座位图时直接下单（随机分配）
    if (!selected && seats.length > 0) { setError('请先选择座位'); return; }
    setOrdering(true);
    setError('');
    try {
      await orderApi.createOrder(token, ticket.id, selected?.id);
      const msg = selected
        ? `成功预订「${ticket.title}」${selected.label} 座位`
        : `成功预订「${ticket.title}」`;
      onSuccess(msg);
      onClose();
    } catch (err) {
      setError(catchError(err, '预订失败'));
      orderApi.getSeats(ticket.id).then(({ seats: s }: { hasSeats: boolean; seats: Seat[] }) => setSeats(s));
      setSelected(null);
    } finally {
      setOrdering(false);
    }
  };

  return (
    <div className="seatmap-overlay" onClick={e => e.target === e.currentTarget && onClose()}>
      <div className="seatmap-card">
        {/* Header */}
        <div className="seatmap-header">
          <div>
            <h2 className="seatmap-title">{ticket.title}</h2>
            <p className="seatmap-subtitle">选择您的座位</p>
          </div>
          <button className="seatmap-close" onClick={onClose}><X size={20} /></button>
        </div>

        {loading ? (
          <div className="seatmap-loading"><Loader2 size={28} className="spinner" /><span>加载座位图...</span></div>
        ) : seats.length === 0 ? (
          <div className="seatmap-no-seats">此票务暂无座位图，将随机分配座位</div>
        ) : (
          <>
            {/* Stage */}
            <div className="seatmap-stage">舞台 / 场地</div>

            {/* Seat grid */}
            <div className="seatmap-grid-wrapper">
              <div className="seatmap-grid">
                {rowKeys.map(rowKey => (
                  <div key={rowKey} className="seatmap-row">
                    <span className="seatmap-row-label">{rowKey}</span>
                    <div className="seatmap-row-seats">
                      {rows[rowKey].sort((a, b) => a.col - b.col).map(seat => (
                        <button
                          key={seat.id}
                          className={[
                            'seatmap-seat',
                            `tier-${seat.tier.toLowerCase()}`,
                            seat.status === 'SOLD' ? 'sold' : '',
                            selected?.id === seat.id ? 'selected' : '',
                          ].filter(Boolean).join(' ')}
                          onClick={() => seat.status === 'AVAILABLE' && setSelected(seat)}
                          disabled={seat.status === 'SOLD'}
                          title={`${seat.label} · ${TIER_LABEL[seat.tier]} · ¥${seat.price}`}
                        >
                          {seat.col}
                        </button>
                      ))}
                    </div>
                  </div>
                ))}
              </div>
            </div>

            {/* Legend */}
            <div className="seatmap-legend">
              {tierGroups.map(tier => (
                <span key={tier} className="seatmap-legend-item">
                  <span className={`seatmap-legend-dot tier-${tier.toLowerCase()}`} />
                  {TIER_LABEL[tier]}
                </span>
              ))}
              <span className="seatmap-legend-item">
                <span className="seatmap-legend-dot sold" />已售
              </span>
              <span className="seatmap-legend-item">
                <span className="seatmap-legend-dot selected" />已选
              </span>
            </div>
          </>
        )}

        {/* Error */}
        {error && <div className="seatmap-error">{error}</div>}

        {/* Bottom bar */}
        <div className="seatmap-footer">
          {selected ? (
            <div className="seatmap-selection">
              <span className="seatmap-sel-label">
                {selected.label}
                <span className={`seatmap-tier-badge tier-${selected.tier.toLowerCase()}`}>
                  {TIER_LABEL[selected.tier]}
                </span>
              </span>
              <span className="seatmap-sel-price">¥{selected.price}</span>
            </div>
          ) : (
            <span className="seatmap-sel-hint">{seats.length > 0 ? '点击选择座位' : ''}</span>
          )}
          <button
            className="seatmap-confirm-btn"
            disabled={(!selected && seats.length > 0) || ordering}
            onClick={handleOrder}
          >
            {ordering ? <><Loader2 size={16} className="spinner" />预订中</> : '确认预订'}
          </button>
        </div>
      </div>
    </div>
  );
};

export default SeatMap;
