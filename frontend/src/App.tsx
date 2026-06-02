import { BrowserRouter, Routes, Route, Navigate } from 'react-router-dom';
import Landing from './pages/Landing';
import { AuthLayout, Login, Register } from './pages/Auth';
import { CustomerLayout, TicketList, OrderList } from './pages/Customer';
import { AdminLayout, Dashboard, TicketManage, UserManage } from './pages/Admin';
import AdminLogin from './pages/Admin/AdminLogin';
import ProtectedRoute from './components/ProtectedRoute';
import AdminProtectedRoute from './components/AdminProtectedRoute';
import './styles/tokens.css';

function App() {
  return (
    <BrowserRouter>
      <Routes>
        {/* 营销落地页 */}
        <Route path="/" element={<Landing />} />

        {/* 用户认证 */}
        <Route path="/auth" element={<AuthLayout />}>
          <Route index element={<Navigate to="login" replace />} />
          <Route path="login" element={<Login />} />
          <Route path="register" element={<Register />} />
        </Route>

        {/* 管理员登录（独立页面，不共享 AuthLayout） */}
        <Route path="/admin/login" element={<AdminLogin />} />

        {/* 客户界面（需用户登录） */}
        <Route
          path="/customer"
          element={<ProtectedRoute><CustomerLayout /></ProtectedRoute>}
        >
          <Route index element={<TicketList />} />
          <Route path="orders" element={<OrderList />} />
        </Route>

        {/* 管理后台（需管理员登录） */}
        <Route
          path="/admin"
          element={<AdminProtectedRoute><AdminLayout /></AdminProtectedRoute>}
        >
          <Route index element={<Dashboard />} />
          <Route path="tickets" element={<TicketManage />} />
          <Route path="users" element={<UserManage />} />
        </Route>
      </Routes>
    </BrowserRouter>
  );
}

export default App;
