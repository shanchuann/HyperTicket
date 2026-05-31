import { useState, useEffect } from 'react';
import { Calendar, MapPin, Clock, CheckCircle, XCircle, AlertCircle } from 'lucide-react';
import './OrderList.css';

interface Order {
  id: number;
  ticket_title: string;
  venue: string;
  event_date: string;
  quantity: number;
  total_price: number;
  status: 'PENDING' | 'CONFIRMED' | 'CANCELLED' | 'EXPIRED';
  created_at: string;
}

const OrderList = () => {
  const [orders, setOrders] = useState<Order[]>([]);
  const [loading, setLoading] = useState(true);

  // 模拟数据
  const mockOrders: Order[] = [
    {
      id: 1,
      ticket_title: '2026 周杰伦演唱会',
      venue: '国家体育场（鸟巢）',
      event_date: '2026-08-15',
      quantity: 2,
      total_price: 1160,
      status: 'CONFIRMED',
      created_at: '2026-05-20T10:30:00'
    },
    {
      id: 2,
      ticket_title: '故宫博物院',
      venue: '故宫博物院',
      event_date: '2026-06-10',
      quantity: 1,
      total_price: 60,
      status: 'PENDING',
      created_at: '2026-05-28T14:20:00'
    },
    {
      id: 3,
      ticket_title: '2026 中超联赛',
      venue: '工人体育场',
      event_date: '2026-06-20',
      quantity: 2,
      total_price: 240,
      status: 'CANCELLED',
      created_at: '2026-05-15T09:00:00'
    }
  ];

  useEffect(() => {
    const loadOrders = async () => {
      setLoading(true);
      await new Promise(resolve => setTimeout(resolve, 600));
      setOrders(mockOrders);
      setLoading(false);
    };
    loadOrders();
  }, []);

  const formatDate = (dateString: string) => {
    const date = new Date(dateString);
    return date.toLocaleDateString('zh-CN', {
      year: 'numeric',
      month: 'long',
      day: 'numeric',
      weekday: 'short'
    });
  };

  const formatDateTime = (dateString: string) => {
    const date = new Date(dateString);
    return date.toLocaleDateString('zh-CN', {
      month: 'short',
      day: 'numeric',
      hour: '2-digit',
      minute: '2-digit'
    });
  };

  const getStatusConfig = (status: Order['status']) => {
    const configs = {
      PENDING: {
        label: '待确认',
        icon: Clock,
        color: 'var(--color-warning)',
        bgColor: 'oklch(0.90 0.12 95 / 0.1)'
      },
      CONFIRMED: {
        label: '已确认',
        icon: CheckCircle,
        color: 'var(--color-success)',
        bgColor: 'oklch(0.77 0.12 65 / 0.1)'
      },
      CANCELLED: {
        label: '已取消',
        icon: XCircle,
        color: 'var(--color-error)',
        bgColor: 'oklch(0.62 0.15 35 / 0.1)'
      },
      EXPIRED: {
        label: '已过期',
        icon: AlertCircle,
        color: 'var(--color-text-tertiary)',
        bgColor: 'oklch(0.65 0.015 210 / 0.1)'
      }
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

  return (
    <div className="order-list-container">
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
                    <span className="order-date">
                      创建于 {formatDateTime(order.created_at)}
                    </span>
                  </div>
                  <div
                    className="order-status"
                    style={{
                      color: statusConfig.color,
                      background: statusConfig.bgColor
                    }}
                  >
                    <StatusIcon size={14} />
                    <span>{statusConfig.label}</span>
                  </div>
                </div>

                <div className="order-content">
                  <h3 className="order-title">{order.ticket_title}</h3>

                  <div className="order-details">
                    <div className="order-detail">
                      <Calendar size={14} />
                      <span>{formatDate(order.event_date)}</span>
                    </div>
                    <div className="order-detail">
                      <MapPin size={14} />
                      <span>{order.venue}</span>
                    </div>
                  </div>
                </div>

                <div className="order-footer">
                  <div className="order-quantity">
                    数量: <strong>{order.quantity}</strong> 张
                  </div>
                  <div className="order-total">
                    <span className="order-total-label">总计</span>
                    <span className="order-total-value">
                      ¥{order.total_price}
                    </span>
                  </div>
                </div>

                {order.status === 'PENDING' && (
                  <div className="order-actions">
                    <button className="order-btn order-btn-secondary">
                      取消订单
                    </button>
                    <button className="order-btn order-btn-primary">
                      立即支付
                    </button>
                  </div>
                )}

                {order.status === 'CONFIRMED' && (
                  <div className="order-actions">
                    <button className="order-btn order-btn-secondary">
                      查看详情
                    </button>
                    <button className="order-btn order-btn-danger">
                      申请退款
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
