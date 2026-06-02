import { useState, useEffect } from 'react';
import { invoke } from '@tauri-apps/api/core';
import './ConnectionStatus.css';

const ConnectionStatus = () => {
  const [connected, setConnected] = useState(false);

  useEffect(() => {
    const check = () => {
      invoke<boolean>('check_connection')
        .then(setConnected)
        .catch(() => setConnected(false));
    };
    check();
    const timer = setInterval(check, 3000);
    return () => clearInterval(timer);
  }, []);

  return (
    <div className={`connection-status ${connected ? 'connected' : 'disconnected'}`}>
      <span className="connection-dot" />
      <span className="connection-label">{connected ? '已连接' : '未连接'}</span>
    </div>
  );
};

export default ConnectionStatus;
