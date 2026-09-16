import { useState } from 'react';
import { Link, useNavigate } from 'react-router-dom';
import { CheckCircle2, Eye, EyeOff, Loader2, Mail, MessageSquareText } from 'lucide-react';
import { authApi } from '../../api/auth';
import type { VerificationChannel } from '../../types';
import { catchError } from '../../utils/errors';
import './AuthForms.css';

type Step = 'account' | 'code' | 'password' | 'done';

const ForgotPassword = () => {
  const navigate = useNavigate();
  const [step, setStep] = useState<Step>('account');
  const [account, setAccount] = useState('');
  const [channel, setChannel] = useState<VerificationChannel>('EMAIL');
  const [challengeId, setChallengeId] = useState('');
  const [resetToken, setResetToken] = useState('');
  const [code, setCode] = useState('');
  const [mockCode, setMockCode] = useState('');
  const [password, setPassword] = useState('');
  const [confirmPassword, setConfirmPassword] = useState('');
  const [showPassword, setShowPassword] = useState(false);
  const [error, setError] = useState('');
  const [loading, setLoading] = useState(false);

  const submit = async (event: React.FormEvent) => {
    event.preventDefault();
    setError('');
    setLoading(true);
    try {
      if (step === 'account') {
        if (!account.trim()) throw new Error('INVALID_INPUT');
        const response = await authApi.requestPasswordReset(account.trim(), channel);
        setChallengeId(response.challenge_id);
        setMockCode(response.mock_code || '');
        setStep('code');
      } else if (step === 'code') {
        if (!/^\d{6}$/.test(code)) {
          setError('请输入 6 位验证码');
          return;
        }
        const response = await authApi.verifyPasswordReset(challengeId, code);
        setResetToken(response.reset_token);
        setStep('password');
      } else if (step === 'password') {
        if (password.length < 8 || password.length > 64 || !/[a-z]/.test(password) || !/[A-Z]/.test(password) || !/\d/.test(password)) {
          setError('新密码需为 8-64 位，并同时包含大小写字母和数字');
          return;
        }
        if (password !== confirmPassword) {
          setError('两次输入的密码不一致');
          return;
        }
        await authApi.confirmPasswordReset(resetToken, password);
        setStep('done');
      }
    } catch (reason) {
      setError(catchError(reason, '操作失败，请稍后再试'));
    } finally {
      setLoading(false);
    }
  };

  if (step === 'done') {
    return (
      <div className="auth-form-container auth-complete">
        <CheckCircle2 size={42} />
        <h1 className="auth-form-title">密码已更新</h1>
        <p className="auth-form-subtitle">所有旧会话均已退出，请使用新密码重新登录。</p>
        <button className="form-submit" onClick={() => navigate('/auth/login')}>返回登录</button>
      </div>
    );
  }

  const stepIndex = step === 'account' ? 1 : step === 'code' ? 2 : 3;

  return (
    <div className="auth-form-container">
      <h1 className="auth-form-title">找回密码</h1>
      <p className="auth-form-subtitle">通过已验证的联系方式重置账号密码</p>

      <div className="auth-steps auth-steps-three" aria-label="找回密码进度">
        <span className="active">1 账号</span>
        <span className={stepIndex >= 2 ? 'active' : ''}>2 验证</span>
        <span className={stepIndex >= 3 ? 'active' : ''}>3 新密码</span>
      </div>

      {error && <div className="auth-error" role="alert">{error}</div>}

      <form className="auth-form" onSubmit={submit} noValidate>
        {step === 'account' && (
          <>
            <div className="verification-channel" aria-label="验证方式">
              <button type="button" aria-pressed={channel === 'EMAIL'} className={channel === 'EMAIL' ? 'active' : ''} onClick={() => setChannel('EMAIL')}><Mail size={17} />邮箱</button>
              <button type="button" aria-pressed={channel === 'SMS'} className={channel === 'SMS' ? 'active' : ''} onClick={() => setChannel('SMS')}><MessageSquareText size={17} />短信</button>
            </div>
            <div className="form-group">
              <label htmlFor="reset-account" className="form-label">手机号或邮箱</label>
              <input id="reset-account" className="form-input" value={account} onChange={(event) => setAccount(event.target.value)} autoComplete="username" disabled={loading} autoFocus />
              <p className="form-help">为保护账号安全，无论账号是否存在，页面都会进入下一步。</p>
            </div>
          </>
        )}

        {step === 'code' && (
          <div className="verification-stage">
            <div>
              <h2>输入验证码</h2>
              <p>若账号存在且已完成该方式验证，验证码将在 5 分钟内送达。</p>
            </div>
            {mockCode && <div className="mock-code">本地测试验证码：<strong>{mockCode}</strong></div>}
            <div className="form-group">
              <label htmlFor="reset-code" className="form-label">6 位验证码</label>
              <input id="reset-code" className="form-input verification-code" inputMode="numeric" autoComplete="one-time-code" maxLength={6} value={code} onChange={(event) => setCode(event.target.value.replace(/\D/g, ''))} disabled={loading} autoFocus />
            </div>
            <button type="button" className="form-link-button align-start" onClick={() => setStep('account')} disabled={loading}>返回修改账号</button>
          </div>
        )}

        {step === 'password' && (
          <>
            <div className="form-group">
              <label htmlFor="new-password" className="form-label">新密码</label>
              <div className="form-input-wrapper">
                <input id="new-password" className="form-input" type={showPassword ? 'text' : 'password'} value={password} onChange={(event) => setPassword(event.target.value)} autoComplete="new-password" placeholder="8-64 位，包含大小写字母和数字" disabled={loading} autoFocus />
                <button type="button" className="form-input-suffix" onClick={() => setShowPassword((value) => !value)} aria-label={showPassword ? '隐藏密码' : '显示密码'}>{showPassword ? <EyeOff size={18} /> : <Eye size={18} />}</button>
              </div>
            </div>
            <div className="form-group">
              <label htmlFor="confirm-new-password" className="form-label">确认新密码</label>
              <input id="confirm-new-password" className="form-input" type="password" value={confirmPassword} onChange={(event) => setConfirmPassword(event.target.value)} autoComplete="new-password" disabled={loading} />
            </div>
          </>
        )}

        <button type="submit" className="form-submit" disabled={loading}>
          {loading ? <><Loader2 size={18} className="spinner" />处理中</> : step === 'account' ? '发送验证码' : step === 'code' ? '验证验证码' : '更新密码'}
        </button>
      </form>

      <p className="auth-form-footer"><Link to="/auth/login" className="form-link">返回登录</Link></p>
    </div>
  );
};

export default ForgotPassword;
