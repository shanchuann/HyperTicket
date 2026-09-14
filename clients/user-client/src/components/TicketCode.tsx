import { useEffect, useRef } from 'react';
import { CalendarDays, Download, MapPin, QrCode, TicketCheck, X } from 'lucide-react';
import { QRCodeCanvas } from 'qrcode.react';
import type { Order } from '../types';
import './TicketCode.css';

type Props = { order: Order; onClose: () => void };

export default function TicketCode({ order, onClose }: Props) {
  const ticketRef = useRef<HTMLDivElement>(null);
  const code = order.order_no || `HT${String(order.id).padStart(10, '0')}`;

  useEffect(() => {
    const onKey = (event: KeyboardEvent) => event.key === 'Escape' && onClose();
    document.body.style.overflow = 'hidden';
    window.addEventListener('keydown', onKey);
    return () => { document.body.style.overflow = ''; window.removeEventListener('keydown', onKey); };
  }, [onClose]);

  const print = () => {
    const popup = window.open('', '_blank', 'width=560,height=760');
    if (!popup || !ticketRef.current) return;
    popup.document.write(`<html><head><title>${code}</title><style>body{font-family:"Microsoft YaHei",sans-serif;padding:32px;color:#173239}.ticket-code-card{max-width:440px;margin:auto}.ticket-code-toolbar,.ticket-code-hint{display:none}.ticket-code-qr{margin:24px 0}svg{vertical-align:middle}</style></head><body>${ticketRef.current.outerHTML}</body></html>`);
    popup.document.close(); popup.focus(); popup.print(); popup.close();
  };

  return <div className="ticket-code-backdrop" onMouseDown={event => event.target === event.currentTarget && onClose()}>
    <section className="ticket-code-shell" role="dialog" aria-modal="true" aria-labelledby="ticket-code-title">
      <div className="ticket-code-toolbar"><div><QrCode size={18}/><span>电子入场码</span></div><div><button onClick={print}><Download size={16}/>打印</button><button onClick={onClose} aria-label="关闭票码"><X size={20}/></button></div></div>
      <div className="ticket-code-card" ref={ticketRef}>
        <div className="ticket-code-status"><TicketCheck size={18}/><span>支付成功 · 可入场</span></div>
        <h2 id="ticket-code-title">{order.title}</h2>
        <div className="ticket-code-info"><span><CalendarDays size={15}/>{order.event_date || '日期待定'}</span><span><MapPin size={15}/>{order.venue || '场馆待定'}</span>{order.seat_label && <strong>{order.seat_label} · {order.seat_tier}</strong>}</div>
        <div className="ticket-code-divider"><i/><span>入场核验</span><i/></div>
        <div className="ticket-code-qr"><QRCodeCanvas value={code} size={176} level="M" marginSize={2}/><div><b>{code}</b><span>数量：{order.quantity} 张</span><small>请向现场工作人员出示此码</small></div></div>
        <p className="ticket-code-hint">票码仅供本人使用，请勿转发或截图分享。</p>
      </div>
    </section>
  </div>;
}
