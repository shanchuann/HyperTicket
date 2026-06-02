import { useState } from 'react';
import { useNavigate, Link } from 'react-router-dom';
import { Eye, EyeOff, Loader2, ShieldCheck } from 'lucide-react';
import { adminApi } from '../../api/admin';
import { useTheme } from '../../hooks/useTheme';
import '../Auth/AuthForms.css';
import './AdminLogin.css';

const errorMessages: Record<string, string> = {
  ADMIN_INVALID_CREDENTIALS: '用户名或密码错误',
  INVALID_INPUT: '请填写用户名和密码',
  DB_UNAVAILABLE: '服务暂时不可用，请稍后再试',
  RATE_LIMITED: '操作过于频繁，请稍后再试',
};

const translateError = (msg: string) => errorMessages[msg] ?? msg;

const AdminLogin = () => {
  const navigate = useNavigate();
  const { theme, toggleTheme } = useTheme();
  const [username, setUsername] = useState('');
  const [password, setPassword] = useState('');
  const [showPassword, setShowPassword] = useState(false);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState('');

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    setError('');
    if (!username.trim() || !password.trim()) {
      setError('请填写用户名和密码');
      return;
    }
    setIsLoading(true);
    try {
      const resp = await adminApi.login(username.trim(), password);
      localStorage.setItem('admin_token', resp.admin_token);
      localStorage.setItem('admin_user', JSON.stringify({ username: resp.username, role: resp.role }));
      navigate('/admin', { replace: true });
    } catch (err) {
      setError(translateError(err instanceof Error ? err.message : '登录失败'));
    } finally {
      setIsLoading(false);
    }
  };

  return (
    <div className="admin-login-page" data-theme={theme}>
      <div className="admin-login-card">
        <div className="admin-login-brand">
          <ShieldCheck size={32} className="admin-login-icon" />
          <h1 className="admin-login-title">管理后台</h1>
          <p className="admin-login-subtitle">HyperTicket 管理员入口</p>
        </div>

        {error && (
          <div className="auth-error" role="alert">{error}</div>
        )}

        <form className="auth-form" onSubmit={handleSubmit} noValidate>
          <div className="form-group">
            <label htmlFor="username" className="form-label">管理员账号</label>
            <input
              type="text"
              id="username"
              value={username}
              onChange={e => { setUsername(e.target.value); setError(''); }}
              placeholder="请输入管理员账号"
              className="form-input"
              autoComplete="username"
              disabled={isLoading}
            />
          </div>

          <div className="form-group">
            <label htmlFor="password" className="form-label">密码</label>
            <div className="form-input-wrapper">
              <input
                type={showPassword ? 'text' : 'password'}
                id="password"
                value={password}
                onChange={e => { setPassword(e.target.value); setError(''); }}
                placeholder="请输入密码"
                className="form-input"
                autoComplete="current-password"
                disabled={isLoading}
              />
              <button
                type="button"
                className="form-input-suffix"
                onClick={() => setShowPassword(!showPassword)}
                tabIndex={-1}
                aria-label={showPassword ? '隐藏密码' : '显示密码'}
              >
                {showPassword ? <EyeOff size={18} /> : <Eye size={18} />}
              </button>
            </div>
          </div>

          <button type="submit" className="form-submit" disabled={isLoading}>
            {isLoading ? (
              <><Loader2 size={18} className="spinner" />验证中</>
            ) : '登录管理后台'}
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
