import { useState } from 'react';
import { Link, useNavigate } from 'react-router-dom';
import { ArrowLeft, CheckCircle2, Eye, EyeOff, Loader2, Mail, MessageSquareText, Send, UserPlus } from 'lucide-react';
import { authApi } from '../../api/auth';
import { toChineseError } from '../../api/errors';
import { useAuth } from '../../hooks/useAuth';
import { useCountdown } from '../../hooks/useCountdown';
import type { VerificationChannel } from '../../types';
import Toast from '../../components/Toast';
import './AuthForms.css';

const strongPassword = (value: string) => value.length >= 8 && value.length <= 64
  && /[a-z]/.test(value) && /[A-Z]/.test(value) && /\d/.test(value);

const scrollToFormStart = () => window.requestAnimationFrame(() => {
  window.scrollTo({ top: 0, behavior: window.matchMedia('(prefers-reduced-motion: reduce)').matches ? 'auto' : 'smooth' });
});

const Register = () => {
  const navigate = useNavigate();
  const { login } = useAuth();
  const cooldown = useCountdown();
  const [form, setForm] = useState({ tel: '', email: '', username: '', password: '', confirmPassword: '', code: '' });
  const [channel, setChannel] = useState<VerificationChannel>('EMAIL');
  const [challengeId, setChallengeId] = useState('');
  const [verificationToken, setVerificationToken] = useState('');
  const [mockCode, setMockCode] = useState('');
  const [showPassword, setShowPassword] = useState(false);
  const [showConfirmPassword, setShowConfirmPassword] = useState(false);
  const [errors, setErrors] = useState<Record<string, string>>({});
  const [busy, setBusy] = useState<'send' | 'verify' | 'register' | ''>('');
  const [toast, setToast] = useState<{ message: string; tone: 'success' | 'error' | 'info' }>({ message: '', tone: 'info' });

  const clearVerification = () => {
    setChallengeId(''); setVerificationToken(''); setMockCode(''); cooldown.reset();
    setForm(value => ({ ...value, code: '' }));
  };

  const update = (name: keyof typeof form, value: string) => {
    setForm(previous => ({ ...previous, [name]: value }));
    setErrors(previous => ({ ...previous, [name]: '' }));
    if ((name === 'tel' || name === 'email') && (challengeId || verificationToken)) clearVerification();
  };

  const changeChannel = (value: VerificationChannel) => {
    setChannel(value); clearVerification();
  };

  const validateContact = () => {
    const next: Record<string, string> = {};
    if (!/^1[3-9]\d{9}$/.test(form.tel)) next.tel = '请输入有效的 11 位手机号';
    if (channel === 'EMAIL' && !/^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(form.email)) next.email = '请输入有效的邮箱地址';
    setErrors(previous => ({ ...previous, ...next }));
    return Object.keys(next).length === 0;
  };

  const sendCode = async () => {
    if (!validateContact()) return;
    setBusy('send');
    try {
      const response = await authApi.requestRegistrationCode(form.tel, form.email, channel);
      setChallengeId(response.challenge_id); setMockCode(response.mock_code || ''); cooldown.start(60);
      setToast({ message: channel === 'EMAIL' ? '验证码已发送至邮箱' : '验证码已发送至手机', tone: 'success' });
    } catch (error) {
      setToast({ message: toChineseError(error, '验证码发送失败，请稍后再试'), tone: 'error' });
    } finally { setBusy(''); }
  };

  const verifyCode = async () => {
    if (!/^\d{6}$/.test(form.code)) { setErrors(previous => ({ ...previous, code: '请输入 6 位数字验证码' })); return; }
    setBusy('verify');
    try {
      const response = await authApi.verifyRegistrationCode(challengeId, form.code);
      setVerificationToken(response.verification_token);
      scrollToFormStart();
      setToast({ message: '身份验证完成，请继续设置账号', tone: 'success' });
    } catch (error) {
      setToast({ message: toChineseError(error, '验证码校验失败'), tone: 'error' });
    } finally { setBusy(''); }
  };

  const submit = async (event: React.FormEvent) => {
    event.preventDefault();
    const next: Record<string, string> = {};
    if (!verificationToken) next.code = '请先获取并验证验证码';
    if (form.username.trim().length < 2) next.username = '用户名至少 2 个字符';
    if (!strongPassword(form.password)) next.password = '密码需为 8–64 位，并包含大小写字母和数字';
    if (form.password !== form.confirmPassword) next.confirmPassword = '两次输入的密码不一致';
    setErrors(next);
    if (Object.keys(next).length) return;
    setBusy('register');
    try {
      const response = await authApi.register(form.tel, form.username.trim(), form.password, verificationToken);
      const token = response.token || '';
      login({ tel: form.tel, username: response.username || form.username.trim(), token }, token);
      navigate('/customer');
    } catch (error) {
      setToast({ message: toChineseError(error, '注册失败，请稍后再试'), tone: 'error' });
    } finally { setBusy(''); }
  };

  return <div className="auth-form-container auth-form-wide">
    <Toast {...toast} onClose={() => setToast(previous => ({ ...previous, message: '' }))} />
    <h1 className="auth-form-title">创建账号</h1>
    <p className="auth-form-subtitle">{verificationToken ? '设置登录信息，完成账号创建' : '先验证用于账号安全的联系方式'}</p>
    <ol className="register-stepper" aria-label="注册进度">
      <li className={verificationToken ? 'complete' : 'active'} aria-current={!verificationToken ? 'step' : undefined}><span>{verificationToken ? <CheckCircle2 size={15}/> : '1'}</span><div><strong>验证联系方式</strong><small>{verificationToken ? '已完成' : '当前步骤'}</small></div></li>
      <i aria-hidden="true" className={verificationToken ? 'complete' : ''}/>
      <li className={verificationToken ? 'active' : ''} aria-current={verificationToken ? 'step' : undefined}><span>2</span><div><strong>设置账号</strong><small>{verificationToken ? '当前步骤' : '下一步'}</small></div></li>
    </ol>
    <form className="auth-form" onSubmit={verificationToken ? submit : event => { event.preventDefault(); challengeId ? void verifyCode() : void sendCode(); }} noValidate>
      {!verificationToken ? <div className="verification-section register-step-panel">
        <div className="verification-heading"><span>1</span><div><strong>验证联系方式</strong><small>用于账号安全与找回密码</small></div></div>
        <div className="channel-switch" aria-label="验证码接收方式">
          <button type="button" className={channel === 'EMAIL' ? 'active' : ''} onClick={() => changeChannel('EMAIL')}><Mail size={16}/>邮箱</button>
          <button type="button" className={channel === 'SMS' ? 'active' : ''} onClick={() => changeChannel('SMS')}><MessageSquareText size={16}/>模拟短信</button>
        </div>
        {channel === 'SMS' && <p className="verification-note">仅用于已启用 Mock SMS 的本地开发环境。</p>}
        <label className="form-group"><span className="form-label">手机号</span><input className={`form-input ${errors.tel ? 'form-input-error' : ''}`} value={form.tel} onChange={e => update('tel', e.target.value.replace(/\D/g, '').slice(0, 11))} autoComplete="tel" disabled={!!verificationToken}/>{errors.tel && <span className="form-error">{errors.tel}</span>}</label>
        {channel === 'EMAIL' && <label className="form-group"><span className="form-label">验证邮箱</span><input type="email" className={`form-input ${errors.email ? 'form-input-error' : ''}`} value={form.email} onChange={e => update('email', e.target.value)} placeholder="name@example.com" autoComplete="email" disabled={!!verificationToken}/>{errors.email && <span className="form-error">{errors.email}</span>}</label>}
        <div className="code-row">
          <label className="form-group"><span className="form-label">验证码</span><input className={`form-input code-input ${errors.code ? 'form-input-error' : ''}`} value={form.code} onChange={e => update('code', e.target.value.replace(/\D/g, '').slice(0, 6))} inputMode="numeric" autoComplete="one-time-code" placeholder="6 位数字" disabled={!challengeId || !!verificationToken}/>{errors.code && <span className="form-error">{errors.code}</span>}</label>
          {!challengeId ? <button type="button" className="form-secondary" onClick={sendCode} disabled={!!busy}><Send size={16}/>{busy === 'send' ? '发送中' : '获取验证码'}</button>
            : <button type="button" className="form-secondary" onClick={verifyCode} disabled={busy === 'verify'}>{busy === 'verify' ? <Loader2 size={16} className="spinner"/> : <CheckCircle2 size={16}/>}验证</button>}
        </div>
        {challengeId && !verificationToken && <button type="button" className="resend-link" onClick={sendCode} disabled={cooldown.seconds > 0 || !!busy}>{cooldown.seconds > 0 ? `${cooldown.seconds} 秒后可重新发送` : '重新发送验证码'}</button>}
        {mockCode && <p className="dev-code">本地模拟验证码：<b>{mockCode}</b></p>}
      </div> : <div className="verification-section account-section register-step-panel">
        <button type="button" className="step-back" onClick={() => { clearVerification(); scrollToFormStart(); }}><ArrowLeft size={16}/>返回修改联系方式</button>
        <div className="verification-heading"><span>2</span><div><strong>设置账号</strong><small>{verificationToken ? '填写登录信息完成注册' : '完成上一步后继续'}</small></div></div>
        <label className="form-group"><span className="form-label">用户名</span><input className={`form-input ${errors.username ? 'form-input-error' : ''}`} value={form.username} onChange={e => update('username', e.target.value)} autoComplete="username" disabled={!verificationToken || !!busy}/>{errors.username && <span className="form-error">{errors.username}</span>}</label>
        <label className="form-group"><span className="form-label">密码</span><div className="form-input-wrapper"><input type={showPassword ? 'text' : 'password'} className={`form-input ${errors.password ? 'form-input-error' : ''}`} value={form.password} onChange={e => update('password', e.target.value)} autoComplete="new-password" placeholder="8–64 位，包含大小写字母和数字" disabled={!verificationToken || !!busy}/><button type="button" className="form-input-suffix" onClick={() => setShowPassword(value => !value)} aria-label={showPassword ? '隐藏密码' : '显示密码'}>{showPassword ? <EyeOff size={18}/> : <Eye size={18}/>}</button></div>{errors.password && <span className="form-error">{errors.password}</span>}</label>
        <label className="form-group"><span className="form-label">确认密码</span><div className="form-input-wrapper"><input type={showConfirmPassword ? 'text' : 'password'} className={`form-input ${errors.confirmPassword ? 'form-input-error' : ''}`} value={form.confirmPassword} onChange={e => update('confirmPassword', e.target.value)} autoComplete="new-password" disabled={!verificationToken || !!busy}/><button type="button" className="form-input-suffix" onClick={() => setShowConfirmPassword(value => !value)} aria-label={showConfirmPassword ? '隐藏密码' : '显示密码'}>{showConfirmPassword ? <EyeOff size={18}/> : <Eye size={18}/>}</button></div>{errors.confirmPassword && <span className="form-error">{errors.confirmPassword}</span>}</label>
        <label className="form-checkbox"><input type="checkbox" required disabled={!!busy}/><span>我已阅读并同意<Link to="/terms" className="form-link">服务条款</Link>和<Link to="/privacy" className="form-link">隐私政策</Link></span></label>
        <button className="form-submit" disabled={!!busy}>{busy === 'register' ? <><Loader2 size={18} className="spinner"/>注册中</> : <><UserPlus size={18}/>完成注册</>}</button>
      </div>}
    </form>
    <p className="auth-form-footer">已有账号？<Link to="/auth/login" className="form-link">立即登录</Link></p>
  </div>;
};

export default Register;
