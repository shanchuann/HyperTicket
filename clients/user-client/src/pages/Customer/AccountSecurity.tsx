import { useCallback, useEffect, useState } from 'react';
import { CheckCircle2, KeyRound, Loader2, Mail, MessageSquareText, RefreshCw, Send, ShieldCheck } from 'lucide-react';
import { authApi } from '../../api/auth';
import { toChineseError } from '../../api/errors';
import { useAuth } from '../../hooks/useAuth';
import { useCountdown } from '../../hooks/useCountdown';
import type { SecurityStatusResponse, VerificationChannel } from '../../types';
import Toast from '../../components/Toast';
import '../Auth/AuthForms.css';
import './AccountSecurity.css';

export default function AccountSecurity() {
  const { token } = useAuth();
  const cooldown = useCountdown();
  const [status, setStatus] = useState<SecurityStatusResponse | null>(null);
  const [channel, setChannel] = useState<VerificationChannel>('EMAIL');
  const [email, setEmail] = useState('');
  const [password, setPassword] = useState('');
  const [code, setCode] = useState('');
  const [challengeId, setChallengeId] = useState('');
  const [mockCode, setMockCode] = useState('');
  const [busy, setBusy] = useState(false);
  const [toast, setToast] = useState<{ message: string; tone: 'success' | 'error' }>({ message: '', tone: 'success' });

  const loadStatus = useCallback(async () => {
    if (!token) return;
    setBusy(true);
    try { const response = await authApi.securityStatus(token); setStatus(response); setEmail(response.email || ''); }
    catch (e) { setToast({ message: toChineseError(e, '无法读取账号安全状态'), tone: 'error' }); }
    finally { setBusy(false); }
  }, [token]);

  useEffect(() => { void loadStatus(); }, [loadStatus]);

  const resetChallenge = () => { setChallengeId(''); setCode(''); setMockCode(''); cooldown.reset(); };
  const requestCode = async () => {
    if (!token) return;
    if (!password) { setToast({ message: '请输入当前登录密码', tone: 'error' }); return; }
    if (channel === 'EMAIL' && !/^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(email)) { setToast({ message: '请输入有效的邮箱地址', tone: 'error' }); return; }
    setBusy(true);
    try {
      const response = await authApi.requestContactVerification(token, password, channel, email);
      setChallengeId(response.challenge_id); setMockCode(response.mock_code || ''); cooldown.start(60);
      setToast({ message: '验证码已发送，请在 5 分钟内完成验证', tone: 'success' });
    } catch (e) { setToast({ message: toChineseError(e, '验证码发送失败'), tone: 'error' }); }
    finally { setBusy(false); }
  };

  const confirmCode = async () => {
    if (!token || !/^\d{6}$/.test(code)) { setToast({ message: '请输入 6 位数字验证码', tone: 'error' }); return; }
    setBusy(true);
    try {
      await authApi.confirmContactVerification(token, challengeId, code);
      resetChallenge(); setPassword(''); await loadStatus();
      setToast({ message: channel === 'EMAIL' ? '邮箱已验证并绑定' : '手机号验证完成', tone: 'success' });
    } catch (e) { setToast({ message: toChineseError(e, '验证码校验失败'), tone: 'error' }); }
    finally { setBusy(false); }
  };

  return <div className="security-page">
    <Toast {...toast} onClose={() => setToast(previous => ({ ...previous, message: '' }))}/>
    <header className="security-page-head"><div><span className="security-kicker"><ShieldCheck size={17}/>账号保护</span><h1>账号安全</h1><p>验证恢复联系方式，确保忘记密码时可以找回账号。</p></div><button className="security-refresh" onClick={loadStatus} disabled={busy}><RefreshCw size={16}/>刷新状态</button></header>
    <section className="security-status" aria-busy={busy}>
      <div><span className={status?.phone_verified ? 'verified' : 'pending'}>{status?.phone_verified ? <CheckCircle2 size={18}/> : <MessageSquareText size={18}/>}</span><div><b>登录手机号</b><small>{status?.usertel ? `${status.usertel.slice(0, 3)}****${status.usertel.slice(-4)}` : '读取中'}</small></div><strong>{status?.phone_verified ? '已验证' : '未验证'}</strong></div>
      <div><span className={status?.email_verified ? 'verified' : 'pending'}>{status?.email_verified ? <CheckCircle2 size={18}/> : <Mail size={18}/>}</span><div><b>恢复邮箱</b><small>{status?.email || '尚未绑定邮箱'}</small></div><strong>{status?.email_verified ? '已验证' : '未验证'}</strong></div>
    </section>
    <section className="security-form-section">
      <div className="security-form-copy"><KeyRound size={22}/><h2>验证恢复方式</h2><p>为保护账号，发送验证码前需要再次输入当前密码。</p></div>
      <div className="security-form">
        <div className="channel-switch"><button type="button" className={channel === 'EMAIL' ? 'active' : ''} onClick={() => { setChannel('EMAIL'); resetChallenge(); }}><Mail size={16}/>验证邮箱</button><button type="button" className={channel === 'SMS' ? 'active' : ''} onClick={() => { setChannel('SMS'); resetChallenge(); }}><MessageSquareText size={16}/>模拟短信</button></div>
        {channel === 'SMS' && <p className="verification-note">仅用于已启用 Mock SMS 的本地开发环境。</p>}
        {channel === 'EMAIL' && <label className="form-group"><span className="form-label">邮箱地址</span><input className="form-input" type="email" value={email} onChange={e => { setEmail(e.target.value); resetChallenge(); }} autoComplete="email" disabled={!!challengeId}/></label>}
        <label className="form-group"><span className="form-label">当前密码</span><input className="form-input" type="password" value={password} onChange={e => { setPassword(e.target.value); resetChallenge(); }} autoComplete="current-password" disabled={!!challengeId}/></label>
        {!challengeId ? <button className="form-submit" type="button" onClick={requestCode} disabled={busy}>{busy ? <Loader2 size={18} className="spinner"/> : <Send size={18}/>}发送验证码</button> : <>
          <label className="form-group"><span className="form-label">验证码</span><input className="form-input code-input-large" value={code} onChange={e => setCode(e.target.value.replace(/\D/g, '').slice(0, 6))} inputMode="numeric" autoComplete="one-time-code" autoFocus/></label>
          {mockCode && <p className="dev-code">本地模拟验证码：<b>{mockCode}</b></p>}
          <button className="form-submit" type="button" onClick={confirmCode} disabled={busy}>{busy ? <Loader2 size={18} className="spinner"/> : <CheckCircle2 size={18}/>}完成验证</button>
          <button className="resend-link centered" type="button" onClick={requestCode} disabled={cooldown.seconds > 0 || busy}>{cooldown.seconds > 0 ? `${cooldown.seconds} 秒后可重新发送` : '重新发送验证码'}</button>
        </>}
      </div>
    </section>
  </div>;
}
