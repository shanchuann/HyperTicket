import { useState, useEffect } from 'react';
import { Outlet, Link, useNavigate, useLocation } from 'react-router-dom';
import { Sun, Moon, Ticket, Users, BarChart3, Settings, LogOut, Menu, X } from 'lucide-react';
import './AdminLayout.css';

const AdminLayout = () => {
  const navigate = useNavigate();
  const location = useLocation();
  const [theme, setTheme] = useState<'light' | 'dark'>('light');
  const [sidebarOpen, setSidebarOpen] = useState(true);
  const [mobileSidebarOpen, setMobileSidebarOpen] = useState(false);

  useEffect(() => {
    document.documentElement.setAttribute('data-theme', theme);
  }, [theme]);

  const toggleTheme = () => {
    setTheme(prev => prev === 'light' ? 'dark' : 'light');
  };

  const handleLogout = () => {
    localStorage.removeItem('token');
    localStorage.removeItem('user');
    navigate('/auth/login');
  };

  const navItems = [
    { path: '/admin', label: '概览', icon: BarChart3 },
    { path: '/admin/tickets', label: '票务管理', icon: Ticket },
    { path: '/admin/users', label: '用户管理', icon: Users },
    { path: '/admin/settings', label: '系统设置', icon: Settings },
  ];

  const isActive = (path: string) => {
    if (path === '/admin') {
      return location.pathname === '/admin';
    }
    return location.pathname.startsWith(path);
  };

  return (
    <div className="admin-layout">
      {/* Desktop Sidebar */}
      <aside className={`admin-sidebar ${sidebarOpen ? '' : 'collapsed'}`}>
        <div className="admin-sidebar-header">
          <Link to="/admin" className="admin-logo">
            {sidebarOpen ? 'HyperTicket' : 'HT'}
          </Link>
          <button
            className="admin-sidebar-toggle"
            onClick={() => setSidebarOpen(!sidebarOpen)}
            aria-label={sidebarOpen ? '收起侧边栏' : '展开侧边栏'}
          >
            {sidebarOpen ? '←' : '→'}
          </button>
        </div>

        <nav className="admin-nav">
          {navItems.map(item => (
            <Link
              key={item.path}
              to={item.path}
              className={`admin-nav-item ${isActive(item.path) ? 'active' : ''}`}
            >
              <item.icon size={20} />
              {sidebarOpen && <span>{item.label}</span>}
            </Link>
          ))}
        </nav>

        <div className="admin-sidebar-footer">
          <button
            className="admin-nav-item"
            onClick={toggleTheme}
          >
            {theme === 'light' ? <Moon size={20} /> : <Sun size={20} />}
            {sidebarOpen && <span>切换主题</span>}
          </button>
          <button
            className="admin-nav-item"
            onClick={handleLogout}
          >
            <LogOut size={20} />
            {sidebarOpen && <span>退出登录</span>}
          </button>
        </div>
      </aside>

      {/* Mobile Sidebar Overlay */}
      {mobileSidebarOpen && (
        <div
          className="admin-mobile-overlay"
          onClick={() => setMobileSidebarOpen(false)}
        />
      )}

      {/* Mobile Sidebar */}
      <aside className={`admin-mobile-sidebar ${mobileSidebarOpen ? 'open' : ''}`}>
        <div className="admin-sidebar-header">
          <Link to="/admin" className="admin-logo" onClick={() => setMobileSidebarOpen(false)}>
            HyperTicket
          </Link>
          <button
            className="admin-sidebar-close"
            onClick={() => setMobileSidebarOpen(false)}
            aria-label="关闭菜单"
          >
            <X size={24} />
          </button>
        </div>

        <nav className="admin-nav">
          {navItems.map(item => (
            <Link
              key={item.path}
              to={item.path}
              className={`admin-nav-item ${isActive(item.path) ? 'active' : ''}`}
              onClick={() => setMobileSidebarOpen(false)}
            >
              <item.icon size={20} />
              <span>{item.label}</span>
            </Link>
          ))}
        </nav>

        <div className="admin-sidebar-footer">
          <button
            className="admin-nav-item"
            onClick={() => {
              toggleTheme();
              setMobileSidebarOpen(false);
            }}
          >
            {theme === 'light' ? <Moon size={20} /> : <Sun size={20} />}
            <span>切换主题</span>
          </button>
          <button
            className="admin-nav-item"
            onClick={handleLogout}
          >
            <LogOut size={20} />
            <span>退出登录</span>
          </button>
        </div>
      </aside>

      {/* Main Content */}
      <main className="admin-main">
        {/* Mobile Header */}
        <header className="admin-mobile-header">
          <button
            className="admin-mobile-menu-btn"
            onClick={() => setMobileSidebarOpen(true)}
            aria-label="打开菜单"
          >
            <Menu size={24} />
          </button>
          <span className="admin-mobile-title">HyperTicket 管理后台</span>
          <div className="admin-mobile-spacer"></div>
        </header>

        <div className="admin-content">
          <Outlet />
        </div>
      </main>
    </div>
  );
};

export default AdminLayout;
