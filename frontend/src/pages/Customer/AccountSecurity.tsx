import { useCallback, useEffect, useState } from 'react';
import { CheckCircle2, Loader2, Mail, RefreshCw, ShieldCheck, Smartphone } from 'lucide-react';
import { authApi } from '../../api/auth';
import type { AccountSecurityStatus, VerificationChannel } from '../../types';
import { catchError } from '../../utils/errors';
import './AccountSecurity.css';

type PendingChallenge = {
  id: string;
  channel: VerificationChannel;
  mockCode?: string;
};

const AccountSecurityPage = () => {
  const token = localStorage.getItem('token') || '';
  const [status, setStatus] = useState<AccountSecurityStatus | null>(null);
  const [channel, setChannel] = useState<VerificationChannel>('EMAIL');
  const [email, setEmail] = useState('');
  const [password, setPassword] = useState('');
  const [code, setCode] = useState('');
  const [pending, setPending] = useState<PendingChallenge | null>(null);
  const [loading, setLoading] = useState(true);
  const [submitting, setSubmitting] = useState(false);
  const [error, setError] = useState('');
  const [success, setSuccess] = useState('');

  const loadStatus = useCallback(async () => {
    setLoading(true);
    setError('');
    try {
      const response = await authApi.getSecurityStatus(token);
      setStatus(response);
      setEmail(response.email || '');
    } catch (reason) {
      setError(catchError(reason, '账号安全状态加载失败'));
    } finally {
      setLoading(false);
    }
  }, [token]);

  useEffect(() => {
    let active = true;
    authApi.getSecurityStatus(token)
      .then((response) => {
        if (!active) return;
        setStatus(response);
        setEmail(response.email || '');
      })
      .catch((reason: unknown) => {
        if (active) setError(catchError(reason, '账号安全状态加载失败'));
      })
      .finally(() => {
        if (active) setLoading(false);
      });
    return () => { active = false; };
  }, [token]);

  const requestCode = async (event: React.FormEvent) => {
    event.preventDefault();
    setError('');
    setSuccess('');
    if (!password) {
      setError('请输入当前密码以确认本人操作');
      return;
    }
    if (channel === 'EMAIL' && !/^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(email)) {
      setError('请输入有效的邮箱地址');
      return;
    }
    setSubmitting(true);
    try {
      const response = await authApi.requestContactVerification(
        token,
        password,
        channel,
        channel === 'EMAIL' ? email.trim() : undefined,
      );
      setPending({ id: response.challenge_id, channel, mockCode: response.mock_code });
      setPassword('');
      setCode('');
      setSuccess(`验证码已发送至${channel === 'EMAIL' ? '邮箱' : '当前手机号'}`);
    } catch (reason) {
      setError(catchError(reason, '验证码发送失败，请稍后重试'));
    } finally {
      setSubmitting(false);
    }
  };

  const confirmCode = async (event: React.FormEvent) => {
    event.preventDefault();
    if (!pending || !/^\d{6}$/.test(code)) {
      setError('请输入 6 位验证码');
      return;
    }
    setSubmitting(true);
    setError('');
    setSuccess('');
    try {
      const response = await authApi.confirmContactVerification(token, pending.id, code);
      setStatus(response);
      setEmail(response.email || '');
      setPending(null);
      setCode('');
      setSuccess(pending.channel === 'EMAIL' ? '邮箱已验证并绑定' : '手机号已完成验证');
      const savedUser = localStorage.getItem('user');
      if (savedUser) {
        const user = JSON.parse(savedUser) as Record<string, unknown>;
        localStorage.setItem('user', JSON.stringify({
          ...user,
          email: response.email,
          emailVerified: response.email_verified,
          phoneVerified: response.phone_verified,
        }));
      }
    } catch (reason) {
      setError(catchError(reason, '验证失败，请重新输入'));
    } finally {
      setSubmitting(false);
    }
  };

  if (loading) {
    return <div className="security-page security-loading"><Loader2 className="spinner" size={24} /><span>正在读取账号安全状态</span></div>;
  }

  return (
    <div className="security-page">
      <header className="security-header">
        <div>
          <h1>账号安全</h1>
          <p>管理用于身份验证和密码找回的联系方式。</p>
        </div>
        <button className="security-refresh" onClick={() => void loadStatus()} aria-label="刷新安全状态"><RefreshCw size={18} /></button>
      </header>

      {error && <div className="security-message error" role="alert">{error}</div>}
      {success && <div className="security-message success" role="status"><CheckCircle2 size={18} />{success}</div>}

      <section className="security-overview" aria-label="账号安全概览">
        <div className="security-identity">
          <ShieldCheck size={28} />
          <div><span>登录手机号</span><strong>{status?.usertel || '-'}</strong></div>
        </div>
        <div className="security-status-row">
          <div>
            <Mail size={20} />
            <span>邮箱</span>
            <strong>{status?.email || '未绑定'}</strong>
            <em className={status?.email_verified ? 'verified' : 'unverified'}>{status?.email_verified ? '已验证' : '未验证'}</em>
          </div>
          <div>
            <Smartphone size={20} />
            <span>手机号</span>
            <strong>{status?.usertel || '-'}</strong>
            <em className={status?.phone_verified ? 'verified' : 'unverified'}>{status?.phone_verified ? '已验证' : '未验证'}</em>
          </div>
        </div>
      </section>

      <section className="security-workflow">
        <div className="security-workflow-copy">
          <h2>{pending ? '输入验证码' : '验证联系方式'}</h2>
          <p>{pending ? '输入刚刚收到的 6 位验证码完成绑定。' : '发送验证码前需要输入当前密码，防止他人篡改账号信息。'}</p>
        </div>

        {!pending ? (
          <form className="security-form" onSubmit={requestCode}>
            <div className="verification-channel compact" aria-label="验证方式">
              <button type="button" aria-pressed={channel === 'EMAIL'} className={channel === 'EMAIL' ? 'active' : ''} onClick={() => setChannel('EMAIL')}><Mail size={17} />邮箱</button>
              <button type="button" aria-pressed={channel === 'SMS'} className={channel === 'SMS' ? 'active' : ''} onClick={() => setChannel('SMS')}><Smartphone size={17} />当前手机号</button>
            </div>
            {channel === 'EMAIL' && (
              <label className="security-field">邮箱地址<input className="form-input" type="email" value={email} onChange={(event) => setEmail(event.target.value)} autoComplete="email" disabled={submitting} /></label>
            )}
            <label className="security-field">当前密码<input className="form-input" type="password" value={password} onChange={(event) => setPassword(event.target.value)} autoComplete="current-password" disabled={submitting} /></label>
            <button className="form-submit security-submit" disabled={submitting}>{submitting ? <><Loader2 size={18} className="spinner" />发送中</> : '发送验证码'}</button>
          </form>
        ) : (
          <form className="security-form" onSubmit={confirmCode}>
            {pending.mockCode && <div className="mock-code">本地测试验证码：<strong>{pending.mockCode}</strong></div>}
            <label className="security-field">6 位验证码<input className="form-input verification-code" inputMode="numeric" autoComplete="one-time-code" maxLength={6} value={code} onChange={(event) => setCode(event.target.value.replace(/\D/g, ''))} disabled={submitting} autoFocus /></label>
            <div className="security-form-actions">
              <button type="button" className="form-link-button" onClick={() => { setPending(null); setCode(''); setError(''); }} disabled={submitting}>返回修改</button>
              <button className="form-submit security-submit" disabled={submitting}>{submitting ? <><Loader2 size={18} className="spinner" />验证中</> : '确认验证'}</button>
            </div>
          </form>
        )}
      </section>
    </div>
  );
};

export default AccountSecurityPage;
