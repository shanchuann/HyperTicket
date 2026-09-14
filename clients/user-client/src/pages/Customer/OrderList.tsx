import { useState, useEffect } from 'react';
import { useNavigate } from 'react-router-dom';
import { Calendar, MapPin, Clock, CheckCircle, XCircle, AlertCircle, Armchair, QrCode } from 'lucide-react';
import { orderApi } from '../../api/orders';
import { toChineseError } from '../../api/errors';
import type { Order } from '../../types';
import Toast from '../../components/Toast';
import TicketCode from '../../components/TicketCode';
import './OrderList.css';

const OrderList = () => {
  const navigate = useNavigate();
  const [orders, setOrders] = useState<Order[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState('');
  const [cancellingId, setCancellingId] = useState<number | null>(null);
  const [payingId, setPayingId] = useState<number | null>(null);
  const [codeOrder, setCodeOrder] = useState<Order | null>(null);
  const [actionMsg, setActionMsg] = useState('');

  useEffect(() => {
    const loadOrders = async () => {
      const token = localStorage.getItem('token');
      if (!token) {
        navigate('/auth/login', { replace: true });
        return;
      }
      setLoading(true);
      setError('');
      try {
        const data = await orderApi.getMyOrders(token);
        setOrders(data);
      } catch (err) {
        setError(toChineseError(err, '加载订单失败，请重试'));
      } finally {
        setLoading(false);
      }
    };
    loadOrders();
  }, [navigate]);

  const handlePay = async (order: Order) => {
    const token = localStorage.getItem('token');
    if (!token) return;
    setPayingId(order.id);
    setActionMsg('');
    try {
      const p = await orderApi.payAndWait(token, order.id);
      if (p.payment_status === 'SUCCESS') {
        const fallback = { ...order, status: 'CONFIRMED' as const, expire_at: '' };
        const refreshed = await orderApi.getMyOrders(token).catch(() => null);
        if (refreshed) setOrders(refreshed);
        else setOrders(prev => prev.map(o => o.id === order.id ? fallback : o));
        setActionMsg('支付成功，电子入场码已生成');
        setCodeOrder(refreshed?.find(item => item.id === order.id) || fallback);
      } else {
        setActionMsg(p.payment_status === 'FAILED' ? '支付失败，请重试' : '订单已失效，金额已退款');
        orderApi.getMyOrders(token).then(setOrders).catch(() => {});
      }
    } catch {
      // 结果未知或订单可能已超时回收，重新拉取
      orderApi.getMyOrders(token).then(setOrders).catch(() => {});
      setActionMsg('支付结果仍在确认中，已为你刷新订单状态');
    } finally {
      setPayingId(null);
    }
  };

  const handleCancel = async (order: Order) => {
    const token = localStorage.getItem('token');
    if (!token) return;
    setCancellingId(order.id);
    setActionMsg('');
    try {
      await orderApi.cancelOrder(token, order.id);
      setActionMsg(`订单 #${order.id} 已取消`);
      setOrders(prev =>
        prev.map(o => o.id === order.id ? { ...o, status: 'CANCELLED' } : o)
      );
    } catch (err) {
      setActionMsg(toChineseError(err, '取消失败'));
    } finally {
      setCancellingId(null);
    }
  };

  const formatDate = (dateString: string) => {
    if (!dateString) return '—';
    const date = new Date(dateString);
    return date.toLocaleDateString('zh-CN', {
      year: 'numeric',
      month: 'long',
      day: 'numeric',
      weekday: 'short',
    });
  };

  const getStatusConfig = (status: Order['status']) => {
    const configs: Record<Order['status'], { label: string; icon: typeof Clock; color: string; bgColor: string }> = {
      PENDING: {
        label: '待支付',
        icon: Clock,
        color: 'var(--color-warning)',
        bgColor: 'oklch(0.90 0.12 95 / 0.1)',
      },
      CONFIRMED: {
        label: '已确认',
        icon: CheckCircle,
        color: 'var(--color-success)',
        bgColor: 'oklch(0.77 0.12 65 / 0.1)',
      },
      CANCELLED: {
        label: '已取消',
        icon: XCircle,
        color: 'var(--color-error)',
        bgColor: 'oklch(0.62 0.15 35 / 0.1)',
      },
      EXPIRED: {
        label: '已过期',
        icon: AlertCircle,
        color: 'var(--color-text-tertiary)',
        bgColor: 'oklch(0.65 0.015 210 / 0.1)',
      },
    };
    return configs[status];
  };

  if (loading) {
    return (
      <div className="order-list-container">
        <div className="order-list-loading">
          <div className="loading-spinner"></div>
          <p>加载中...</p>
        </div>
      </div>
    );
  }

  if (error) {
    return (
      <div className="order-list-container">
        <Toast message={error} tone="error" onClose={() => setError('')} actionLabel="重新加载" onAction={() => window.location.reload()} />
        <h1 className="order-list-title">我的订单</h1>
        <div className="order-list-empty"><p>暂时无法读取订单</p><button className="order-browse-btn" onClick={() => window.location.reload()}>重新加载</button></div>
      </div>
    );
  }

  return (
    <div className="order-list-container">
      <Toast message={actionMsg} tone={actionMsg.includes('已取消') || actionMsg.includes('成功') ? 'success' : 'error'} onClose={() => setActionMsg('')} />
      {codeOrder && <TicketCode order={codeOrder} onClose={() => setCodeOrder(null)} />}
      <h1 className="order-list-title">我的订单</h1>

      {orders.length === 0 ? (
        <div className="order-list-empty">
          <p>暂无订单</p>
          <a href="/customer" className="order-browse-btn">去浏览票务</a>
        </div>
      ) : (
        <div className="order-list">
          {orders.map(order => {
            const statusConfig = getStatusConfig(order.status);
            const StatusIcon = statusConfig.icon;

            return (
              <div key={order.id} className="order-card">
                <div className="order-header">
                  <div className="order-info">
                    <span className="order-id">订单 #{order.id}</span>
                  </div>
                  <div
                    className="order-status"
                    style={{ color: statusConfig.color, background: statusConfig.bgColor }}
                  >
                    <StatusIcon size={14} />
                    <span>{statusConfig.label}</span>
                  </div>
                </div>

                <div className="order-content">
                  <h3 className="order-title">{order.title}</h3>

                  <div className="order-details">
                    {order.event_date && (
                      <div className="order-detail">
                        <Calendar size={14} />
                        <span>{formatDate(order.event_date)}</span>
                      </div>
                    )}
                    {order.venue && (
                      <div className="order-detail">
                        <MapPin size={14} />
                        <span>{order.venue}</span>
                      </div>
                    )}
                    {order.seat_label && <div className="order-detail"><Armchair size={14}/><span>{order.seat_label} · {order.seat_tier}</span></div>}
                  </div>
                </div>

                <div className="order-footer">
                  <div className="order-quantity">
                    数量: <strong>{order.quantity}</strong> 张
                  </div>
                </div>

                {(order.status === 'PENDING' || order.status === 'CONFIRMED') && (
                  <div className="order-actions">
                    {order.status === 'PENDING' && (
                      <button className="order-btn order-btn-primary" disabled={payingId === order.id} onClick={() => handlePay(order)}>
                        {payingId === order.id ? '支付确认中…' : '去支付'}
                      </button>
                    )}
                    {order.status === 'CONFIRMED' && (
                      <button className="order-btn order-btn-primary" onClick={() => setCodeOrder(order)}><QrCode size={16}/>查看入场码</button>
                    )}
                    <button
                      className="order-btn order-btn-secondary"
                      disabled={cancellingId === order.id}
                      onClick={() => handleCancel(order)}
                    >
                      {cancellingId === order.id ? '取消中...' : '取消订单'}
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
