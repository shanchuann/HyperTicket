import { useState, useEffect } from 'react';
import { Wifi, WifiOff } from 'lucide-react';
import wsClient from '../api/client';
import './ConnectionStatus.css';

const ConnectionStatus = () => {
  const [isConnected, setIsConnected] = useState(false);

  useEffect(() => {
    // 检查连接状态
    const checkConnection = () => {
      setIsConnected(wsClient.isConnected());
    };

    checkConnection();

    // 每秒检查一次
    const interval = setInterval(checkConnection, 1000);

    // 尝试自动连接
    if (!wsClient.isConnected()) {
      wsClient.connect().catch(() => {
        // 连接失败，状态会自动更新
      });
    }

    return () => clearInterval(interval);
  }, []);

  const handleConnect = () => {
    wsClient.connect().catch(console.error);
  };

  return (
    <div className={`connection-status ${isConnected ? 'connected' : 'disconnected'}`}>
      {isConnected ? (
        <>
          <Wifi size={16} />
          <span>已连接</span>
        </>
      ) : (
        <>
          <WifiOff size={16} />
          <span>未连接</span>
          <button onClick={handleConnect} className="connect-btn">
            重新连接
          </button>
        </>
      )}
    </div>
  );
};

export default ConnectionStatus;
