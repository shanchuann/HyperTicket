import { useState, useEffect, useCallback } from 'react';
import { Plus, Search, Trash2, Calendar, MapPin, Loader2, X } from 'lucide-react';
import { adminApi } from '../../api/admin';
import type { AdminTicket } from '../../api/admin';
import './TicketManage.css';

const TicketManage = () => {
  const [tickets, setTickets] = useState<AdminTicket[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState('');
  const [searchQuery, setSearchQuery] = useState('');
  const [showAddModal, setShowAddModal] = useState(false);
  const [deleting, setDeleting] = useState<number | null>(null);

  const loadTickets = useCallback(async () => {
    setLoading(true);
    setError('');
    try {
      setTickets(await adminApi.listTickets());
    } catch (e) {
      setError(e instanceof Error ? e.message : '加载失败');
    } finally {
      setLoading(false);
    }
  }, []);

  useEffect(() => { loadTickets(); }, [loadTickets]);

  const handleDelete = async (ticketId: number, title: string) => {
    if (!window.confirm(`确认下架「${title}」？此操作不可撤销。`)) return;
    setDeleting(ticketId);
    try {
      await adminApi.deleteTicket(ticketId);
      setTickets(prev => prev.map(t => t.ticket_id === ticketId ? { ...t, status: 0 } : t));
    } catch (e) {
      alert(e instanceof Error ? e.message : '下架失败');
    } finally {
      setDeleting(null);
    }
  };

  const filtered = tickets.filter(t =>
    t.title.toLowerCase().includes(searchQuery.toLowerCase()) ||
    t.venue.toLowerCase().includes(searchQuery.toLowerCase())
  );

  const formatDate = (d: string) => {
    try { return new Date(d).toLocaleDateString('zh-CN'); } catch { return d; }
  };

  if (loading) {
    return (
      <div className="ticket-manage">
        <div className="ticket-manage-loading">
          <div className="loading-spinner" /><p>加载中...</p>
        </div>
      </div>
    );
  }

  return (
    <div className="ticket-manage">
      <div className="ticket-manage-header">
        <h1 className="ticket-manage-title">票务管理</h1>
        <button className="ticket-add-btn" onClick={() => setShowAddModal(true)}>
          <Plus size={18} /><span>添加票务</span>
        </button>
      </div>

      {error && (
        <div className="ticket-manage-error">
          {error}
          <button onClick={loadTickets}>重试</button>
        </div>
      )}

      <div className="ticket-manage-toolbar">
        <div className="ticket-manage-search">
          <Search size={18} className="search-icon" />
          <input
            type="text"
            placeholder="搜索票务名称或场馆..."
            value={searchQuery}
            onChange={e => setSearchQuery(e.target.value)}
          />
        </div>
        <span className="ticket-count-label">共 {filtered.length} 条</span>
      </div>

      <div className="ticket-table-wrapper">
        <table className="ticket-table">
          <thead>
            <tr>
              <th>ID</th>
              <th>票务名称</th>
              <th>场馆</th>
              <th>日期</th>
              <th>库存</th>
              <th>状态</th>
              <th>操作</th>
            </tr>
          </thead>
          <tbody>
            {filtered.length === 0 ? (
              <tr>
                <td colSpan={7} style={{ textAlign: 'center', color: 'var(--color-text-tertiary)', padding: 'var(--space-8)' }}>
                  暂无票务数据
                </td>
              </tr>
            ) : filtered.map(ticket => (
              <tr key={ticket.ticket_id}>
                <td className="ticket-id">#{ticket.ticket_id}</td>
                <td>
                  <span className="ticket-name">{ticket.title}</span>
                </td>
                <td>
                  <div className="venue-info">
                    <MapPin size={14} /><span>{ticket.venue}</span>
                  </div>
                </td>
                <td>
                  <div className="date-info">
                    <Calendar size={14} /><span>{formatDate(ticket.event_date)}</span>
                  </div>
                </td>
                <td>
                  <div className="stock-info">
                    <div className="stock-bar-mini">
                      <div
                        className="stock-fill-mini"
                        style={{
                          width: ticket.total_seats > 0
                            ? `${(ticket.available_seats / ticket.total_seats * 100).toFixed(1)}%`
                            : '0%'
                        }}
                      />
                    </div>
                    <span className="stock-text">
                      {ticket.available_seats.toLocaleString()} / {ticket.total_seats.toLocaleString()}
                    </span>
                  </div>
                </td>
                <td>
                  <span className={`status-badge ${ticket.status === 1 ? 'active' : 'inactive'}`}>
                    {ticket.status === 1 ? '在售' : '下架'}
                  </span>
                </td>
                <td>
                  <div className="ticket-actions">
                    {ticket.status === 1 && (
                      <button
                        className="action-btn action-btn-danger"
                        title="下架"
                        onClick={() => handleDelete(ticket.ticket_id, ticket.title)}
                        disabled={deleting === ticket.ticket_id}
                      >
                        {deleting === ticket.ticket_id
                          ? <Loader2 size={16} className="spinner" />
                          : <Trash2 size={16} />}
                      </button>
                    )}
                  </div>
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>

      {/* 添加票务弹窗 */}
      {showAddModal && (
        <AddTicketModal
          onClose={() => setShowAddModal(false)}
          onSuccess={() => { setShowAddModal(false); loadTickets(); }}
        />
      )}
    </div>
  );
};

// ===== 添加票务弹窗 =====
interface AddTicketModalProps {
  onClose: () => void;
  onSuccess: () => void;
}

const AddTicketModal = ({ onClose, onSuccess }: AddTicketModalProps) => {
  const [form, setForm] = useState({ title: '', venue: '', event_date: '', total_seats: '' });
  const [submitting, setSubmitting] = useState(false);
  const [error, setError] = useState('');

  const handleChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    setForm(prev => ({ ...prev, [e.target.name]: e.target.value }));
    setError('');
  };

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    const seats = parseInt(form.total_seats, 10);
    if (!form.title.trim() || !form.venue.trim() || !form.event_date) {
      setError('请填写完整信息');
      return;
    }
    if (isNaN(seats) || seats <= 0) {
      setError('座位数必须为正整数');
      return;
    }
    setSubmitting(true);
    setError('');
    try {
      await adminApi.addTicket(form.title.trim(), form.venue.trim(), form.event_date, seats);
      onSuccess();
    } catch (e) {
      setError(e instanceof Error ? e.message : '添加失败');
    } finally {
      setSubmitting(false);
    }
  };

  return (
    <div className="modal-overlay" onClick={e => e.target === e.currentTarget && onClose()}>
      <div className="modal-card">
        <div className="modal-header">
          <h2 className="modal-title">添加票务</h2>
          <button className="modal-close" onClick={onClose} aria-label="关闭"><X size={20} /></button>
        </div>

        {error && <div className="modal-error">{error}</div>}

        <form className="modal-form" onSubmit={handleSubmit}>
          <div className="form-group">
            <label className="form-label">票务名称</label>
            <input name="title" value={form.title} onChange={handleChange}
              className="form-input" placeholder="如：2026 周杰伦演唱会" disabled={submitting} />
          </div>
          <div className="form-group">
            <label className="form-label">场馆</label>
            <input name="venue" value={form.venue} onChange={handleChange}
              className="form-input" placeholder="如：国家体育场（鸟巢）" disabled={submitting} />
          </div>
          <div className="form-group">
            <label className="form-label">演出日期</label>
            <input type="date" name="event_date" value={form.event_date} onChange={handleChange}
              className="form-input" disabled={submitting} />
          </div>
          <div className="form-group">
            <label className="form-label">总座位数</label>
            <input type="number" name="total_seats" value={form.total_seats} onChange={handleChange}
              className="form-input" placeholder="如：80000" min="1" disabled={submitting} />
          </div>

          <div className="modal-actions">
            <button type="button" className="modal-btn-cancel" onClick={onClose} disabled={submitting}>取消</button>
            <button type="submit" className="modal-btn-submit" disabled={submitting}>
              {submitting ? <><Loader2 size={16} className="spinner" />提交中</> : '确认添加'}
            </button>
          </div>
        </form>
      </div>
    </div>
  );
};

export default TicketManage;
