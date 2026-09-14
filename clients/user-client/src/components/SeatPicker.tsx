import { useEffect, useMemo, useState, type CSSProperties } from 'react';
import { Armchair, CalendarDays, Loader2, MapPin, TicketCheck, X } from 'lucide-react';
import { orderApi } from '../api/orders';
import { toChineseError } from '../api/errors';
import type { Seat, Ticket } from '../types';
import './SeatPicker.css';

const tierLabel: Record<string, string> = { VIP: 'VIP', Standard: '标准区', Economy: '经济区' };

type Props = {
  ticket: Ticket;
  quantity: number;
  onClose: () => void;
  onSuccess: (message: string) => void;
  onError: (message: string) => void;
};

export default function SeatPicker({ ticket, quantity, onClose, onSuccess, onError }: Props) {
  const [seats, setSeats] = useState<Seat[]>([]);
  const [hasSeats, setHasSeats] = useState(false);
  const [selected, setSelected] = useState<Seat | null>(null);
  const [loading, setLoading] = useState(true);
  const [ordering, setOrdering] = useState(false);

  useEffect(() => {
    const onKey = (event: KeyboardEvent) => event.key === 'Escape' && !ordering && onClose();
    document.body.style.overflow = 'hidden';
    window.addEventListener('keydown', onKey);
    return () => { document.body.style.overflow = ''; window.removeEventListener('keydown', onKey); };
  }, [onClose, ordering]);

  useEffect(() => {
    orderApi.getSeats(ticket.id)
      .then(result => { setHasSeats(result.hasSeats); setSeats(result.seats); })
      .catch(error => onError(toChineseError(error, '座位图加载失败，请稍后重试')))
      .finally(() => setLoading(false));
  }, [onError, ticket.id]);

  const rows = useMemo(() => {
    const grouped = new Map<string, Seat[]>();
    for (const seat of seats) grouped.set(seat.row, [...(grouped.get(seat.row) || []), seat]);
    return [...grouped.entries()].map(([row, items]) => [row, [...items].sort((a, b) => a.col - b.col)] as const);
  }, [seats]);
  const maxColumn = useMemo(() => Math.max(1, ...seats.map(seat => seat.col)), [seats]);
  const availableCount = useMemo(() => seats.filter(seat => seat.status === 'AVAILABLE').length, [seats]);
  const aisleAfter = Math.ceil(maxColumn / 2);
  const seatGridStyle = {
    gridTemplateColumns: `repeat(${maxColumn + 1}, var(--seat-size))`,
  } satisfies CSSProperties;

  const submit = async () => {
    const token = localStorage.getItem('token');
    if (!token) { onError('请先登录后再预订'); return; }
    if (hasSeats && !selected) { onError('请先选择一个可用座位'); return; }
    setOrdering(true);
    try {
      if (selected) await orderApi.createSeatOrderAndWait(token, ticket.id, selected.id);
      else await orderApi.createOrderAndWait(token, ticket.id, quantity);
      onSuccess(selected
        ? `已锁定「${ticket.title}」${selected.label}，请在订单页完成支付`
        : `已锁定 ${quantity} 张「${ticket.title}」，请在订单页完成支付`);
      onClose();
    } catch (error) {
      onError(toChineseError(error, '预订失败，请重新选择'));
      if (hasSeats) {
        orderApi.getSeats(ticket.id).then(result => setSeats(result.seats)).catch(() => undefined);
        setSelected(null);
      }
    } finally { setOrdering(false); }
  };

  return (
    <div className="seat-picker-backdrop" onMouseDown={event => event.target === event.currentTarget && !ordering && onClose()}>
      <section className="seat-picker" role="dialog" aria-modal="true" aria-labelledby="seat-picker-title">
        <header>
          <div className="seat-picker-heading">
            <span className="seat-picker-step">预订第 1 步 · 确认座位</span>
            <h2 id="seat-picker-title">{ticket.title}</h2>
            <div className="seat-picker-meta"><span><MapPin size={14}/>{ticket.venue}</span><span><CalendarDays size={14}/>{ticket.event_date}</span></div>
          </div>
          <button onClick={onClose} disabled={ordering} aria-label="关闭选座"><X size={20}/></button>
        </header>

        {loading ? <div className="seat-picker-loading"><Loader2 className="spinner"/><span>正在读取可用座位</span></div> : hasSeats ? <>
          <div className="seat-picker-plan-head">
            <div><strong>选择座位</strong><span>座位按场次排号与座号定位，横向滑动可查看完整区域。</span></div>
            <b>{availableCount} 个可选</b>
          </div>
          <div className="seat-picker-scroll" tabIndex={0} aria-label="座位分布图，可横向和纵向滚动">
            <div className="seat-map-canvas">
              <div className="seat-picker-stage"><span>面向舞台</span><strong>舞台 / 银幕</strong></div>
              <div className="seat-axis" aria-hidden="true">
                <span>排</span>
                <div className="seat-grid" style={seatGridStyle}>
                  {Array.from({ length: maxColumn }, (_, index) => {
                    const column = index + 1;
                    return <small key={column} style={{ gridColumn: column + (column > aisleAfter ? 1 : 0) }}>{column}</small>;
                  })}
                  <i style={{ gridColumn: aisleAfter + 1 }}>过道</i>
                </div>
              </div>
              {rows.map(([row, items]) => <div className="seat-row" key={row}>
                <span>{row}</span>
                <div className="seat-grid" style={seatGridStyle}>{items.map(seat => <button
                  key={seat.id}
                  style={{ gridColumn: seat.col + (seat.col > aisleAfter ? 1 : 0), gridRow: 1 }}
                  className={`seat ${seat.tier.toLowerCase()} ${seat.status === 'SOLD' ? 'sold' : ''} ${selected?.id === seat.id ? 'selected' : ''}`}
                  disabled={seat.status === 'SOLD'}
                  onClick={() => setSelected(seat)}
                  aria-label={`${seat.label}，第 ${seat.row} 排第 ${seat.col} 座，${tierLabel[seat.tier]}，${seat.price} 元${seat.status === 'SOLD' ? '，已售' : ''}`}
                  aria-pressed={selected?.id === seat.id}
                  title={`${seat.label} · ${tierLabel[seat.tier]} · ¥${seat.price}`}
                ><Armchair size={15}/><small>{seat.col}</small></button>)}</div>
              </div>)}
            </div>
          </div>
          <div className="seat-legend"><span><i className="vip"/>VIP</span><span><i className="standard"/>标准区</span><span><i className="economy"/>经济区</span><span><i className="sold"/>已售</span><small>过道与空缺座号按场次数据保留</small></div>
        </> : <div className="seat-picker-general"><TicketCheck size={30}/><div><b>本场不设在线选座</b><span>将按你选择的数量预留票券，共 {quantity} 张。</span></div></div>}

        <footer>
          <div className="seat-selection-summary"><small>当前选择</small>{selected ? <span><strong>{selected.label}</strong><em>{tierLabel[selected.tier]}</em><b>¥{selected.price}</b></span> : <span>{hasSeats ? '尚未选择座位' : `普通票 × ${quantity}`}</span>}</div>
          <button className="seat-picker-confirm" onClick={submit} disabled={loading || ordering || (hasSeats && !selected)}>{ordering ? <><Loader2 size={16} className="spinner"/>正在锁定</> : <>确认并锁定<TicketCheck size={17}/></>}</button>
        </footer>
      </section>
    </div>
  );
}
