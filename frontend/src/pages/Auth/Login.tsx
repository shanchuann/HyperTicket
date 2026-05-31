import { useState } from 'react';
import { Link, useNavigate } from 'react-router-dom';
import { Eye, EyeOff, Loader2 } from 'lucide-react';
import './AuthForms.css';

const Login = () => {
  const navigate = useNavigate();
  const [formData, setFormData] = useState({
    tel: '',
    password: ''
  });
  const [showPassword, setShowPassword] = useState(false);
  const [errors, setErrors] = useState<Record<string, string>>({});
  const [isLoading, setIsLoading] = useState(false);
  const [authError, setAuthError] = useState('');

  const validateForm = () => {
    const newErrors: Record<string, string> = {};

    if (!formData.tel) {
      newErrors.tel = '请输入手机号';
    } else if (!/^1[3-9]\d{9}$/.test(formData.tel)) {
      newErrors.tel = '请输入有效的手机号';
    }

    if (!formData.password) {
      newErrors.password = '请输入密码';
    } else if (formData.password.length < 6) {
      newErrors.password = '密码长度至少 6 位';
    }

    setErrors(newErrors);
    return Object.keys(newErrors).length === 0;
  };

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    setAuthError('');

    if (!validateForm()) return;

    setIsLoading(true);

    try {
      // TODO: 调用实际的登录 API
      // const response = await authApi.login(formData.tel, formData.password);

      // 模拟登录成功
      await new Promise(resolve => setTimeout(resolve, 1500));

      // 存储用户信息到 localStorage
      localStorage.setItem('token', 'mock_token');
      localStorage.setItem('user', JSON.stringify({
        tel: formData.tel,
        username: '用户'
      }));

      // 跳转到客户首页
      navigate('/customer');
    } catch (error) {
      setAuthError('手机号或密码错误，请重试');
    } finally {
      setIsLoading(false);
    }
  };

  const handleChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const { name, value } = e.target;
    setFormData(prev => ({ ...prev, [name]: value }));
    // 清除该字段的错误
    if (errors[name]) {
      setErrors(prev => ({ ...prev, [name]: '' }));
    }
    // 清除全局错误
    if (authError) setAuthError('');
  };

  return (
    <div className="auth-form-container">
      <h1 className="auth-form-title">欢迎回来</h1>
      <p className="auth-form-subtitle">登录您的 HyperTicket 账号</p>

      {authError && (
        <div className="auth-error" role="alert">
          {authError}
        </div>
      )}

      <form className="auth-form" onSubmit={handleSubmit} noValidate>
        <div className="form-group">
          <label htmlFor="tel" className="form-label">
            手机号
          </label>
          <input
            type="tel"
            id="tel"
            name="tel"
            value={formData.tel}
            onChange={handleChange}
            placeholder="请输入手机号"
            className={`form-input ${errors.tel ? 'form-input-error' : ''}`}
            autoComplete="tel"
            disabled={isLoading}
          />
          {errors.tel && (
            <span className="form-error" role="alert">
              {errors.tel}
            </span>
          )}
        </div>

        <div className="form-group">
          <label htmlFor="password" className="form-label">
            密码
          </label>
          <div className="form-input-wrapper">
            <input
              type={showPassword ? 'text' : 'password'}
              id="password"
              name="password"
              value={formData.password}
              onChange={handleChange}
              placeholder="请输入密码"
              className={`form-input ${errors.password ? 'form-input-error' : ''}`}
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
          {errors.password && (
            <span className="form-error" role="alert">
              {errors.password}
            </span>
          )}
        </div>

        <div className="form-options">
          <label className="form-checkbox">
            <input type="checkbox" disabled={isLoading} />
            <span>记住我</span>
          </label>
          <Link to="/auth/forgot-password" className="form-link">
            忘记密码
          </Link>
        </div>

        <button
          type="submit"
          className="form-submit"
          disabled={isLoading}
        >
          {isLoading ? (
            <>
              <Loader2 size={18} className="spinner" />
              登录中
            </>
          ) : (
            '登录'
          )}
        </button>
      </form>

      <p className="auth-form-footer">
        还没有账号？
        <Link to="/auth/register" className="form-link">
          立即注册
        </Link>
      </p>
    </div>
  );
};

export default Login;
