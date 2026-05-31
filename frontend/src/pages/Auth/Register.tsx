import { useState } from 'react';
import { Link, useNavigate } from 'react-router-dom';
import { Eye, EyeOff, Loader2 } from 'lucide-react';
import './AuthForms.css';

const Register = () => {
  const navigate = useNavigate();
  const [formData, setFormData] = useState({
    tel: '',
    username: '',
    password: '',
    confirmPassword: ''
  });
  const [showPassword, setShowPassword] = useState(false);
  const [showConfirmPassword, setShowConfirmPassword] = useState(false);
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

    if (!formData.username) {
      newErrors.username = '请输入用户名';
    } else if (formData.username.length < 2) {
      newErrors.username = '用户名至少 2 个字符';
    }

    if (!formData.password) {
      newErrors.password = '请输入密码';
    } else if (formData.password.length < 6) {
      newErrors.password = '密码长度至少 6 位';
    } else if (!/(?=.*[0-9])(?=.*[a-zA-Z])/.test(formData.password)) {
      newErrors.password = '密码需包含数字和字母';
    }

    if (!formData.confirmPassword) {
      newErrors.confirmPassword = '请确认密码';
    } else if (formData.password !== formData.confirmPassword) {
      newErrors.confirmPassword = '两次输入的密码不一致';
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
      // TODO: 调用实际的注册 API
      // const response = await authApi.register(
      //   formData.tel,
      //   formData.username,
      //   formData.password
      // );

      // 模拟注册成功
      await new Promise(resolve => setTimeout(resolve, 1500));

      // 存储用户信息到 localStorage
      localStorage.setItem('token', 'mock_token');
      localStorage.setItem('user', JSON.stringify({
        tel: formData.tel,
        username: formData.username
      }));

      // 跳转到客户首页
      navigate('/customer');
    } catch (error) {
      setAuthError('注册失败，该手机号可能已被注册');
    } finally {
      setIsLoading(false);
    }
  };

  const handleChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const { name, value } = e.target;
    setFormData(prev => ({ ...prev, [name]: value }));
    if (errors[name]) {
      setErrors(prev => ({ ...prev, [name]: '' }));
    }
    if (authError) setAuthError('');
  };

  // 密码强度检查
  const getPasswordStrength = (password: string): number => {
    let strength = 0;
    if (password.length >= 6) strength++;
    if (password.length >= 8) strength++;
    if (/[a-z]/.test(password) && /[A-Z]/.test(password)) strength++;
    if (/\d/.test(password)) strength++;
    if (/[^a-zA-Z0-9]/.test(password)) strength++;
    return strength;
  };

  const passwordStrength = getPasswordStrength(formData.password);

  return (
    <div className="auth-form-container">
      <h1 className="auth-form-title">创建账号</h1>
      <p className="auth-form-subtitle">注册 HyperTicket 账号开始预订</p>

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
          <label htmlFor="username" className="form-label">
            用户名
          </label>
          <input
            type="text"
            id="username"
            name="username"
            value={formData.username}
            onChange={handleChange}
            placeholder="请输入用户名"
            className={`form-input ${errors.username ? 'form-input-error' : ''}`}
            autoComplete="username"
            disabled={isLoading}
          />
          {errors.username && (
            <span className="form-error" role="alert">
              {errors.username}
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
              placeholder="请设置密码（至少6位，包含数字和字母）"
              className={`form-input ${errors.password ? 'form-input-error' : ''}`}
              autoComplete="new-password"
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
          {formData.password && !errors.password && (
            <div className="password-strength">
              <div className="password-strength-bar">
                {[1, 2, 3, 4, 5].map((level) => (
                  <div
                    key={level}
                    className={`password-strength-segment ${
                      passwordStrength >= level ? 'active' : ''
                    }`}
                  />
                ))}
              </div>
              <span className="password-strength-text">
                {passwordStrength <= 2 ? '弱' : passwordStrength <= 4 ? '中' : '强'}
              </span>
            </div>
          )}
          {errors.password && (
            <span className="form-error" role="alert">
              {errors.password}
            </span>
          )}
        </div>

        <div className="form-group">
          <label htmlFor="confirmPassword" className="form-label">
            确认密码
          </label>
          <div className="form-input-wrapper">
            <input
              type={showConfirmPassword ? 'text' : 'password'}
              id="confirmPassword"
              name="confirmPassword"
              value={formData.confirmPassword}
              onChange={handleChange}
              placeholder="请再次输入密码"
              className={`form-input ${errors.confirmPassword ? 'form-input-error' : ''}`}
              autoComplete="new-password"
              disabled={isLoading}
            />
            <button
              type="button"
              className="form-input-suffix"
              onClick={() => setShowConfirmPassword(!showConfirmPassword)}
              tabIndex={-1}
              aria-label={showConfirmPassword ? '隐藏密码' : '显示密码'}
            >
              {showConfirmPassword ? <EyeOff size={18} /> : <Eye size={18} />}
            </button>
          </div>
          {errors.confirmPassword && (
            <span className="form-error" role="alert">
              {errors.confirmPassword}
            </span>
          )}
        </div>

        <div className="form-agreement">
          <label className="form-checkbox">
            <input type="checkbox" disabled={isLoading} />
            <span>
              我已阅读并同意
              <Link to="/terms" className="form-link">服务条款</Link>
              和
              <Link to="/privacy" className="form-link">隐私政策</Link>
            </span>
          </label>
        </div>

        <button
          type="submit"
          className="form-submit"
          disabled={isLoading}
        >
          {isLoading ? (
            <>
              <Loader2 size={18} className="spinner" />
              注册中
            </>
          ) : (
            '注册'
          )}
        </button>
      </form>

      <p className="auth-form-footer">
        已有账号？
        <Link to="/auth/login" className="form-link">
          立即登录
        </Link>
      </p>
    </div>
  );
};

export default Register;
