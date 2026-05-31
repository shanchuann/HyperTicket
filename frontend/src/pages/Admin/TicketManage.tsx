import { useState, useEffect } from 'react';
import { Plus, Search, Edit2, Trash2, MoreHorizontal, Calendar, MapPin } from 'lucide-react';
import './TicketManage.css';

interface Ticket {
  id: number;
  title: string;
  venue: string;
  event_date: string;
  total_seats: number;
  available_seats: number;
  price: number;
  category: string;
  status: number;
}

const TicketManage = () => {
  const [tickets, setTickets] = useState<Ticket[]>([]);
  const [loading, setLoading] = useState(true);
  const [searchQuery, setSearchQuery] = useState('');

  useEffect(() => {
    const loadTickets = async () => {
      setLoading(true);
      await new Promise(resolve => setTimeout(resolve, 500));
      setTickets([
        {
          id: 1,
          title: '2026 周杰伦演唱会',
          venue: '国家体育场（鸟巢）',
          event_date: '2026-08-15',
          total_seats: 80000,
          available_seats: 1250,
          price: 580,
          category: '演出',
          status: 1
        },
        {
          id: 2,
          title: '2026 中超联赛',
          venue: '工人体育场',
          event_date: '2026-06-20',
          total_seats: 68000,
          available_seats: 8500,
          price: 120,
          category: '赛事',
          status: 1
        },
        {
          id: 3,
          title: '故宫博物院',
          venue: '故宫博物院',
          event_date: '2026-06-01',
          total_seats: 8000,
          available_seats: 5000,
          price: 60,
          category: '景区',
          status: 1
        },
        {
          id: 4,
          title: '复仇者联盟5',
          venue: '万达影城（CBD店）',
          event_date: '2026-07-10',
          total_seats: 200,
          available_seats: 45,
          price: 65,
          category: '电影',
          status: 1
        },
        {
          id: 5,
          title: '张学友经典演唱会',
          venue: '上海梅赛德斯奔驰文化中心',
          event_date: '2026-09-01',
          total_seats: 18000,
          available_seats: 3200,
          price: 480,
          category: '演出',
          status: 1
        },
        {
          id: 6,
          title: '环球影城',
          venue: '北京环球度假区',
          event_date: '2026-06-15',
          total_seats: 50000,
          available_seats: 8000,
          price: 418,
          category: '景区',
          status: 1
        }
      ]);
      setLoading(false);
    };
    loadTickets();
  }, []);

  const filteredTickets = tickets.filter(ticket =>
    ticket.title.toLowerCase().includes(searchQuery.toLowerCase()) ||
    ticket.venue.toLowerCase().includes(searchQuery.toLowerCase())
  );

  const formatDate = (dateString: string) => {
    return new Date(dateString).toLocaleDateString('zh-CN');
  };

  const getStockPercent = (available: number, total: number) => {
    return ((available / total) * 100).toFixed(1);
  };

  if (loading) {
    return (
      <div className="ticket-manage">
        <div className="ticket-manage-loading">
          <div className="loading-spinner"></div>
          <p>加载中...</p>
        </div>
      </div>
    );
  }

  return (
    <div className="ticket-manage">
      <div className="ticket-manage-header">
        <h1 className="ticket-manage-title">票务管理</h1>
        <button className="ticket-add-btn">
          <Plus size={18} />
          <span>添加票务</span>
        </button>
      </div>

      <div className="ticket-manage-toolbar">
        <div className="ticket-manage-search">
          <Search size={18} className="search-icon" />
          <input
            type="text"
            placeholder="搜索票务名称或场馆..."
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
          />
        </div>
      </div>

      <div className="ticket-table-wrapper">
        <table className="ticket-table">
          <thead>
            <tr>
              <th>票务名称</th>
              <th>场馆</th>
              <th>日期</th>
              <th>类别</th>
              <th>库存</th>
              <th>价格</th>
              <th>状态</th>
              <th>操作</th>
            </tr>
          </thead>
          <tbody>
            {filteredTickets.map(ticket => (
              <tr key={ticket.id}>
                <td>
                  <div className="ticket-info">
                    <span className="ticket-name">{ticket.title}</span>
                  </div>
                </td>
                <td>
                  <div className="venue-info">
                    <MapPin size={14} />
                    <span>{ticket.venue}</span>
                  </div>
                </td>
                <td>
                  <div className="date-info">
                    <Calendar size={14} />
                    <span>{formatDate(ticket.event_date)}</span>
                  </div>
                </td>
                <td>
                  <span className="ticket-category-tag">{ticket.category}</span>
                </td>
                <td>
                  <div className="stock-info">
                    <div className="stock-bar-mini">
                      <div
                        className="stock-fill-mini"
                        style={{ width: `${getStockPercent(ticket.available_seats, ticket.total_seats)}%` }}
                      />
                    </div>
                    <span className="stock-text">
                      {ticket.available_seats.toLocaleString()} / {ticket.total_seats.toLocaleString()}
                    </span>
                  </div>
                </td>
                <td>
                  <span className="ticket-price">¥{ticket.price}</span>
                </td>
                <td>
                  <span className={`status-badge ${ticket.status === 1 ? 'active' : 'inactive'}`}>
                    {ticket.status === 1 ? '在售' : '下架'}
                  </span>
                </td>
                <td>
                  <div className="ticket-actions">
                    <button className="action-btn" title="编辑">
                      <Edit2 size={16} />
                    </button>
                    <button className="action-btn" title="删除">
                      <Trash2 size={16} />
                    </button>
                    <button className="action-btn" title="更多">
                      <MoreHorizontal size={16} />
                    </button>
                  </div>
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </div>
  );
};

export default TicketManage;
