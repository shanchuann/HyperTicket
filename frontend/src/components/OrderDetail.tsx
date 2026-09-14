import { useRef } from 'react';
import { X, Calendar, MapPin, Clock, CheckCircle, XCircle, AlertCircle, Tag, Download } from 'lucide-react';
import { QRCodeCanvas } from 'qrcode.react';
import type { Order } from '../types';
import SeatMini from './SeatMini';
import './OrderDetail.css';

const CATEGORY_LABEL: Record<string, string> = {
  concert: '演唱会', sports: '体育赛事',
  movie: '观影', theater: '舞台剧', exhibition: '展览',
};

const TIER_LABEL: Record<string, string> = { VIP: 'VIP', Standard: '标准', Economy: '经济' };

const STATUS_CONFIG: Record<Order['status'], { label: string; color: string; Icon: typeof Clock }> = {
  PENDING:   { label: '待确认', color: 'var(--color-warning)',       Icon: Clock },
  CONFIRMED: { label: '已确认', color: 'var(--color-success)',       Icon: CheckCircle },
  CANCELLED: { label: '已取消', color: 'var(--color-error)',         Icon: XCircle },
  EXPIRED:   { label: '已过期', color: 'var(--color-text-tertiary)', Icon: AlertCircle },
};

interface Props {
  order: Order;
  onClose: () => void;
}

const OrderDetail = ({ order, onClose }: Props) => {
  const printRef = useRef<HTMLDivElement>(null);
  const sc = STATUS_CONFIG[order.status];
  const StatusIcon = sc.Icon;
  const verifyUrl = `${window.location.origin}/verify?no=${order.order_no || order.id}`;

  const formatFullDate = (d: string) => {
    if (!d) return '—';
    return new Date(d).toLocaleDateString('zh-CN', {
      year: 'numeric', month: 'long', day: 'numeric', weekday: 'long',
    });
  };

  const handleDownload = () => {
    // 使用 CSS print 打印当前票券
    const style = document.createElement('style');
    style.id = '__ticket_print_style';
    style.textContent = `
      @media print {
        body > * { display: none !important; }
        #__ticket_print_root { display: block !important; }
      }
    `;
    const wrapper = document.createElement('div');
    wrapper.id = '__ticket_print_root';
    wrapper.style.cssText = 'display:none; position:fixed; inset:0; background:white; z-index:99999; padding:24px;';
    if (printRef.current) {
      wrapper.innerHTML = printRef.current.outerHTML;
    }
    document.head.appendChild(style);
    document.body.appendChild(wrapper);
    window.print();
    setTimeout(() => {
      document.head.removeChild(style);
      document.body.removeChild(wrapper);
    }, 1000);
  };

  return (
    <div className="od-overlay" onClick={e => e.target === e.currentTarget && onClose()}>
      <div className="od-shell">
        {/* 操作栏（不打印）*/}
        <div className="od-topbar od-noprint">
          <span className="od-topbar-title">电子凭证</span>
          <div style={{ display: 'flex', gap: '8px' }}>
            <button className="od-action-btn" onClick={handleDownload} title="下载/打印凭证">
              <Download size={16} /><span>下载</span>
            </button>
            <button className="od-close" onClick={onClose}><X size={20} /></button>
          </div>
        </div>

        {/* 票券主体（打印区域）*/}
        <div className="od-ticket" ref={printRef}>
          {/* 顶部色条 */}
          <div className="od-tear-top">
            <div className="od-tear-circle left" />
            <div className="od-tear-line" />
            <div className="od-tear-circle right" />
          </div>

          {/* 票头 */}
          <div className="od-ticket-head">
            <div className="od-status-badge" style={{ color: sc.color }}>
              <StatusIcon size={14} /><span>{sc.label}</span>
            </div>
            <span className="od-ticket-no">{order.order_no || `#${order.id}`}</span>
          </div>

          {/* 活动名 + 类别 */}
          <h2 className="od-title">{order.title}</h2>
          {order.category && (
            <span className="od-category-badge">
              {CATEGORY_LABEL[order.category] ?? order.category}
            </span>
          )}

          {/* 活动信息 */}
          <div className="od-info-grid">
            <div className="od-info-row">
              <Calendar size={15} className="od-info-icon" />
              <div>
                <div className="od-info-label">演出日期</div>
                <div className="od-info-value">{formatFullDate(order.event_date)}</div>
              </div>
            </div>
            <div className="od-info-row">
              <MapPin size={15} className="od-info-icon" />
              <div>
                <div className="od-info-label">演出场馆</div>
                <div className="od-info-value">{order.venue || '—'}</div>
              </div>
            </div>
            {order.seat_label && (
              <div className="od-info-row">
                <Tag size={15} className="od-info-icon" />
                <div>
                  <div className="od-info-label">座位</div>
                  <div className="od-info-value">
                    <strong>{order.seat_label}</strong>
                    {order.seat_tier && (
                      <span className={`od-tier-badge tier-${order.seat_tier.toLowerCase()}`}>
                        {TIER_LABEL[order.seat_tier] ?? order.seat_tier}
                      </span>
                    )}
                    {order.seat_price > 0 && <span className="od-price">¥{order.seat_price}</span>}
                  </div>
                </div>
              </div>
            )}
          </div>

          {/* 座位可视化 */}
          {order.seat_label && (
            <div className="od-seat-vis-section">
              <div className="od-seat-vis-label">座位位置</div>
              <SeatMini seatLabel={order.seat_label} tier={order.seat_tier} />
            </div>
          )}

          {/* 分割线 */}
          <div className="od-tear-mid">
            <div className="od-tear-circle left" />
            <div className="od-tear-line dashed" />
            <div className="od-tear-circle right" />
          </div>

          {/* QR 码 */}
          <div className="od-qr-section">
            <div className="od-qr-wrap">
              <QRCodeCanvas
                value={verifyUrl}
                size={120}
                level="M"
                marginSize={1}
                style={{ display: 'block' }}
              />
            </div>
            <div className="od-qr-meta">
              <div className="od-qr-hint">扫描二维码验证票务状态</div>
              <div className="od-qr-no">{order.order_no || `HT${String(order.id).padStart(8,'0')}`}</div>
              <div className="od-qr-sub">数量：{order.quantity} 张</div>
            </div>
          </div>

          {/* 底部元信息 */}
          <div className="od-meta">
            <div className="od-meta-item">
              <span className="od-meta-label">订单编号</span>
              <span className="od-meta-value">#{order.id}</span>
            </div>
            {order.created_at && (
              <div className="od-meta-item">
                <span className="od-meta-label">下单时间</span>
                <span className="od-meta-value">{order.created_at}</span>
              </div>
            )}
          </div>
        </div>

        <p className="od-hint od-noprint">凭此凭证入场，请勿泄露订单编号</p>
      </div>
    </div>
  );
};

export default OrderDetail;
