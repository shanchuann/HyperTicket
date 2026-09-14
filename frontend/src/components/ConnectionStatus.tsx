import { useState, useEffect } from 'react';
import wsClient from '../api/client';
import './ConnectionStatus.css';

const ConnectionStatus = () => {
  const [isConnected, setIsConnected] = useState(false);

  useEffect(() => {
    const check = () => setIsConnected(wsClient.isConnected());
    check();
    const id = setInterval(check, 1000);
    if (!wsClient.isConnected()) wsClient.connect().catch(() => {});
    return () => clearInterval(id);
  }, []);

  if (isConnected) {
    return (
      <div className="ws-status connected" title="服务连接正常">
        <span className="ws-dot" />
        <span>已连接</span>
      </div>
    );
  }

  return (
    <div className="ws-status disconnected" title="服务未连接">
      <span className="ws-dot" />
      <span>未连接</span>
      <button className="ws-reconnect" onClick={() => wsClient.connect().catch(() => {})}>
        重连
      </button>
    </div>
  );
};

export default ConnectionStatus;
