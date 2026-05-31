import { useState, useEffect } from 'react';
import { Outlet, Link, useNavigate, useLocation } from 'react-router-dom';
import { Sun, Moon, User, Ticket, ShoppingBag, LogOut, Menu, X } from 'lucide-react';
import './CustomerLayout.css';

const CustomerLayout = () => {
  const navigate = useNavigate();
  const location = useLocation();
  const [theme, setTheme] = useState<'light' | 'dark'>('light');
  const [mobileMenuOpen, setMobileMenuOpen] = useState(false);
  const [scrolled, setScrolled] = useState(false);

  useEffect(() => {
    const handleScroll = () => {
      setScrolled(window.scrollY > 10);
    };
    window.addEventListener('scroll', handleScroll);
    return () => window.removeEventListener('scroll', handleScroll);
  }, []);

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
    { path: '/customer', label: '票务浏览', icon: Ticket },
    { path: '/customer/orders', label: '我的订单', icon: ShoppingBag },
    { path: '/customer/profile', label: '个人中心', icon: User },
  ];

  const isActive = (path: string) => {
    return location.pathname === path || location.pathname.startsWith(path + '/');
  };

  return (
    <div className="customer-layout">
      {/* Header */}
      <header className={`customer-header ${scrolled ? 'scrolled' : ''}`}>
        <div className="customer-header-container">
          <Link to="/customer" className="customer-logo">
            HyperTicket
          </Link>

          {/* Desktop Navigation */}
          <nav className="customer-nav">
            {navItems.map(item => (
              <Link
                key={item.path}
                to={item.path}
                className={`customer-nav-item ${isActive(item.path) ? 'active' : ''}`}
              >
                <item.icon size={18} />
                <span>{item.label}</span>
              </Link>
            ))}
          </nav>

          {/* Desktop Actions */}
          <div className="customer-actions">
            <button
              className="customer-theme-toggle"
              onClick={toggleTheme}
              aria-label="切换主题"
            >
              {theme === 'light' ? <Moon size={20} /> : <Sun size={20} />}
            </button>
            <button
              className="customer-logout"
              onClick={handleLogout}
              aria-label="退出登录"
            >
              <LogOut size={20} />
            </button>
          </div>

          {/* Mobile Menu Toggle */}
          <button
            className="customer-mobile-toggle"
            onClick={() => setMobileMenuOpen(!mobileMenuOpen)}
            aria-label="菜单"
          >
            {mobileMenuOpen ? <X size={24} /> : <Menu size={24} />}
          </button>
        </div>
      </header>

      {/* Mobile Navigation */}
      {mobileMenuOpen && (
        <div className="customer-mobile-menu">
          <nav className="customer-mobile-nav">
            {navItems.map(item => (
              <Link
                key={item.path}
                to={item.path}
                className={`customer-mobile-nav-item ${isActive(item.path) ? 'active' : ''}`}
                onClick={() => setMobileMenuOpen(false)}
              >
                <item.icon size={20} />
                <span>{item.label}</span>
              </Link>
            ))}
          </nav>
          <div className="customer-mobile-actions">
            <button className="customer-mobile-action" onClick={toggleTheme}>
              {theme === 'light' ? <Moon size={20} /> : <Sun size={20} />}
              <span>切换主题</span>
            </button>
            <button className="customer-mobile-action" onClick={handleLogout}>
              <LogOut size={20} />
              <span>退出登录</span>
            </button>
          </div>
        </div>
      )}

      {/* Main Content */}
      <main className="customer-main">
        <Outlet />
      </main>
    </div>
  );
};

export default CustomerLayout;
