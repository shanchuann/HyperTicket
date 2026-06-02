import { useState, useEffect, useCallback } from 'react';
import { Search, UserX, UserCheck, Loader2 } from 'lucide-react';
import { adminApi } from '../../api/admin';
import type { AdminUser } from '../../api/admin';
import './UserManage.css';

const maskTel = (tel: string) =>
  tel.length === 11 ? tel.slice(0, 3) + '****' + tel.slice(7) : tel;

const UserManage = () => {
  const [users, setUsers] = useState<AdminUser[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState('');
  const [searchQuery, setSearchQuery] = useState('');
  const [filter, setFilter] = useState<'all' | 'normal' | 'blacklisted'>('all');
  const [operating, setOperating] = useState<number | null>(null);

  const loadUsers = useCallback(async () => {
    setLoading(true);
    setError('');
    try {
      setUsers(await adminApi.listUsers());
    } catch (e) {
      setError(e instanceof Error ? e.message : '加载失败');
    } finally {
      setLoading(false);
    }
  }, []);

  useEffect(() => { loadUsers(); }, [loadUsers]);

  const handleBlacklist = async (user: AdminUser) => {
    const action = user.status === 1 ? 'add' : 'remove';
    const label = action === 'add' ? '加入黑名单' : '移出黑名单';
    if (!window.confirm(`确认将「${user.username}」${label}？`)) return;

    setOperating(user.user_id);
    try {
      await adminApi.blacklist(user.tel, action);
      setUsers(prev =>
        prev.map(u => u.user_id === user.user_id ? { ...u, status: action === 'add' ? 0 : 1 } : u)
      );
    } catch (e) {
      alert(e instanceof Error ? e.message : '操作失败');
    } finally {
      setOperating(null);
    }
  };

  const filtered = users.filter(u => {
    const matchSearch =
      u.username.toLowerCase().includes(searchQuery.toLowerCase()) ||
      u.tel.includes(searchQuery);
    const matchFilter =
      filter === 'all' ? true :
      filter === 'normal' ? u.status === 1 :
      u.status === 0;
    return matchSearch && matchFilter;
  });

  const normalCount = users.filter(u => u.status === 1).length;
  const blackCount  = users.filter(u => u.status === 0).length;

  if (loading) {
    return (
      <div className="user-manage">
        <div className="user-manage-loading">
          <div className="loading-spinner" /><p>加载中...</p>
        </div>
      </div>
    );
  }

  return (
    <div className="user-manage">
      <div className="user-manage-header">
        <h1 className="user-manage-title">用户管理</h1>
        <div className="user-manage-summary">
          <span className="summary-badge normal">正常 {normalCount}</span>
          <span className="summary-badge blacklisted">黑名单 {blackCount}</span>
        </div>
      </div>

      {error && (
        <div className="user-manage-error">
          {error}
          <button onClick={loadUsers}>重试</button>
        </div>
      )}

      <div className="user-manage-toolbar">
        <div className="user-manage-search">
          <Search size={18} className="search-icon" />
          <input
            type="text"
            placeholder="搜索用户名或手机号..."
            value={searchQuery}
            onChange={e => setSearchQuery(e.target.value)}
          />
        </div>
        <div className="user-filter-tabs">
          {(['all', 'normal', 'blacklisted'] as const).map(f => (
            <button
              key={f}
              className={`filter-tab ${filter === f ? 'active' : ''}`}
              onClick={() => setFilter(f)}
            >
              {f === 'all' ? '全部' : f === 'normal' ? '正常' : '黑名单'}
            </button>
          ))}
        </div>
        <span className="user-count-label">共 {filtered.length} 人</span>
      </div>

      <div className="user-table-wrapper">
        <table className="user-table">
          <thead>
            <tr>
              <th>ID</th>
              <th>用户名</th>
              <th>手机号</th>
              <th>状态</th>
              <th>操作</th>
            </tr>
          </thead>
          <tbody>
            {filtered.length === 0 ? (
              <tr>
                <td colSpan={5} className="user-table-empty">暂无用户数据</td>
              </tr>
            ) : filtered.map(user => (
              <tr key={user.user_id}>
                <td className="user-id">#{user.user_id}</td>
                <td className="user-name">{user.username}</td>
                <td className="user-tel">{maskTel(user.tel)}</td>
                <td>
                  <span className={`user-status-badge ${user.status === 1 ? 'status-normal' : 'status-blacklisted'}`}>
                    {user.status === 1 ? '正常' : '黑名单'}
                  </span>
                </td>
                <td>
                  <button
                    className={`user-action-btn ${user.status === 1 ? 'btn-ban' : 'btn-unban'}`}
                    onClick={() => handleBlacklist(user)}
                    disabled={operating === user.user_id}
                    title={user.status === 1 ? '加入黑名单' : '移出黑名单'}
                  >
                    {operating === user.user_id
                      ? <Loader2 size={15} className="spinner" />
                      : user.status === 1
                        ? <><UserX size={15} /><span>封禁</span></>
                        : <><UserCheck size={15} /><span>解封</span></>
                    }
                  </button>
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </div>
  );
};

export default UserManage;
