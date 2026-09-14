import { useState, useEffect } from 'react';
import { useNavigate } from 'react-router-dom';
import { Calendar, MapPin, Clock, CheckCircle, XCircle, AlertCircle } from 'lucide-react';
import { orderApi } from '../../api/orders';
import type { Order } from '../../types';
import './OrderList.css';

const OrderList = () => {
  const navigate = useNavigate();
  const [orders, setOrders] = useState<Order[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState('');
  const [cancellingId, setCancellingId] = useState<number | null>(null);
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
        setError(err instanceof Error ? err.message : '加载订单失败，请重试');
      } finally {
        setLoading(false);
      }
    };
    loadOrders();
  }, [navigate]);

  const handlePay = async (order: Order) => {
    const token = localStorage.getItem('token');
    if (!token) return;
    try {
      const p = await orderApi.payAndWait(token, order.id);
      if (p.payment_status === 'SUCCESS') {
        setOrders(prev => prev.map(o => o.id === order.id ? { ...o, status: 'CONFIRMED' as const, expire_at: '' } : o));
      } else {
        setActionMsg(p.payment_status === 'FAILED' ? '支付失败，请重试' : '订单已失效，金额已退款');
        orderApi.getMyOrders(token).then(setOrders).catch(() => {});
      }
    } catch {
      // 结果未知或订单可能已超时回收，重新拉取
      orderApi.getMyOrders(token).then(setOrders).catch(() => {});
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
      setActionMsg(err instanceof Error ? err.message : '取消失败');
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
        <h1 className="order-list-title">我的订单</h1>
        <div className="order-action-msg error">
          {error}
          <button
            style={{ marginLeft: '12px', cursor: 'pointer', background: 'none', border: 'none', color: 'inherit', textDecoration: 'underline' }}
            onClick={() => window.location.reload()}
          >重试</button>
        </div>
      </div>
    );
  }

  return (
    <div className="order-list-container">
      <h1 className="order-list-title">我的订单</h1>

      {actionMsg && (
        <div className={`order-action-msg ${actionMsg.includes('已取消') ? 'success' : 'error'}`}>
          {actionMsg}
        </div>
      )}

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
                      <button className="order-btn order-btn-primary" onClick={() => handlePay(order)}>
                        去支付
                      </button>
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
