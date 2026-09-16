import { useEffect, useMemo, useRef, useState } from 'react';
import { CheckCircle2, Loader2, ScanLine, ShieldCheck, X } from 'lucide-react';
import { QRCodeCanvas } from 'qrcode.react';
import { orderApi, type PaymentResponse } from '../api/orders';
import { toChineseError } from '../api/errors';
import type { Order } from '../types';
import './PaymentDialog.css';

type Props = {
  order: Order;
  onClose: () => void;
  onPaid: (order: Order) => void;
  onError: (message: string) => void;
};

const paymentKey = (orderId: number) => {
  const random = globalThis.crypto?.randomUUID?.().replaceAll('-', '') || `${Date.now()}${Math.random().toString(36).slice(2)}`;
  return `mock_${orderId}_${random}`.slice(0, 64);
};

export default function PaymentDialog({ order, onClose, onPaid, onError }: Props) {
  const [payment, setPayment] = useState<PaymentResponse | null>(null);
  const [phase, setPhase] = useState<'loading' | 'ready' | 'scanning' | 'paid'>('loading');
  const started = useRef(false);
  const idempotencyKey = useRef(paymentKey(order.id));
  const fallbackMinor = (order.seat_price || order.ticket_price * order.quantity) * 100;

  useEffect(() => {
    if (started.current) return;
    started.current = true;
    const token = localStorage.getItem('token');
    if (!token) { onError('登录状态已失效，请重新登录'); onClose(); return; }
    const initialize = async () => {
      try {
        let current: PaymentResponse;
        try { current = await orderApi.queryPayment(token, order.id); }
        catch { current = await orderApi.payOrder(token, order.id, 'MOCK', idempotencyKey.current); }
        setPayment(current);
        if (current.payment_status === 'SUCCEEDED') setPhase('paid');
        else setPhase('ready');
      } catch (error) {
        onError(toChineseError(error, '模拟支付单创建失败'));
        onClose();
      }
    };
    void initialize();
  }, [onClose, onError, order.id]);

  useEffect(() => {
    const onKey = (event: KeyboardEvent) => event.key === 'Escape' && phase !== 'scanning' && onClose();
    document.body.style.overflow = 'hidden';
    window.addEventListener('keydown', onKey);
    return () => { document.body.style.overflow = ''; window.removeEventListener('keydown', onKey); };
  }, [onClose, phase]);

  const amountMinor = payment?.amount_minor ?? fallbackMinor;
  const qrPayload = useMemo(() => JSON.stringify({
    scheme: 'hyperticket-mock-pay',
    payment_no: payment?.payment_no || `PENDING-${order.id}`,
    amount_minor: amountMinor,
    currency: payment?.currency || 'CNY',
    provider: 'MOCK',
  }), [amountMinor, order.id, payment]);

  const simulateScan = async () => {
    const token = localStorage.getItem('token');
    if (!token) return;
    setPhase('scanning');
    try {
      const result = await orderApi.waitForPayment(token, order.id);
      setPayment(result);
      if (result.payment_status !== 'SUCCEEDED') throw new Error(result.payment_status === 'FAILED' ? '模拟支付失败' : '订单已关闭');
      setPhase('paid');
      const orders = await orderApi.getMyOrders(token).catch(() => []);
      const paidOrder = orders.find(item => item.id === order.id) || { ...order, status: 'CONFIRMED' as const, expire_at: '' };
      window.setTimeout(() => onPaid(paidOrder), 350);
    } catch (error) {
      setPhase('ready');
      onError(toChineseError(error, '支付结果确认失败，请稍后重试'));
    }
  };

  return <div className="payment-backdrop" onMouseDown={event => event.target === event.currentTarget && phase !== 'scanning' && onClose()}>
    <section className="payment-dialog" role="dialog" aria-modal="true" aria-labelledby="payment-title">
      <header>
        <div><span className="payment-mock-tag">MOCK 模拟通道</span><h2 id="payment-title">扫码支付</h2></div>
        <button type="button" aria-label="关闭支付" onClick={onClose} disabled={phase === 'scanning'}><X size={20}/></button>
      </header>

      <div className="payment-order-line"><span>{order.title}</span><b>订单 {order.order_no || `#${order.id}`}</b></div>
      <div className="payment-amount"><span>应付金额</span><strong><small>¥</small>{(amountMinor / 100).toFixed(2)}</strong></div>

      <div className={`payment-qr ${phase}`}>
        {phase === 'loading' ? <Loader2 className="spinner" size={42}/> : phase === 'paid' ? <CheckCircle2 size={72}/> : <QRCodeCanvas value={qrPayload} size={188} level="M" marginSize={2}/>} 
        {phase !== 'loading' && phase !== 'paid' && <i><ScanLine size={17}/>模拟二维码</i>}
      </div>

      <div className="payment-status" aria-live="polite">
        {phase === 'loading' && '正在创建模拟支付单'}
        {phase === 'ready' && '等待模拟扫码'}
        {phase === 'scanning' && <><Loader2 className="spinner" size={16}/>已扫描，正在返回订单</>}
        {phase === 'paid' && <><CheckCircle2 size={16}/>支付成功，正在出票</>}
      </div>

      <button className="payment-scan-button" type="button" disabled={phase !== 'ready'} onClick={simulateScan}>
        <ScanLine size={18}/>{phase === 'ready' ? '模拟扫码' : phase === 'paid' ? '支付完成' : '处理中'}
      </button>
      <p className="payment-security"><ShieldCheck size={15}/>仅用于开发测试，不会发起真实扣款</p>
    </section>
  </div>;
}
