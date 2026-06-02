import { useState, useEffect } from 'react';
import { Ticket, Users, ShoppingCart, TrendingUp } from 'lucide-react';
import { adminApi } from '../../api/admin';
import type { AdminStats, AdminTicket } from '../../api/admin';
import './Dashboard.css';

const Dashboard = () => {
  const [stats, setStats] = useState<AdminStats>({ user_count: 0, ticket_count: 0, order_count: 0, today_orders: 0 });
  const [tickets, setTickets] = useState<AdminTicket[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState('');

  useEffect(() => {
    const load = async () => {
      setLoading(true);
      setError('');
      try {
        const [s, t] = await Promise.all([adminApi.stats(), adminApi.listTickets()]);
        setStats(s);
        setTickets(t);
      } catch (e) {
        setError(e instanceof Error ? e.message : '加载失败');
      } finally {
        setLoading(false);
      }
    };
    load();
  }, []);

  const statCards = [
    { title: '在售票务', value: stats.ticket_count, icon: Ticket,      color: 'var(--color-primary)' },
    { title: '注册用户', value: stats.user_count,   icon: Users,       color: 'var(--color-accent)' },
    { title: '累计订单', value: stats.order_count,  icon: ShoppingCart, color: 'var(--color-success)' },
    { title: '今日订单', value: stats.today_orders, icon: TrendingUp,  color: 'var(--color-warning)' },
  ];

  // 库存不足：剩余不足 20% 的票务
  const lowStockTickets = tickets
    .filter(t => t.status === 1 && t.total_seats > 0)
    .map(t => ({ ...t, percent: ((t.available_seats / t.total_seats) * 100) }))
    .filter(t => t.percent < 20)
    .sort((a, b) => a.percent - b.percent)
    .slice(0, 5);

  const formatNumber = (n: number) => n.toLocaleString('zh-CN');

  if (loading) {
    return (
      <div className="admin-dashboard">
        <div className="dashboard-loading">
          <div className="loading-spinner" />
          <p>加载中...</p>
        </div>
      </div>
    );
  }

  if (error) {
    return (
      <div className="admin-dashboard">
        <div className="dashboard-error">
          <p>加载失败：{error}</p>
          <button onClick={() => window.location.reload()}>重试</button>
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
            </div>
          </div>
        ))}
      </div>

      <div className="dashboard-grid">
        {/* 所有票务列表 */}
        <div className="dashboard-section">
          <h2 className="section-title">票务列表</h2>
          <div className="orders-table-wrapper">
            <table className="orders-table">
              <thead>
                <tr>
                  <th>ID</th>
                  <th>名称</th>
                  <th>场馆</th>
                  <th>日期</th>
                  <th>剩余/总量</th>
                  <th>状态</th>
                </tr>
              </thead>
              <tbody>
                {tickets.length === 0 ? (
                  <tr><td colSpan={6} style={{ textAlign: 'center', color: 'var(--color-text-tertiary)' }}>暂无票务</td></tr>
                ) : tickets.map(t => (
                  <tr key={t.ticket_id}>
                    <td className="order-id">#{t.ticket_id}</td>
                    <td>{t.title}</td>
                    <td>{t.venue}</td>
                    <td className="order-time">{t.event_date}</td>
                    <td>{formatNumber(t.available_seats)} / {formatNumber(t.total_seats)}</td>
                    <td>
                      <span className={`order-status ${t.status === 1 ? 'status-confirmed' : 'status-cancelled'}`}>
                        {t.status === 1 ? '在售' : '下架'}
                      </span>
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </div>

        {/* 库存预警 */}
        <div className="dashboard-section">
          <h2 className="section-title">库存预警（剩余 &lt;20%）</h2>
          {lowStockTickets.length === 0 ? (
            <p style={{ color: 'var(--color-text-tertiary)', padding: 'var(--space-4) 0' }}>
              所有票务库存充足
            </p>
          ) : (
            <div className="stock-list">
              {lowStockTickets.map(t => (
                <div key={t.ticket_id} className="stock-item">
                  <div className="stock-info">
                    <p className="stock-title">{t.title}</p>
                    <p className="stock-count">
                      剩余 {formatNumber(t.available_seats)} / {formatNumber(t.total_seats)}
                    </p>
                  </div>
                  <div className="stock-bar">
                    <div className="stock-fill" style={{ width: `${t.percent.toFixed(1)}%` }} />
                  </div>
                  <span className="stock-percent">{t.percent.toFixed(1)}%</span>
                </div>
              ))}
            </div>
          )}
        </div>
      </div>
    </div>
  );
};

export default Dashboard;
