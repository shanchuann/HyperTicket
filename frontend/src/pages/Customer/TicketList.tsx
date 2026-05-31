import { useState, useEffect } from 'react';
import { Search, Calendar, MapPin, ChevronRight } from 'lucide-react';
import './TicketList.css';

interface Ticket {
  id: number;
  title: string;
  venue: string;
  event_date: string;
  available_seats: number;
  total_seats: number;
  category: 'concert' | 'sports' | 'attraction' | 'movie';
  price: number;
  status: number;
}

const TicketList = () => {
  const [tickets, setTickets] = useState<Ticket[]>([]);
  const [loading, setLoading] = useState(true);
  const [searchQuery, setSearchQuery] = useState('');
  const [selectedCategory, setSelectedCategory] = useState<string>('all');

  // 模拟数据
  const mockTickets: Ticket[] = [
    {
      id: 1,
      title: '2026 周杰伦演唱会',
      venue: '国家体育场（鸟巢）',
      event_date: '2026-08-15',
      available_seats: 1250,
      total_seats: 80000,
      category: 'concert',
      price: 580,
      status: 1
    },
    {
      id: 2,
      title: '2026 中超联赛',
      venue: '工人体育场',
      event_date: '2026-06-20',
      available_seats: 8500,
      total_seats: 68000,
      category: 'sports',
      price: 120,
      status: 1
    },
    {
      id: 3,
      title: '故宫博物院',
      venue: '故宫博物院',
      event_date: '2026-06-01',
      available_seats: 5000,
      total_seats: 8000,
      category: 'attraction',
      price: 60,
      status: 1
    },
    {
      id: 4,
      title: '复仇者联盟5',
      venue: '万达影城（CBD店）',
      event_date: '2026-07-10',
      available_seats: 45,
      total_seats: 200,
      category: 'movie',
      price: 65,
      status: 1
    },
    {
      id: 5,
      title: '张学友经典演唱会',
      venue: '上海梅赛德斯奔驰文化中心',
      event_date: '2026-09-01',
      available_seats: 3200,
      total_seats: 18000,
      category: 'concert',
      price: 480,
      status: 1
    },
    {
      id: 6,
      title: '环球影城',
      venue: '北京环球度假区',
      event_date: '2026-06-15',
      available_seats: 8000,
      total_seats: 50000,
      category: 'attraction',
      price: 418,
      status: 1
    }
  ];

  const categories = [
    { id: 'all', label: '全部' },
    { id: 'concert', label: '演出' },
    { id: 'sports', label: '赛事' },
    { id: 'attraction', label: '景区' },
    { id: 'movie', label: '电影' }
  ];

  useEffect(() => {
    // 模拟 API 调用
    const loadTickets = async () => {
      setLoading(true);
      await new Promise(resolve => setTimeout(resolve, 800));
      setTickets(mockTickets);
      setLoading(false);
    };
    loadTickets();
  }, []);

  const filteredTickets = tickets.filter(ticket => {
    const matchesSearch = ticket.title.toLowerCase().includes(searchQuery.toLowerCase()) ||
                         ticket.venue.toLowerCase().includes(searchQuery.toLowerCase());
    const matchesCategory = selectedCategory === 'all' || ticket.category === selectedCategory;
    return matchesSearch && matchesCategory;
  });

  const formatDate = (dateString: string) => {
    const date = new Date(dateString);
    return date.toLocaleDateString('zh-CN', {
      month: 'short',
      day: 'numeric',
      weekday: 'short'
    });
  };

  const getCategoryLabel = (category: string) => {
    const map: Record<string, string> = {
      concert: '演出',
      sports: '赛事',
      attraction: '景区',
      movie: '电影'
    };
    return map[category] || category;
  };

  if (loading) {
    return (
      <div className="ticket-list-container">
        <div className="ticket-list-loading">
          <div className="loading-spinner"></div>
          <p>加载中...</p>
        </div>
      </div>
    );
  }

  return (
    <div className="ticket-list-container">
      {/* Search and Filter */}
      <div className="ticket-list-header">
        <h1 className="ticket-list-title">票务浏览</h1>

        <div className="ticket-list-filters">
          <div className="ticket-search">
            <Search size={18} className="ticket-search-icon" />
            <input
              type="text"
              placeholder="搜索演出、场馆..."
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              className="ticket-search-input"
            />
          </div>

          <div className="ticket-categories">
            {categories.map(cat => (
              <button
                key={cat.id}
                className={`ticket-category-btn ${selectedCategory === cat.id ? 'active' : ''}`}
                onClick={() => setSelectedCategory(cat.id)}
              >
                {cat.label}
              </button>
            ))}
          </div>
        </div>
      </div>

      {/* Ticket Grid */}
      <div className="ticket-grid">
        {filteredTickets.map(ticket => (
          <div key={ticket.id} className="ticket-card">
            <div className="ticket-card-header">
              <span className="ticket-category">{getCategoryLabel(ticket.category)}</span>
              <span className="ticket-date">
                <Calendar size={14} />
                {formatDate(ticket.event_date)}
              </span>
            </div>

            <h3 className="ticket-title">{ticket.title}</h3>

            <div className="ticket-venue">
              <MapPin size={14} />
              <span>{ticket.venue}</span>
            </div>

            <div className="ticket-seats">
              <div className="ticket-seats-bar">
                <div
                  className="ticket-seats-fill"
                  style={{
                    width: `${((ticket.total_seats - ticket.available_seats) / ticket.total_seats) * 100}%`
                  }}
                />
              </div>
              <span className="ticket-seats-text">
                剩余 {ticket.available_seats} 张
              </span>
            </div>

            <div className="ticket-footer">
              <div className="ticket-price">
                <span className="ticket-price-symbol">¥</span>
                <span className="ticket-price-value">{ticket.price}</span>
                <span className="ticket-price-unit">起</span>
              </div>
              <button className="ticket-action">
                预订
                <ChevronRight size={16} />
              </button>
            </div>
          </div>
        ))}
      </div>

      {filteredTickets.length === 0 && (
        <div className="ticket-list-empty">
          <p>没有找到匹配的票务</p>
          <button
            className="ticket-clear-filter"
            onClick={() => {
              setSearchQuery('');
              setSelectedCategory('all');
            }}
          >
            清除筛选
          </button>
        </div>
      )}
    </div>
  );
};

export default TicketList;
