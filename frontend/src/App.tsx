import { BrowserRouter, Routes, Route, Navigate } from 'react-router-dom';
import Landing from './pages/Landing';
import { AuthLayout, Login, Register } from './pages/Auth';
import { CustomerLayout, TicketList, OrderList } from './pages/Customer';
import { AdminLayout, Dashboard, TicketManage } from './pages/Admin';
import './styles/tokens.css';

function App() {
  return (
    <BrowserRouter>
      <Routes>
        {/* 营销落地页 */}
        <Route path="/" element={<Landing />} />

        {/* 认证页面 */}
        <Route path="/auth" element={<AuthLayout />}>
          <Route index element={<Navigate to="login" replace />} />
          <Route path="login" element={<Login />} />
          <Route path="register" element={<Register />} />
        </Route>

        {/* 客户界面 */}
        <Route path="/customer" element={<CustomerLayout />}>
          <Route index element={<TicketList />} />
          <Route path="orders" element={<OrderList />} />
        </Route>

        {/* 管理后台 */}
        <Route path="/admin" element={<AdminLayout />}>
          <Route index element={<Dashboard />} />
          <Route path="tickets" element={<TicketManage />} />
        </Route>
      </Routes>
    </BrowserRouter>
  );
}

export default App;
