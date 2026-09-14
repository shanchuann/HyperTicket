import { useState } from 'react';
import { useNavigate, Link } from 'react-router-dom';
import { Eye, EyeOff, Loader2, ShieldCheck, KeyRound } from 'lucide-react';
import { adminApi } from '../../api/admin';
import { useTheme } from '../../hooks/useTheme';
import { catchError } from '../../utils/errors';
import '../Auth/AuthForms.css';
import './AdminLogin.css';

// ── Force-change password modal ───────────────────────────────────────────────

interface ChangeModalProps {
  pendingToken: string;
  pendingUsername: string;
  pendingRole: string;
  onDone: (token: string, username: string, role: string) => void;
}

const ForceChangeModal = ({ pendingToken, pendingUsername, pendingRole, onDone }: ChangeModalProps) => {
  const [newPwd, setNewPwd] = useState('');
  const [confirm, setConfirm] = useState('');
  const [show, setShow] = useState(false);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState('');

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    if (newPwd !== confirm) { setError('两次输入的密码不一致'); return; }
    if (newPwd.length < 6 || newPwd.length > 16) { setError('密码需 6-16 位'); return; }
    setLoading(true);
    setError('');
    // temporarily set token so changePassword can read it
    localStorage.setItem('admin_token', pendingToken);
    try {
      await adminApi.changePassword(newPwd);
      onDone(pendingToken, pendingUsername, pendingRole);
    } catch (err) {
      localStorage.removeItem('admin_token');
      setError(catchError(err, '修改失败'));
    } finally {
      setLoading(false);
    }
  };

  return (
    <div className="admin-login-page" style={{ position: 'fixed', inset: 0, zIndex: 999 }}>
      <div className="admin-login-card">
        <div className="admin-login-brand">
          <KeyRound size={32} className="admin-login-icon" style={{ color: 'var(--color-warning)' }} />
          <h1 className="admin-login-title">修改默认密码</h1>
          <p className="admin-login-subtitle">检测到您正在使用默认密码，请立即修改</p>
        </div>

        {error && <div className="auth-error" role="alert">{error}</div>}

        <form className="auth-form" onSubmit={handleSubmit} noValidate>
          <div className="form-group">
            <label className="form-label">新密码</label>
            <div className="form-input-wrapper">
              <input type={show ? 'text' : 'password'} value={newPwd}
                onChange={e => { setNewPwd(e.target.value); setError(''); }}
                placeholder="6-16 位，含大小写字母和数字" className="form-input"
                autoComplete="new-password" disabled={loading} />
              <button type="button" className="form-input-suffix"
                onClick={() => setShow(s => !s)} tabIndex={-1}>
                {show ? <EyeOff size={18} /> : <Eye size={18} />}
              </button>
            </div>
          </div>
          <div className="form-group">
            <label className="form-label">确认新密码</label>
            <input type={show ? 'text' : 'password'} value={confirm}
              onChange={e => { setConfirm(e.target.value); setError(''); }}
              placeholder="再次输入新密码" className="form-input"
              autoComplete="new-password" disabled={loading} />
          </div>
          <button type="submit" className="form-submit" disabled={loading}>
            {loading ? <><Loader2 size={18} className="spinner" />修改中</> : '确认修改并登录'}
          </button>
        </form>
      </div>
    </div>
  );
};

// ── Login page ────────────────────────────────────────────────────────────────

const AdminLogin = () => {
  const navigate = useNavigate();
  const { theme, toggleTheme } = useTheme();
  const [username, setUsername] = useState('');
  const [password, setPassword] = useState('');
  const [showPassword, setShowPassword] = useState(false);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState('');
  const [forceChange, setForceChange] = useState<{
    token: string; username: string; role: string;
  } | null>(null);

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    setError('');
    if (!username.trim() || !password.trim()) { setError('请填写用户名和密码'); return; }
    setIsLoading(true);
    try {
      const resp = await adminApi.login(username.trim(), password);
      if (resp.is_default_password) {
        setForceChange({ token: resp.admin_token, username: resp.username, role: resp.role });
      } else {
        localStorage.setItem('admin_token', resp.admin_token);
        localStorage.setItem('admin_user', JSON.stringify({ username: resp.username, role: resp.role }));
        navigate('/admin', { replace: true });
      }
    } catch (err) {
      setError(catchError(err, '登录失败'));
    } finally {
      setIsLoading(false);
    }
  };

  if (forceChange) {
    return (
      <ForceChangeModal
        pendingToken={forceChange.token}
        pendingUsername={forceChange.username}
        pendingRole={forceChange.role}
        onDone={(token, user, role) => {
          localStorage.setItem('admin_token', token);
          localStorage.setItem('admin_user', JSON.stringify({ username: user, role }));
          navigate('/admin', { replace: true });
        }}
      />
    );
  }

  return (
    <div className="admin-login-page" data-theme={theme}>
      <div className="admin-login-card">
        <div className="admin-login-brand">
          <ShieldCheck size={32} className="admin-login-icon" />
          <h1 className="admin-login-title">管理后台</h1>
          <p className="admin-login-subtitle">HyperTicket 管理员入口</p>
        </div>

        {error && <div className="auth-error" role="alert">{error}</div>}

        <form className="auth-form" onSubmit={handleSubmit} noValidate>
          <div className="form-group">
            <label htmlFor="username" className="form-label">管理员账号</label>
            <input type="text" id="username" value={username}
              onChange={e => { setUsername(e.target.value); setError(''); }}
              placeholder="请输入管理员账号" className="form-input"
              autoComplete="username" disabled={isLoading} />
          </div>
          <div className="form-group">
            <label htmlFor="password" className="form-label">密码</label>
            <div className="form-input-wrapper">
              <input type={showPassword ? 'text' : 'password'} id="password" value={password}
                onChange={e => { setPassword(e.target.value); setError(''); }}
                placeholder="请输入密码" className="form-input"
                autoComplete="current-password" disabled={isLoading} />
              <button type="button" className="form-input-suffix"
                onClick={() => setShowPassword(!showPassword)} tabIndex={-1}
                aria-label={showPassword ? '隐藏密码' : '显示密码'}>
                {showPassword ? <EyeOff size={18} /> : <Eye size={18} />}
              </button>
            </div>
          </div>
          <button type="submit" className="form-submit" disabled={isLoading}>
            {isLoading ? <><Loader2 size={18} className="spinner" />验证中</> : '登录管理后台'}
          </button>
        </form>

        <div className="admin-login-footer">
          <Link to="/auth/login" className="form-link">返回用户登录</Link>
          <button className="admin-login-theme-btn" onClick={toggleTheme} type="button">
            {theme === 'light' ? '深色' : '浅色'}模式
          </button>
        </div>
      </div>
    </div>
  );
};

export default AdminLogin;
