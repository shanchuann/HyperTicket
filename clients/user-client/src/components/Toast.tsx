import { useEffect } from 'react';
import { AlertTriangle, CheckCircle2, Info, X } from 'lucide-react';
import './Toast.css';

type ToastTone = 'success' | 'error' | 'info';

type ToastProps = {
  message: string;
  tone?: ToastTone;
  onClose: () => void;
  actionLabel?: string;
  onAction?: () => void;
  duration?: number;
};

const icons = { success: CheckCircle2, error: AlertTriangle, info: Info };

export default function Toast({ message, tone = 'info', onClose, actionLabel, onAction, duration = 4500 }: ToastProps) {
  useEffect(() => {
    if (!message || duration <= 0) return;
    const timer = window.setTimeout(onClose, duration);
    return () => window.clearTimeout(timer);
  }, [duration, message, onClose]);

  if (!message) return null;
  const Icon = icons[tone];

  return (
    <div className={`app-toast ${tone}`} role={tone === 'error' ? 'alert' : 'status'} aria-live="polite">
      <Icon size={20} aria-hidden="true" />
      <div className="app-toast-content">
        <strong>{tone === 'success' ? '操作成功' : tone === 'error' ? '需要处理' : '提示'}</strong>
        <span>{message}</span>
      </div>
      {actionLabel && onAction && <button className="app-toast-action" onClick={onAction}>{actionLabel}</button>}
      <button className="app-toast-close" onClick={onClose} aria-label="关闭通知"><X size={17} /></button>
      {duration > 0 && <span className="app-toast-timer" style={{ animationDuration: `${duration}ms` }} />}
    </div>
  );
}
