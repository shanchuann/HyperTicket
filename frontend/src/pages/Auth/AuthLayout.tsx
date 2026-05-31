import { useState, useEffect } from 'react';
import { Outlet, Link } from 'react-router-dom';
import { Sun, Moon, Zap, Shield, Activity } from 'lucide-react';
import './AuthLayout.css';

const AuthLayout = () => {
  const [theme, setTheme] = useState<'light' | 'dark'>('light');

  useEffect(() => {
    document.documentElement.setAttribute('data-theme', theme);
  }, [theme]);

  const toggleTheme = () => {
    setTheme(prev => prev === 'light' ? 'dark' : 'light');
  };

  const features = [
    {
      icon: Zap,
      title: '高性能',
      description: '单实例 QPS > 10,000'
    },
    {
      icon: Shield,
      title: '安全可靠',
      description: '企业级安全防护'
    },
    {
      icon: Activity,
      title: '实时监控',
      description: '99.9% 服务可用性'
    }
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
          <h2>企业级票务管理平台</h2>
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
