import { useState, useEffect } from 'react';
import { CheckCircle2, CircleAlert, LoaderCircle } from 'lucide-react';
import wsClient from '../api/client';
import './ConnectionStatus.css';

const ConnectionStatus = () => {
  const [state, setState] = useState<'checking' | 'connected' | 'disconnected'>('checking');

  useEffect(() => {
    const check = () => {
      wsClient.isConnected()
        .then(connected => setState(connected ? 'connected' : 'disconnected'))
        .catch(() => setState('disconnected'));
    };
    check();
    const timer = setInterval(check, 3000);
    return () => clearInterval(timer);
  }, []);

  return (
    <div className={`connection-status ${state}`} title={state === 'connected' ? '客户端已连接到 HyperTicket 服务' : '正在检查服务连接'} role="status" aria-live="polite">
      {state === 'connected' ? <CheckCircle2 size={15} /> : state === 'checking' ? <LoaderCircle size={15} className="connection-spin" /> : <CircleAlert size={15} />}
      <span className="connection-label">{state === 'connected' ? '服务在线' : state === 'checking' ? '检查连接' : '服务离线'}</span>
    </div>
  );
};

export default ConnectionStatus;
