import { useState } from 'react';
import { Link, useNavigate } from 'react-router-dom';
import { ArrowLeft, CheckCircle2, Eye, EyeOff, KeyRound, Loader2, Mail, MessageSquareText, Send } from 'lucide-react';
import { authApi } from '../../api/auth';
import { toChineseError } from '../../api/errors';
import { useCountdown } from '../../hooks/useCountdown';
import type { VerificationChannel } from '../../types';
import Toast from '../../components/Toast';
import './AuthForms.css';

type Step = 'request' | 'verify' | 'reset';
const strongPassword = (value: string) => value.length >= 8 && value.length <= 64 && /[a-z]/.test(value) && /[A-Z]/.test(value) && /\d/.test(value);

export default function ForgotPassword() {
  const navigate = useNavigate();
  const cooldown = useCountdown();
  const [step, setStep] = useState<Step>('request');
  const [channel, setChannel] = useState<VerificationChannel>('EMAIL');
  const [account, setAccount] = useState('');
  const [code, setCode] = useState('');
  const [challengeId, setChallengeId] = useState('');
  const [resetToken, setResetToken] = useState('');
  const [password, setPassword] = useState('');
  const [confirm, setConfirm] = useState('');
  const [showPassword, setShowPassword] = useState(false);
  const [mockCode, setMockCode] = useState('');
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState('');
  const [notice, setNotice] = useState('');

  const requestCode = async () => {
    if (!account.trim()) { setError('请输入手机号或已验证邮箱'); return; }
    setBusy(true); setError('');
    try {
      const response = await authApi.requestPasswordReset(account.trim(), channel);
      setChallengeId(response.challenge_id); setMockCode(response.mock_code || ''); setStep('verify'); cooldown.start(60);
      setNotice('若账号存在且已完成该方式验证，验证码将很快送达');
    } catch (e) { setError(toChineseError(e, '暂时无法发送验证码')); }
    finally { setBusy(false); }
  };

  const verify = async () => {
    if (!/^\d{6}$/.test(code)) { setError('请输入 6 位数字验证码'); return; }
    setBusy(true); setError('');
    try { const response = await authApi.verifyPasswordReset(challengeId, code); setResetToken(response.reset_token); setStep('reset'); setNotice('验证成功，请设置新密码'); }
    catch (e) { setError(toChineseError(e, '验证码校验失败')); }
    finally { setBusy(false); }
  };

  const reset = async (event: React.FormEvent) => {
    event.preventDefault();
    if (!strongPassword(password)) { setError('密码需为 8–64 位，并包含大小写字母和数字'); return; }
    if (password !== confirm) { setError('两次输入的密码不一致'); return; }
    setBusy(true); setError('');
    try { await authApi.confirmPasswordReset(resetToken, password); navigate('/auth/login', { replace: true, state: { passwordReset: true } }); }
    catch (e) { setError(toChineseError(e, '密码重置失败，请重新发起验证')); }
    finally { setBusy(false); }
  };

  return <div className="auth-form-container">
    <Toast message={error || notice} tone={error ? 'error' : 'success'} onClose={() => { setError(''); setNotice(''); }} />
    <Link className="auth-back-link" to="/auth/login"><ArrowLeft size={16}/>返回登录</Link>
    <h1 className="auth-form-title">找回密码</h1>
    <p className="auth-form-subtitle">{step === 'request' ? '通过已验证的联系方式确认身份' : step === 'verify' ? '输入收到的 6 位验证码' : '设置新的登录密码'}</p>
    <div className="step-indicator" aria-label="密码找回进度"><span className="done">1</span><i className={step !== 'request' ? 'done' : ''}/><span className={step !== 'request' ? 'done' : ''}>2</span><i className={step === 'reset' ? 'done' : ''}/><span className={step === 'reset' ? 'done' : ''}>3</span></div>
    {step === 'request' && <div className="auth-form">
      <div className="channel-switch"><button type="button" className={channel === 'EMAIL' ? 'active' : ''} onClick={() => setChannel('EMAIL')}><Mail size={16}/>邮箱</button><button type="button" className={channel === 'SMS' ? 'active' : ''} onClick={() => setChannel('SMS')}><MessageSquareText size={16}/>模拟短信</button></div>
      {channel === 'SMS' && <p className="verification-note">仅用于已启用 Mock SMS 的本地开发环境。</p>}
      <label className="form-group"><span className="form-label">账号</span><input className="form-input" value={account} onChange={e => setAccount(e.target.value)} placeholder="手机号或已验证邮箱" autoComplete="username"/></label>
      <button className="form-submit" type="button" onClick={requestCode} disabled={busy}>{busy ? <Loader2 size={18} className="spinner"/> : <Send size={18}/>}发送验证码</button>
    </div>}
    {step === 'verify' && <div className="auth-form">
      <p className="verification-destination">验证码将发送至账号 <b>{account}</b> 已验证的{channel === 'EMAIL' ? '邮箱' : '手机（本地模拟）'}</p>
      <label className="form-group"><span className="form-label">验证码</span><input className="form-input code-input-large" value={code} onChange={e => setCode(e.target.value.replace(/\D/g, '').slice(0, 6))} inputMode="numeric" autoComplete="one-time-code" maxLength={6} autoFocus/></label>
      {mockCode && <p className="dev-code">本地模拟验证码：<b>{mockCode}</b></p>}
      <button className="form-submit" type="button" onClick={verify} disabled={busy}>{busy ? <Loader2 size={18} className="spinner"/> : <CheckCircle2 size={18}/>}验证身份</button>
      <button className="resend-link centered" type="button" onClick={requestCode} disabled={cooldown.seconds > 0 || busy}>{cooldown.seconds > 0 ? `${cooldown.seconds} 秒后可重新发送` : '重新发送验证码'}</button>
    </div>}
    {step === 'reset' && <form className="auth-form" onSubmit={reset}>
      <label className="form-group"><span className="form-label">新密码</span><div className="form-input-wrapper"><input className="form-input" type={showPassword ? 'text' : 'password'} value={password} onChange={e => setPassword(e.target.value)} autoComplete="new-password" placeholder="8–64 位，包含大小写字母和数字"/><button className="form-input-suffix" type="button" onClick={() => setShowPassword(value => !value)} aria-label={showPassword ? '隐藏密码' : '显示密码'}>{showPassword ? <EyeOff size={18}/> : <Eye size={18}/>}</button></div></label>
      <label className="form-group"><span className="form-label">确认新密码</span><input className="form-input" type={showPassword ? 'text' : 'password'} value={confirm} onChange={e => setConfirm(e.target.value)} autoComplete="new-password"/></label>
      <button className="form-submit" disabled={busy}>{busy ? <Loader2 size={18} className="spinner"/> : <KeyRound size={18}/>}重置密码</button>
    </form>}
  </div>;
}
