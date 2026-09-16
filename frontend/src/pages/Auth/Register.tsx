import { useEffect, useState } from 'react';
import { Link, useNavigate } from 'react-router-dom';
import { CheckCircle2, Eye, EyeOff, Loader2, Mail, MessageSquareText } from 'lucide-react';
import { authApi } from '../../api/auth';
import { useAuth } from '../../hooks/useAuth';
import type { VerificationChannel } from '../../types';
import { catchError } from '../../utils/errors';
import './AuthForms.css';

type FormData = {
  tel: string;
  email: string;
  username: string;
  password: string;
  confirmPassword: string;
};

const passwordError = (password: string) => {
  if (!password) return '请输入密码';
  if (password.length < 8 || password.length > 64) return '密码长度需为 8-64 位';
  if (!/[a-z]/.test(password) || !/[A-Z]/.test(password) || !/\d/.test(password)) {
    return '密码需同时包含大写字母、小写字母和数字';
  }
  return '';
};

const Register = () => {
  const navigate = useNavigate();
  const { login } = useAuth();
  const [formData, setFormData] = useState<FormData>({
    tel: '', email: '', username: '', password: '', confirmPassword: '',
  });
  const [channel, setChannel] = useState<VerificationChannel>('EMAIL');
  const [challengeId, setChallengeId] = useState('');
  const [code, setCode] = useState('');
  const [mockCode, setMockCode] = useState('');
  const [cooldown, setCooldown] = useState(0);
  const [agreed, setAgreed] = useState(false);
  const [showPassword, setShowPassword] = useState(false);
  const [showConfirmPassword, setShowConfirmPassword] = useState(false);
  const [errors, setErrors] = useState<Record<string, string>>({});
  const [isLoading, setIsLoading] = useState(false);
  const [authError, setAuthError] = useState('');

  useEffect(() => {
    if (cooldown <= 0) return;
    const timer = window.setInterval(() => setCooldown((value) => Math.max(0, value - 1)), 1000);
    return () => window.clearInterval(timer);
  }, [cooldown]);

  const updateField = (event: React.ChangeEvent<HTMLInputElement>) => {
    const { name, value } = event.target;
    setFormData((current) => ({ ...current, [name]: value }));
    setErrors((current) => ({ ...current, [name]: '' }));
    setAuthError('');
  };

  const validateAccount = () => {
    const next: Record<string, string> = {};
    if (!/^1[3-9]\d{9}$/.test(formData.tel)) next.tel = '请输入有效的手机号';
    if (channel === 'EMAIL' && !/^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(formData.email)) {
      next.email = '请输入有效的邮箱地址';
    }
    if (formData.username.trim().length < 2) next.username = '用户名至少 2 个字符';
    const pwdError = passwordError(formData.password);
    if (pwdError) next.password = pwdError;
    if (formData.confirmPassword !== formData.password) next.confirmPassword = '两次输入的密码不一致';
    if (!agreed) next.agreement = '请先同意服务条款和隐私政策';
    setErrors(next);
    return Object.keys(next).length === 0;
  };

  const requestCode = async () => {
    if (!validateAccount()) return;
    setIsLoading(true);
    setAuthError('');
    try {
      const response = await authApi.requestRegistrationCode(
        formData.tel,
        channel,
        channel === 'EMAIL' ? formData.email.trim() : undefined,
      );
      setChallengeId(response.challenge_id);
      setMockCode(response.mock_code || '');
      setCode('');
      setCooldown(60);
    } catch (error) {
      setAuthError(catchError(error, '验证码发送失败，请稍后再试'));
    } finally {
      setIsLoading(false);
    }
  };

  const handleSubmit = async (event: React.FormEvent) => {
    event.preventDefault();
    if (!challengeId) {
      await requestCode();
      return;
    }
    if (!/^\d{6}$/.test(code)) {
      setErrors((current) => ({ ...current, code: '请输入 6 位验证码' }));
      return;
    }
    setIsLoading(true);
    setAuthError('');
    try {
      const verified = await authApi.verifyRegistrationCode(challengeId, code);
      const response = await authApi.register(
        formData.tel,
        formData.username.trim(),
        formData.password,
        verified.verification_token,
        channel === 'EMAIL' ? formData.email.trim() : undefined,
      );
      const token = response.token || '';
      login({
        tel: formData.tel,
        username: response.username || formData.username.trim(),
        token,
        email: response.email,
        emailVerified: response.email_verified,
        phoneVerified: response.phone_verified,
      }, token);
      navigate('/customer');
    } catch (error) {
      setAuthError(catchError(error, '注册失败，请稍后再试'));
    } finally {
      setIsLoading(false);
    }
  };

  const switchChannel = (value: VerificationChannel) => {
    setChannel(value);
    setChallengeId('');
    setCode('');
    setMockCode('');
    setCooldown(0);
    setErrors({});
    setAuthError('');
  };

  return (
    <div className="auth-form-container">
      <h1 className="auth-form-title">创建账号</h1>
      <p className="auth-form-subtitle">验证联系方式后即可注册 HyperTicket</p>

      <div className="auth-steps" aria-label="注册进度">
        <span className="active">1 账号信息</span>
        <span className={challengeId ? 'active' : ''}>2 验证并注册</span>
      </div>

      {authError && <div className="auth-error" role="alert">{authError}</div>}

      <form className="auth-form" onSubmit={handleSubmit} noValidate>
        {!challengeId ? (
          <>
            <div className="verification-channel" aria-label="验证方式">
              <button type="button" aria-pressed={channel === 'EMAIL'} className={channel === 'EMAIL' ? 'active' : ''} onClick={() => switchChannel('EMAIL')}>
                <Mail size={17} />邮箱验证
              </button>
              <button type="button" aria-pressed={channel === 'SMS'} className={channel === 'SMS' ? 'active' : ''} onClick={() => switchChannel('SMS')}>
                <MessageSquareText size={17} />短信验证
              </button>
            </div>

            <div className="form-group">
              <label htmlFor="tel" className="form-label">手机号</label>
              <input className={`form-input ${errors.tel ? 'form-input-error' : ''}`} id="tel" name="tel" type="tel" value={formData.tel} onChange={updateField} autoComplete="tel" disabled={isLoading} />
              {errors.tel && <span className="form-error" role="alert">{errors.tel}</span>}
            </div>

            {channel === 'EMAIL' && (
              <div className="form-group">
                <label htmlFor="email" className="form-label">邮箱</label>
                <input className={`form-input ${errors.email ? 'form-input-error' : ''}`} id="email" name="email" type="email" value={formData.email} onChange={updateField} autoComplete="email" disabled={isLoading} />
                {errors.email && <span className="form-error" role="alert">{errors.email}</span>}
              </div>
            )}

            <div className="form-group">
              <label htmlFor="username" className="form-label">用户名</label>
              <input className={`form-input ${errors.username ? 'form-input-error' : ''}`} id="username" name="username" value={formData.username} onChange={updateField} autoComplete="username" disabled={isLoading} />
              {errors.username && <span className="form-error" role="alert">{errors.username}</span>}
            </div>

            <div className="form-group">
              <label htmlFor="password" className="form-label">密码</label>
              <div className="form-input-wrapper">
                <input className={`form-input ${errors.password ? 'form-input-error' : ''}`} id="password" name="password" type={showPassword ? 'text' : 'password'} value={formData.password} onChange={updateField} autoComplete="new-password" placeholder="8-64 位，包含大小写字母和数字" disabled={isLoading} />
                <button type="button" className="form-input-suffix" onClick={() => setShowPassword((value) => !value)} aria-label={showPassword ? '隐藏密码' : '显示密码'}>{showPassword ? <EyeOff size={18} /> : <Eye size={18} />}</button>
              </div>
              {errors.password && <span className="form-error" role="alert">{errors.password}</span>}
            </div>

            <div className="form-group">
              <label htmlFor="confirmPassword" className="form-label">确认密码</label>
              <div className="form-input-wrapper">
                <input className={`form-input ${errors.confirmPassword ? 'form-input-error' : ''}`} id="confirmPassword" name="confirmPassword" type={showConfirmPassword ? 'text' : 'password'} value={formData.confirmPassword} onChange={updateField} autoComplete="new-password" disabled={isLoading} />
                <button type="button" className="form-input-suffix" onClick={() => setShowConfirmPassword((value) => !value)} aria-label={showConfirmPassword ? '隐藏密码' : '显示密码'}>{showConfirmPassword ? <EyeOff size={18} /> : <Eye size={18} />}</button>
              </div>
              {errors.confirmPassword && <span className="form-error" role="alert">{errors.confirmPassword}</span>}
            </div>

            <div className="form-agreement">
              <label className="form-checkbox">
                <input type="checkbox" checked={agreed} onChange={(event) => { setAgreed(event.target.checked); setErrors((current) => ({ ...current, agreement: '' })); }} disabled={isLoading} />
                <span>我已阅读并同意<Link to="/terms" className="form-link">服务条款</Link>和<Link to="/privacy" className="form-link">隐私政策</Link></span>
              </label>
              {errors.agreement && <span className="form-error" role="alert">{errors.agreement}</span>}
            </div>
          </>
        ) : (
          <div className="verification-stage">
            <CheckCircle2 size={32} />
            <div>
              <h2>验证码已发送</h2>
              <p>{channel === 'EMAIL' ? formData.email : formData.tel}，验证码 5 分钟内有效。</p>
            </div>
            {mockCode && <div className="mock-code">本地测试验证码：<strong>{mockCode}</strong></div>}
            <div className="form-group">
              <label htmlFor="code" className="form-label">6 位验证码</label>
              <input className={`form-input verification-code ${errors.code ? 'form-input-error' : ''}`} id="code" inputMode="numeric" autoComplete="one-time-code" maxLength={6} value={code} onChange={(event) => { setCode(event.target.value.replace(/\D/g, '')); setErrors((current) => ({ ...current, code: '' })); }} disabled={isLoading} autoFocus />
              {errors.code && <span className="form-error" role="alert">{errors.code}</span>}
            </div>
            <div className="verification-actions">
              <button type="button" className="form-link-button" onClick={() => setChallengeId('')} disabled={isLoading}>修改信息</button>
              <button type="button" className="form-link-button" onClick={requestCode} disabled={isLoading || cooldown > 0}>{cooldown > 0 ? `${cooldown} 秒后重发` : '重新发送'}</button>
            </div>
          </div>
        )}

        <button type="submit" className="form-submit" disabled={isLoading}>
          {isLoading ? <><Loader2 size={18} className="spinner" />处理中</> : challengeId ? '验证并创建账号' : '发送验证码'}
        </button>
      </form>

      <p className="auth-form-footer">已有账号？<Link to="/auth/login" className="form-link">立即登录</Link></p>
    </div>
  );
};

export default Register;
