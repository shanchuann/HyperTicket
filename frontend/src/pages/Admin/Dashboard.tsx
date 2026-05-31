import { useState, useEffect } from 'react';
import { Ticket, Users, ShoppingCart, TrendingUp, AlertCircle } from 'lucide-react';
import './Dashboard.css';

interface DashboardStats {
  totalTickets: number;
  totalUsers: number;
  totalOrders: number;
  todayOrders: number;
}

const Dashboard = () => {
  const [stats, setStats] = useState<DashboardStats>({
    totalTickets: 0,
    totalUsers: 0,
    totalOrders: 0,
    todayOrders: 0
  });
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    // 模拟 API 调用
    const loadStats = async () => {
      setLoading(true);
      await new Promise(resolve => setTimeout(resolve, 500));
      setStats({
        totalTickets: 156,
        totalUsers: 2847,
        totalOrders: 12580,
        todayOrders: 156
      });
      setLoading(false);
    };
    loadStats();
  }, []);

  const statCards = [
    {
      title: '票务总数',
      value: stats.totalTickets,
      icon: Ticket,
      trend: '+12',
      trendUp: true,
      color: 'var(--color-primary)'
    },
    {
      title: '用户总数',
      value: stats.totalUsers,
      icon: Users,
      trend: '+5.3%',
      trendUp: true,
      color: 'var(--color-accent)'
    },
    {
      title: '订单总数',
      value: stats.totalOrders,
      icon: ShoppingCart,
      trend: '+8.1%',
      trendUp: true,
      color: 'var(--color-success)'
    },
    {
      title: '今日订单',
      value: stats.todayOrders,
      icon: TrendingUp,
      trend: '-2',
      trendUp: false,
      color: 'var(--color-warning)'
    }
  ];

  const recentOrders = [
    { id: 12581, user: '张三', ticket: '周杰伦演唱会', amount: 1160, status: 'CONFIRMED', time: '10分钟前' },
    { id: 12580, user: '李四', ticket: '中超联赛', amount: 240, status: 'PENDING', time: '25分钟前' },
    { id: 12579, user: '王五', ticket: '故宫博物院', amount: 60, status: 'CONFIRMED', time: '1小时前' },
    { id: 12578, user: '赵六', ticket: '环球影城', amount: 418, status: 'CANCELLED', time: '2小时前' },
    { id: 12577, user: '钱七', ticket: '张学友演唱会', amount: 960, status: 'CONFIRMED', time: '3小时前' }
  ];

  const lowStockTickets = [
    { id: 1, title: '周杰伦演唱会', available: 1250, total: 80000, percent: 1.6 },
    { id: 2, title: '复仇者联盟5', available: 45, total: 200, percent: 22.5 },
    { id: 3, title: '张学友演唱会', available: 3200, total: 18000, percent: 17.8 }
  ];

  const formatNumber = (num: number) => {
    return num.toLocaleString('zh-CN');
  };

  const getStatusLabel = (status: string) => {
    const map: Record<string, string> = {
      CONFIRMED: '已确认',
      PENDING: '待确认',
      CANCELLED: '已取消'
    };
    return map[status] || status;
  };

  const getStatusClass = (status: string) => {
    const map: Record<string, string> = {
      CONFIRMED: 'status-confirmed',
      PENDING: 'status-pending',
      CANCELLED: 'status-cancelled'
    };
    return map[status] || '';
  };

  if (loading) {
    return (
      <div className="admin-dashboard">
        <div className="dashboard-loading">
          <div className="loading-spinner"></div>
          <p>加载中...</p>
        </div>
      </div>
    );
  }

  return (
    <div className="admin-dashboard">
      <h1 className="dashboard-title">管理概览</h1>

      {/* Stats Cards */}
      <div className="stats-grid">
        {statCards.map(card => (
          <div key={card.title} className="stat-card">
            <div className="stat-icon" style={{ background: `${card.color}15`, color: card.color }}>
              <card.icon size={24} />
            </div>
            <div className="stat-content">
              <p className="stat-title">{card.title}</p>
              <p className="stat-value">{formatNumber(card.value)}</p>
              <div className="stat-trend">
                <span className={card.trendUp ? 'trend-up' : 'trend-down'}>
                  {card.trendUp ? '↑' : '↓'} {card.trend}
                </span>
                <span className="trend-label">较上周</span>
              </div>
            </div>
          </div>
        ))}
      </div>

      <div className="dashboard-grid">
        {/* Recent Orders */}
        <div className="dashboard-section">
          <h2 className="section-title">最近订单</h2>
          <div className="orders-table-wrapper">
            <table className="orders-table">
              <thead>
                <tr>
                  <th>订单号</th>
                  <th>用户</th>
                  <th>票务</th>
                  <th>金额</th>
                  <th>状态</th>
                  <th>时间</th>
                </tr>
              </thead>
              <tbody>
                {recentOrders.map(order => (
                  <tr key={order.id}>
                    <td className="order-id">#{order.id}</td>
                    <td>{order.user}</td>
                    <td>{order.ticket}</td>
                    <td className="order-amount">¥{order.amount}</td>
                    <td>
                      <span className={`order-status ${getStatusClass(order.status)}`}>
                        {getStatusLabel(order.status)}
                      </span>
                    </td>
                    <td className="order-time">{order.time}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </div>

        {/* Low Stock Alert */}
        <div className="dashboard-section">
          <div className="section-header">
            <h2 className="section-title">库存预警</h2>
            <AlertCircle size={18} className="alert-icon" />
          </div>
          <div className="stock-list">
            {lowStockTickets.map(ticket => (
              <div key={ticket.id} className="stock-item">
                <div className="stock-info">
                  <p className="stock-title">{ticket.title}</p>
                  <p className="stock-count">
                    剩余 {formatNumber(ticket.available)} / {formatNumber(ticket.total)}
                  </p>
                </div>
                <div className="stock-bar">
                  <div
                    className="stock-fill"
                    style={{ width: `${ticket.percent}%` }}
                  />
                </div>
                <span className="stock-percent">{ticket.percent}%</span>
              </div>
            ))}
          </div>
        </div>
      </div>
    </div>
  );
};

export default Dashboard;
