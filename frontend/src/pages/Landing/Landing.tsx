import { useState, useEffect } from 'react';
import { motion } from 'framer-motion';
import {
  Music,
  Trophy,
  MapPin,
  Film,
  Zap,
  Shield,
  Activity,
  Layers,
  Sun,
  Moon,
  Menu,
  X
} from 'lucide-react';
import './Landing.css';

const Landing = () => {
  const [theme, setTheme] = useState<'light' | 'dark'>('light');
  const [mobileMenuOpen, setMobileMenuOpen] = useState(false);
  const [scrolled, setScrolled] = useState(false);

  useEffect(() => {
    const handleScroll = () => {
      setScrolled(window.scrollY > 50);
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

  const features = [
    {
      icon: Music,
      title: '演出票务',
      description: '音乐会、话剧、展览，精彩不容错过'
    },
    {
      icon: Trophy,
      title: '赛事票务',
      description: '体育赛事、电竞比赛，实时库存更新'
    },
    {
      icon: MapPin,
      title: '景区门票',
      description: '热门景点、主题乐园，提前预订免排队'
    },
    {
      icon: Film,
      title: '电影票务',
      description: '院线新片、特殊场次，座位实时选择'
    }
  ];

  const advantages = [
    {
      icon: Zap,
      title: '高性能',
      description: '单实例 QPS > 10,000，P99 延迟 < 100ms',
      metric: '10K+ QPS'
    },
    {
      icon: Shield,
      title: '安全可靠',
      description: '企业级会话管理，事务保证防超卖',
      metric: '0 超卖'
    },
    {
      icon: Activity,
      title: '实时监控',
      description: 'Prometheus Metrics，健康检查，可观测性完整',
      metric: '99.9% 可用'
    },
    {
      icon: Layers,
      title: '水平扩展',
      description: 'Redis Session 持久化，支持多实例部署',
      metric: '无限扩展'
    }
  ];

  const steps = [
    {
      number: '01',
      title: '注册账号',
      description: '手机号快速注册，30 秒完成'
    },
    {
      number: '02',
      title: '浏览票务',
      description: '实时库存，详细信息，一目了然'
    },
    {
      number: '03',
      title: '在线预订',
      description: '安全支付，即时确认，订单管理'
    }
  ];

  return (
    <div className="landing">
      {/* Navigation */}
      <motion.nav
        className={`nav ${scrolled ? 'nav-scrolled' : ''}`}
        initial={{ y: -100 }}
        animate={{ y: 0 }}
        transition={{ duration: 0.3 }}
      >
        <div className="nav-container">
          <a href="/" className="nav-brand">
            <span className="nav-logo-text">HyperTicket</span>
          </a>

          <div className={`nav-links ${mobileMenuOpen ? 'nav-links-open' : ''}`}>
            <a href="#features" onClick={() => setMobileMenuOpen(false)}>功能</a>
            <a href="#advantages" onClick={() => setMobileMenuOpen(false)}>优势</a>
            <a href="#how-it-works" onClick={() => setMobileMenuOpen(false)}>使用流程</a>
            <button className="btn-secondary" onClick={() => setMobileMenuOpen(false)}>
              登录
            </button>
            <button className="btn-primary" onClick={() => setMobileMenuOpen(false)}>
              开始预订
            </button>
          </div>

          <div className="nav-actions">
            <button
              className="theme-toggle"
              onClick={toggleTheme}
              aria-label="切换主题"
            >
              {theme === 'light' ? <Moon size={20} /> : <Sun size={20} />}
            </button>

            <button
              className="mobile-menu-toggle"
              onClick={() => setMobileMenuOpen(!mobileMenuOpen)}
              aria-label="菜单"
            >
              {mobileMenuOpen ? <X size={24} /> : <Menu size={24} />}
            </button>
          </div>
        </div>
      </motion.nav>

      {/* Hero Section */}
      <section className="hero">
        <div className="hero-bg">
          <div className="hero-shape hero-shape-1"></div>
          <div className="hero-shape hero-shape-2"></div>
          <div className="hero-shape hero-shape-3"></div>
        </div>

        <div className="hero-content">
          <motion.h1
            className="hero-title"
            initial={{ opacity: 0, y: 20 }}
            animate={{ opacity: 1, y: 0 }}
            transition={{ duration: 0.3, delay: 0 }}
          >
            演出、赛事、景区、电影，一站式预订体验
          </motion.h1>

          <motion.p
            className="hero-subtitle"
            initial={{ opacity: 0, y: 20 }}
            animate={{ opacity: 1, y: 0 }}
            transition={{ duration: 0.3, delay: 0.1 }}
          >
            Concerts, Sports, Attractions, Movies - Your One-Stop Booking Experience
          </motion.p>

          <motion.div
            className="hero-cta"
            initial={{ opacity: 0, y: 20 }}
            animate={{ opacity: 1, y: 0 }}
            transition={{ duration: 0.3, delay: 0.2 }}
          >
            <button className="btn-primary btn-large">开始预订</button>
            <button className="btn-secondary btn-large">了解技术</button>
          </motion.div>
        </div>
      </section>

      {/* Features Section */}
      <section id="features" className="section features">
        <div className="container">
          <h2 className="section-title">核心功能</h2>
          <p className="section-subtitle">覆盖多种票务场景，满足不同需求</p>

          <div className="features-grid">
            {features.map((feature, index) => (
              <motion.div
                key={feature.title}
                className="feature-card"
                initial={{ opacity: 0, y: 20 }}
                whileInView={{ opacity: 1, y: 0 }}
                viewport={{ once: true, margin: "-100px" }}
                transition={{ duration: 0.3, delay: index * 0.05 }}
              >
                <div className="feature-icon">
                  <feature.icon size={32} />
                </div>
                <h3 className="feature-title">{feature.title}</h3>
                <p className="feature-description">{feature.description}</p>
              </motion.div>
            ))}
          </div>
        </div>
      </section>

      {/* Advantages Section */}
      <section id="advantages" className="section advantages">
        <div className="container">
          <h2 className="section-title">技术优势</h2>
          <p className="section-subtitle">企业级性能与可靠性保障</p>

          <div className="advantages-list">
            {advantages.map((advantage, index) => (
              <motion.div
                key={advantage.title}
                className={`advantage-item ${index % 2 === 1 ? 'advantage-item-reverse' : ''}`}
                initial={{ opacity: 0, x: index % 2 === 0 ? -20 : 20 }}
                whileInView={{ opacity: 1, x: 0 }}
                viewport={{ once: true, margin: "-100px" }}
                transition={{ duration: 0.3 }}
              >
                <div className="advantage-content">
                  <div className="advantage-icon">
                    <advantage.icon size={40} />
                  </div>
                  <h3 className="advantage-title">{advantage.title}</h3>
                  <p className="advantage-description">{advantage.description}</p>
                </div>
                <div className="advantage-metric">
                  <span className="metric-value">{advantage.metric}</span>
                </div>
              </motion.div>
            ))}
          </div>
        </div>
      </section>

      {/* How It Works Section */}
      <section id="how-it-works" className="section how-it-works">
        <div className="container">
          <h2 className="section-title">使用流程</h2>
          <p className="section-subtitle">三步开始您的预订之旅</p>

          <div className="steps">
            {steps.map((step, index) => (
              <motion.div
                key={step.number}
                className="step"
                initial={{ opacity: 0, y: 20 }}
                whileInView={{ opacity: 1, y: 0 }}
                viewport={{ once: true, margin: "-100px" }}
                transition={{ duration: 0.3, delay: index * 0.1 }}
              >
                <div className="step-number">{step.number}</div>
                <h3 className="step-title">{step.title}</h3>
                <p className="step-description">{step.description}</p>
              </motion.div>
            ))}
          </div>
        </div>
      </section>

      {/* Final CTA Section */}
      <section className="section cta-section">
        <div className="container">
          <motion.div
            className="cta-box"
            initial={{ opacity: 0, y: 20 }}
            whileInView={{ opacity: 1, y: 0 }}
            viewport={{ once: true }}
            transition={{ duration: 0.3 }}
          >
            <h2 className="cta-title">立即开始使用 HyperTicket</h2>
            <p className="cta-subtitle">加入我们，体验高效可靠的票务管理</p>
            <button className="btn-primary btn-large">免费注册</button>
          </motion.div>
        </div>
      </section>

      {/* Footer */}
      <footer className="footer">
        <div className="container">
          <div className="footer-content">
            <div className="footer-logo">HyperTicket</div>
            <div className="footer-links">
              <a href="#about">关于我们</a>
              <a href="#docs">技术文档</a>
              <a href="#contact">联系我们</a>
            </div>
          </div>
          <div className="footer-copyright">
            <span>&copy; 2026 HyperTicket - </span>
            <a href="https://www.shanchuann.cn/" target="_blank" rel="noopener noreferrer">山川不念旧</a>
          </div>
        </div>
      </footer>
    </div>
  );
};

export default Landing;
