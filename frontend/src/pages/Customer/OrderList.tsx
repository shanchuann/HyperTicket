import { useState, useEffect } from 'react';
import { useNavigate } from 'react-router-dom';
import { Calendar, MapPin, Clock, CheckCircle, XCircle, AlertCircle, Trash2, ChevronRight, CreditCard } from 'lucide-react';
import { orderApi } from '../../api/orders';
import type { Order } from '../../types';
import { catchError } from '../../utils/errors';
import { useToast } from '../../components/ToastContext';
import OrderDetail from '../../components/OrderDetail';
import './OrderList.css';

const OrderList = () => {
  const navigate = useNavigate();
  const toast = useToast();
  const [orders, setOrders] = useState<Order[]>([]);
  const [loading, setLoading] = useState(true);
  const [cancellingId, setCancellingId] = useState<number | null>(null);
  const [deletingId, setDeletingId] = useState<number | null>(null);
  const [payingId, setPayingId] = useState<number | null>(null);
  const [detailOrder, setDetailOrder] = useState<Order | null>(null);
  const [now, setNow] = useState(() => Date.now());

  // 有待支付订单时每秒刷新倒计时
  useEffect(() => {
    if (!orders.some(o => o.status === 'PENDING' && o.expire_at)) return;
    const timer = setInterval(() => setNow(Date.now()), 1000);
    return () => clearInterval(timer);
  }, [orders]);

  useEffect(() => {
    const loadOrders = async () => {
      const token = localStorage.getItem('token');
      if (!token) { navigate('/auth/login', { replace: true }); return; }
      setLoading(true);
      try {
        setOrders(await orderApi.getMyOrders(token));
      } catch (err) {
        toast.error(catchError(err, '加载订单失败，请重试'));
      } finally {
        setLoading(false);
      }
    };
    loadOrders();
  }, [navigate]); // eslint-disable-line react-hooks/exhaustive-deps

  const handleCancel = async (order: Order) => {
    const token = localStorage.getItem('token');
    if (!token) return;
    setCancellingId(order.id);
    try {
      await orderApi.cancelOrder(token, order.id);
      toast.success(`订单 #${order.id} 已取消`);
      setOrders(prev => prev.map(o => o.id === order.id ? { ...o, status: 'CANCELLED' } : o));
    } catch (err) {
      toast.error(catchError(err, '取消失败'));
    } finally {
      setCancellingId(null);
    }
  };

  const handleDelete = async (order: Order) => {
    const token = localStorage.getItem('token');
    if (!token) return;
    setDeletingId(order.id);
    try {
      await orderApi.deleteOrder(token, order.id);
      setOrders(prev => prev.filter(o => o.id !== order.id));
      toast.success('记录已删除');
    } catch (err) {
      toast.error(catchError(err, '删除失败'));
    } finally {
      setDeletingId(null);
    }
  };

  // v3 异步支付：发起支付创建流水，模拟网关延迟结算，轮询至终态
  const handlePay = async (order: Order) => {
    const token = localStorage.getItem('token');
    if (!token) return;
    setPayingId(order.id);
    try {
      const p = await orderApi.payAndWait(token, order.id);
      if (p.payment_status === 'SUCCESS') {
        toast.success(`订单 #${order.id} 支付成功`);
        setOrders(prev => prev.map(o => o.id === order.id ? { ...o, status: 'CONFIRMED', expire_at: '' } : o));
      } else if (p.payment_status === 'FAILED') {
        toast.error('支付失败（网关拒绝），可在支付截止前重试');
      } else {
        // REFUNDED：订单已超时回收/已取消，网关扣款自动退回
        toast.error('订单已失效，支付金额已自动退款');
        orderApi.getMyOrders(token).then(setOrders).catch(() => {});
      }
    } catch (err) {
      toast.error(catchError(err, '支付失败，订单可能已超时'));
      // 结果未知或订单可能已被回收：重新拉取同步状态
      orderApi.getMyOrders(token).then(setOrders).catch(() => {});
    } finally {
      setPayingId(null);
    }
  };

  // PENDING 支付倒计时（mm:ss），超时显示 00:00 等待后台回收
  const payCountdown = (order: Order): string => {
    if (!order.expire_at) return '';
    const remain = new Date(order.expire_at.replace(' ', 'T')).getTime() - now;
    if (remain <= 0) return '00:00';
    const m = Math.floor(remain / 60000);
    const s = Math.floor((remain % 60000) / 1000);
    return `${String(m).padStart(2, '0')}:${String(s).padStart(2, '0')}`;
  };

  const formatDate = (dateString: string) => {
    if (!dateString) return '—';
    return new Date(dateString).toLocaleDateString('zh-CN', {
      year: 'numeric', month: 'long', day: 'numeric', weekday: 'short',
    });
  };

  const getStatusConfig = (status: Order['status']) => {
    const configs: Record<Order['status'], { label: string; icon: typeof Clock; color: string; bgColor: string }> = {
      PENDING:   { label: '待支付', icon: Clock,        color: 'var(--color-warning)',       bgColor: 'oklch(0.90 0.12 95 / 0.1)' },
      CONFIRMED: { label: '已确认', icon: CheckCircle,  color: 'var(--color-success)',       bgColor: 'oklch(0.77 0.12 65 / 0.1)' },
      CANCELLED: { label: '已取消', icon: XCircle,      color: 'var(--color-error)',         bgColor: 'oklch(0.62 0.15 35 / 0.1)' },
      EXPIRED:   { label: '已过期', icon: AlertCircle,  color: 'var(--color-text-tertiary)', bgColor: 'oklch(0.65 0.015 210 / 0.1)' },
    };
    return configs[status];
  };

  if (loading) {
    return (
      <div className="order-list-container">
        <div className="order-list-loading">
          <div className="loading-spinner" /><p>加载中...</p>
        </div>
      </div>
    );
  }

  return (
    <div className="order-list-container">
      <h1 className="order-list-title">我的订单</h1>

      {detailOrder && (
        <OrderDetail order={detailOrder} onClose={() => setDetailOrder(null)} />
      )}
      {orders.length === 0 ? (
        <div className="order-list-empty">
          <p>暂无订单</p>
          <a href="/customer" className="order-browse-btn">去浏览票务</a>
        </div>
      ) : (
        <div className="order-list">
          {orders.map(order => {
            const cfg = getStatusConfig(order.status);
            const Icon = cfg.icon;
            return (
              <div key={order.id} className="order-card order-card-clickable"
                onClick={() => setDetailOrder(order)}>
                <div className="order-header">
                  <span className="order-id">订单 #{order.id}</span>
                  <div style={{ display: 'flex', alignItems: 'center', gap: 'var(--space-2)' }}>
                    <div className="order-status" style={{ color: cfg.color, background: cfg.bgColor }}>
                      <Icon size={14} /><span>{cfg.label}</span>
                    </div>
                    <ChevronRight size={16} style={{ color: 'var(--color-text-tertiary)' }} />
                  </div>
                </div>

                <div className="order-content">
                  <h3 className="order-title">{order.title}</h3>
                  <div className="order-details">
                    {order.event_date && (
                      <div className="order-detail"><Calendar size={14} /><span>{formatDate(order.event_date)}</span></div>
                    )}
                    {order.venue && (
                      <div className="order-detail"><MapPin size={14} /><span>{order.venue}</span></div>
                    )}
                  </div>
                </div>

                <div className="order-footer">
                  <div className="order-quantity">数量: <strong>{order.quantity}</strong> 张</div>
                  {(order.seat_price > 0 || order.ticket_price > 0) && (
                    <div className="order-quantity">
                      总价: <strong>￥{order.seat_price > 0 ? order.seat_price : order.ticket_price * order.quantity}</strong>
                    </div>
                  )}
                  {order.status === 'PENDING' && order.expire_at && (
                    <div className="order-countdown">
                      <Clock size={13} /><span>支付剩余 {payCountdown(order)}</span>
                    </div>
                  )}
                </div>

                {(order.status === 'PENDING' || order.status === 'CONFIRMED') && (
                  <div className="order-actions" onClick={e => e.stopPropagation()}>
                    {order.status === 'PENDING' && (
                      <button className="order-btn order-btn-pay"
                        disabled={payingId === order.id} onClick={() => handlePay(order)}>
                        <CreditCard size={14} />
                        {payingId === order.id ? '支付中...' : '去支付'}
                      </button>
                    )}
                    <button className="order-btn order-btn-secondary"
                      disabled={cancellingId === order.id} onClick={() => handleCancel(order)}>
                      {cancellingId === order.id ? '取消中...' : '取消订单'}
                    </button>
                  </div>
                )}
                {(order.status === 'CANCELLED' || order.status === 'EXPIRED') && (
                  <div className="order-actions" onClick={e => e.stopPropagation()}>
                    <button className="order-btn order-btn-delete"
                      disabled={deletingId === order.id} onClick={() => handleDelete(order)} title="删除此记录">
                      <Trash2 size={14} />
                      {deletingId === order.id ? '删除中...' : '删除记录'}
                    </button>
                  </div>
                )}
              </div>
            );
          })}
        </div>
      )}
    </div>
  );
};

export default OrderList;
