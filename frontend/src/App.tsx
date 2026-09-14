import { BrowserRouter, Routes, Route, Navigate } from 'react-router-dom';
import Landing from './pages/Landing';
import { AuthLayout, Login, Register } from './pages/Auth';
import { CustomerLayout, TicketList, OrderList, FavoriteList } from './pages/Customer';
import { AdminLayout, Dashboard, TicketManage, UserManage } from './pages/Admin';
import AdminLogin from './pages/Admin/AdminLogin';
import VerifyOrder from './pages/VerifyOrder';
import ProtectedRoute from './components/ProtectedRoute';
import AdminProtectedRoute from './components/AdminProtectedRoute';
import { ToastProvider } from './components/Toast';
import './styles/tokens.css';

function App() {
  return (
    <ToastProvider>
      <BrowserRouter>
        <Routes>
          <Route path="/" element={<Landing />} />
          <Route path="/auth" element={<AuthLayout />}>
            <Route index element={<Navigate to="login" replace />} />
            <Route path="login" element={<Login />} />
            <Route path="register" element={<Register />} />
          </Route>
          <Route path="/admin/login" element={<AdminLogin />} />
          <Route path="/verify" element={<VerifyOrder />} />
          <Route path="/customer" element={<ProtectedRoute><CustomerLayout /></ProtectedRoute>}>
            <Route index element={<TicketList />} />
            <Route path="favorites" element={<FavoriteList />} />
            <Route path="orders" element={<OrderList />} />
          </Route>
          <Route path="/admin" element={<AdminProtectedRoute><AdminLayout /></AdminProtectedRoute>}>
            <Route index element={<Dashboard />} />
            <Route path="tickets" element={<TicketManage />} />
            <Route path="users" element={<UserManage />} />
          </Route>
        </Routes>
      </BrowserRouter>
    </ToastProvider>
  );
}

export default App;
