import { Navigate, useLocation } from 'react-router-dom';

const AdminProtectedRoute = ({ children }: { children: React.ReactNode }) => {
  const location = useLocation();
  const adminToken = localStorage.getItem('admin_token');
  if (!adminToken) {
    return <Navigate to="/admin/login" state={{ from: location.pathname }} replace />;
  }
  return <>{children}</>;
};

export default AdminProtectedRoute;
