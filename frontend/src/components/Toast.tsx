import { useState, useCallback, useMemo, useRef } from 'react';
import { ToastContext } from './ToastContext';
import type { ToastContextValue, ToastType } from './ToastContext';
import './Toast.css';

interface ToastItem {
  id: number;
  message: string;
  type: ToastType;
  exiting: boolean;
}


let nextId = 1;
const DURATION = 3500;
const EXIT_DURATION = 300;

export const ToastProvider = ({ children }: { children: React.ReactNode }) => {
  const [toasts, setToasts] = useState<ToastItem[]>([]);
  const timers = useRef<Map<number, ReturnType<typeof setTimeout>>>(new Map());

  const remove = useCallback((id: number) => {
    setToasts(prev => prev.filter(t => t.id !== id));
    timers.current.delete(id);
  }, []);

  const startExit = useCallback((id: number) => {
    setToasts(prev => prev.map(t => t.id === id ? { ...t, exiting: true } : t));
    const t = setTimeout(() => remove(id), EXIT_DURATION);
    timers.current.set(id, t);
  }, [remove]);

  const showToast = useCallback((message: string, type: ToastType = 'info') => {
    const id = nextId++;
    setToasts(prev => [...prev, { id, message, type, exiting: false }]);
    const t = setTimeout(() => startExit(id), DURATION);
    timers.current.set(id, t);
  }, [startExit]);

  const dismiss = useCallback((id: number) => {
    const t = timers.current.get(id);
    if (t) clearTimeout(t);
    startExit(id);
  }, [startExit]);

  const value = useMemo<ToastContextValue>(() => ({
    showToast,
    success: (msg) => showToast(msg, 'success'),
    error: (msg) => showToast(msg, 'error'),
    info: (msg) => showToast(msg, 'info'),
  }), [showToast]);

  return (
    <ToastContext.Provider value={value}>
      {children}
      <div className="toast-container" aria-live="polite">
        {toasts.map(t => (
          <div
            key={t.id}
            className={`toast toast-${t.type} ${t.exiting ? 'toast-exit' : 'toast-enter'}`}
            onClick={() => dismiss(t.id)}
            role="alert"
          >
            <span className="toast-dot" />
            <span className="toast-msg">{t.message}</span>
            <button className="toast-close" onClick={e => { e.stopPropagation(); dismiss(t.id); }}>×</button>
          </div>
        ))}
      </div>
    </ToastContext.Provider>
  );
};
