import { useState, useEffect, useCallback, useRef } from 'react';
import { Plus, Search, Trash2, Calendar, MapPin, Loader2, X, ImagePlus } from 'lucide-react';
import { adminApi } from '../../api/admin';
import type { AdminTicket } from '../../api/admin';
import { catchError } from '../../utils/errors';
import { useToast } from '../../components/ToastContext';
import DatePicker from '../../components/DatePicker';
import ImageCropEditor from '../../components/ImageCropEditor';
import CustomSelect from '../../components/CustomSelect';
import './TicketManage.css';

const CATEGORY_OPTIONS = [
  { value: 'concert',    label: '演唱会' },
  { value: 'sports',     label: '体育赛事' },
  { value: 'movie',      label: '观影' },
  { value: 'theater',    label: '舞台剧' },
  { value: 'exhibition', label: '展览' },
];

const TicketManage = () => {
  const toast = useToast();
  const [tickets, setTickets] = useState<AdminTicket[]>([]);
  const [loading, setLoading] = useState(true);
  const [searchQuery, setSearchQuery] = useState('');
  const [showAddModal, setShowAddModal] = useState(false);
  const [deleting, setDeleting] = useState<number | null>(null);

  const loadTickets = useCallback(async () => {
    try {
      setTickets(await adminApi.listTickets());
    } catch (e) {
      toast.error(catchError(e, '加载票务列表失败'));
    } finally {
      setLoading(false);
    }
  }, []); // eslint-disable-line react-hooks/exhaustive-deps

  useEffect(() => {
    let cancelled = false;
    adminApi.listTickets()
      .then(data => { if (!cancelled) setTickets(data); })
      .catch(e => { if (!cancelled) toast.error(catchError(e, '加载票务列表失败')); })
      .finally(() => { if (!cancelled) setLoading(false); });
    return () => { cancelled = true; };
  }, [toast]);

  const handleDelete = async (ticketId: number, title: string) => {
    if (!window.confirm(`确认下架「${title}」？此操作不可撤销。`)) return;
    setDeleting(ticketId);
    try {
      await adminApi.deleteTicket(ticketId);
      setTickets(prev => prev.map(t => t.ticket_id === ticketId ? { ...t, status: 0 } : t));
      toast.success(`「${title}」已下架`);
    } catch (e) {
      toast.error(catchError(e, '下架失败'));
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
  const toast = useToast();
  const [form, setForm] = useState({ title: '', venue: '', event_date: '', total_seats: '', price: '', category: 'concert', city: '北京', artist: '', description: '', notice: '' });
  const [rawImage, setRawImage] = useState<string>('');      // original file dataUrl, for crop editor
  const [coverImage, setCoverImage] = useState<string>('');  // cropped result
  const [coverPreview, setCoverPreview] = useState<string>('');
  const [showCrop, setShowCrop] = useState(false);
  const [submitting, setSubmitting] = useState(false);
  const [error, setError] = useState('');
  const fileInputRef = useRef<HTMLInputElement>(null);

  const handleChange = (e: React.ChangeEvent<HTMLInputElement | HTMLTextAreaElement>) => {
    setForm(prev => ({ ...prev, [e.target.name]: e.target.value }));
    setError('');
  };

  const handleImageChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    if (!file) return;
    if (file.size > 5 * 1024 * 1024) {
      setError('图片大小不能超过 5MB');
      return;
    }
    const reader = new FileReader();
    reader.onload = ev => {
      setRawImage(ev.target?.result as string);
      setShowCrop(true);
    };
    reader.readAsDataURL(file);
  };

  const handleCropConfirm = (dataUrl: string) => {
    setCoverImage(dataUrl);
    setCoverPreview(dataUrl);
    setShowCrop(false);
    setRawImage('');
  };

  const handleRemoveImage = () => {
    setCoverImage('');
    setCoverPreview('');
    if (fileInputRef.current) fileInputRef.current.value = '';
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
      await adminApi.addTicket(form.title.trim(), form.venue.trim(), form.event_date, seats, coverImage || undefined, form.category, parseInt(form.price) || 0, {
        city: form.city.trim() || '北京',
        artist: form.artist.trim(),
        description: form.description.trim(),
        notice: form.notice.trim(),
      });
      toast.success(`票务「${form.title.trim()}」已添加，座位图正在生成`);
      onSuccess();
    } catch (e) {
      setError(catchError(e, '添加失败'));
    } finally {
      setSubmitting(false);
    }
  };

  return (
    <>
      {/* Crop editor renders on top of modal */}
      {showCrop && rawImage && (
        <ImageCropEditor
          src={rawImage}
          onConfirm={handleCropConfirm}
          onCancel={() => { setShowCrop(false); setRawImage(''); if (fileInputRef.current) fileInputRef.current.value = ''; }}
        />
      )}

      <div className="modal-overlay" onClick={e => e.target === e.currentTarget && onClose()}>
        <div className="modal-card">
          <div className="modal-header">
            <h2 className="modal-title">添加票务</h2>
            <button className="modal-close" onClick={onClose} aria-label="关闭"><X size={20} /></button>
          </div>

          {error && <div className="modal-error">{error}</div>}

          <form className="modal-form" onSubmit={handleSubmit}>
            {/* Cover image */}
            <div className="form-group">
              <label className="form-label">封面图片（可选）</label>
              <div className="cover-upload-area">
                {coverPreview ? (
                  <div className="cover-preview-wrapper">
                    <img src={coverPreview} alt="封面预览" className="cover-preview-img" />
                    <div className="cover-preview-actions">
                      <button type="button" className="cover-redit-btn"
                        onClick={() => { setRawImage(coverPreview); setShowCrop(true); }}
                        disabled={submitting}>重新裁剪</button>
                      <button type="button" className="cover-remove-btn"
                        onClick={handleRemoveImage} disabled={submitting}><X size={14} /></button>
                    </div>
                  </div>
                ) : (
                  <button type="button" className="cover-upload-btn"
                    onClick={() => fileInputRef.current?.click()} disabled={submitting}>
                    <ImagePlus size={24} />
                    <span>点击上传封面</span>
                    <span className="cover-upload-hint">支持 JPG / PNG / WebP，最大 5MB</span>
                  </button>
                )}
                <input ref={fileInputRef} type="file" accept="image/jpeg,image/png,image/webp"
                  onChange={handleImageChange} style={{ display: 'none' }} disabled={submitting} />
              </div>
            </div>

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
              <DatePicker value={form.event_date} onChange={v => setForm(f => ({ ...f, event_date: v }))} disabled={submitting} />
            </div>
            <div className="form-group">
              <label className="form-label">总座位数</label>
              <input type="number" name="total_seats" value={form.total_seats} onChange={handleChange}
                className="form-input" placeholder="如：80000" min="1" disabled={submitting} />
            </div>
            <div className="form-row">
              <div className="form-group">
                <label className="form-label">演出类别</label>
                <CustomSelect
                  options={CATEGORY_OPTIONS}
                  value={form.category}
                  onChange={v => setForm(f => ({ ...f, category: v }))}
                  disabled={submitting}
                />
              </div>
              <div className="form-group">
                <label className="form-label">基础票价（元）</label>
                <input type="number" name="price" value={form.price} onChange={handleChange}
                  className="form-input" placeholder="如：380" min="0" disabled={submitting} />
              </div>
            </div>

            <div className="form-row">
              <div className="form-group">
                <label className="form-label">城市</label>
                <input type="text" name="city" value={form.city} onChange={handleChange}
                  className="form-input" placeholder="如：北京" disabled={submitting} />
              </div>
              <div className="form-group">
                <label className="form-label">艺人/团体（可选）</label>
                <input type="text" name="artist" value={form.artist} onChange={handleChange}
                  className="form-input" placeholder="如：周杰伦" disabled={submitting} />
              </div>
            </div>
            <div className="form-group">
              <label className="form-label">演出详情（可选）</label>
              <textarea name="description" value={form.description} onChange={handleChange}
                className="form-input form-textarea" rows={3} placeholder="演出介绍、亮点..." disabled={submitting} />
            </div>
            <div className="form-group">
              <label className="form-label">购票须知（可选）</label>
              <textarea name="notice" value={form.notice} onChange={handleChange}
                className="form-input form-textarea" rows={3} placeholder="实名制、退改签规则..." disabled={submitting} />
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
    </>
  );
};

export default TicketManage;
