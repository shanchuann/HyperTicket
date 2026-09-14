import { Outlet, Link } from 'react-router-dom';
import { Sun, Moon, Zap, Shield, Activity } from 'lucide-react';
import { useTheme } from '../../hooks/useTheme';
import './AuthLayout.css';

const AuthLayout = () => {
  const { theme, toggleTheme } = useTheme();

  // 实测数据（WSL + Windows MySQL，10 并发，io_threads=1）
  const features = [
    { icon: Zap, title: '高性能', description: '实测 QPS 1,973，P50 延迟 1.8ms' },
    { icon: Shield, title: '安全可靠', description: '事务锁防超卖，bcrypt 加密，SQL 预处理' },
    { icon: Activity, title: '低延迟', description: '实测 P99 < 25ms（10 并发）' },
  ];

  return (
    <div className="auth-layout">
      <div className="auth-left">
        <header className="auth-header">
          <Link to="/" className="auth-logo">HyperTicket</Link>
          <button
            className="auth-theme-toggle"
            onClick={toggleTheme}
            aria-label="切换主题"
          >
            {theme === 'light' ? <Moon size={20} /> : <Sun size={20} />}
          </button>
        </header>

        <main className="auth-content">
          <Outlet />
        </main>
      </div>

      <div className="auth-right">
        <div className="auth-illustration">
          <p>为演出、赛事、景区、电影提供一站式预订解决方案</p>

          <div className="auth-features">
            {features.map((feature) => (
              <div key={feature.title} className="auth-feature">
                <div className="auth-feature-icon">
                  <feature.icon size={24} />
                </div>
                <div className="auth-feature-text">
                  <h3>{feature.title}</h3>
                  <p>{feature.description}</p>
                </div>
              </div>
            ))}
          </div>
        </div>
      </div>
    </div>
  );
};

export default AuthLayout;
