import { BrowserRouter, Routes, Route, Navigate } from 'react-router-dom';
import { AuthLayout, ForgotPassword, Login, Register } from './pages/Auth';
import { AccountSecurity, CustomerLayout, TicketList, OrderList } from './pages/Customer';
import ProtectedRoute from './components/ProtectedRoute';
import './styles/tokens.css';

function App() {
  return (
    <BrowserRouter>
      <Routes>
        <Route path="/" element={<Navigate to="/auth/login" replace />} />

        <Route path="/auth" element={<AuthLayout />}>
          <Route index element={<Navigate to="login" replace />} />
          <Route path="login" element={<Login />} />
          <Route path="register" element={<Register />} />
          <Route path="forgot-password" element={<ForgotPassword />} />
        </Route>

        <Route
          path="/customer"
          element={<ProtectedRoute><CustomerLayout /></ProtectedRoute>}
        >
          <Route index element={<TicketList />} />
          <Route path="orders" element={<OrderList />} />
          <Route path="security" element={<AccountSecurity />} />
        </Route>

        <Route path="*" element={<Navigate to="/auth/login" replace />} />
      </Routes>
    </BrowserRouter>
  );
}

export default App;
