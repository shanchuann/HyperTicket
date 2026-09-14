import { useState, useRef, useEffect } from 'react';
import { Calendar, ChevronLeft, ChevronRight } from 'lucide-react';
import './DatePicker.css';

interface DatePickerProps {
  value: string; // YYYY-MM-DD
  onChange: (v: string) => void;
  disabled?: boolean;
}

const WEEKDAYS = ['一', '二', '三', '四', '五', '六', '日'];

const DatePicker = ({ value, onChange, disabled }: DatePickerProps) => {
  const today = new Date();
  const todayStr = [
    today.getFullYear(),
    String(today.getMonth() + 1).padStart(2, '0'),
    String(today.getDate()).padStart(2, '0'),
  ].join('-');

  const initView = () => value
    ? { y: +value.slice(0, 4), m: +value.slice(5, 7) - 1 }
    : { y: today.getFullYear(), m: today.getMonth() };

  const [open, setOpen] = useState(false);
  const [view, setView] = useState(initView);
  const ref = useRef<HTMLDivElement>(null);

  useEffect(() => {
    if (!open) return;
    const handler = (e: MouseEvent) => {
      if (!ref.current?.contains(e.target as Node)) setOpen(false);
    };
    document.addEventListener('mousedown', handler);
    return () => document.removeEventListener('mousedown', handler);
  }, [open]);

  const daysInMonth = new Date(view.y, view.m + 1, 0).getDate();
  const firstWeekday = (() => {
    const d = new Date(view.y, view.m, 1).getDay();
    return d === 0 ? 6 : d - 1; // Mon = 0
  })();

  const cells: (number | null)[] = [
    ...Array(firstWeekday).fill(null),
    ...Array.from({ length: daysInMonth }, (_, i) => i + 1),
  ];
  while (cells.length % 7) cells.push(null);

  const select = (day: number) => {
    onChange(`${view.y}-${String(view.m + 1).padStart(2, '0')}-${String(day).padStart(2, '0')}`);
    setOpen(false);
  };

  const navigate = (delta: number) => {
    setView(v => {
      const d = new Date(v.y, v.m + delta, 1);
      return { y: d.getFullYear(), m: d.getMonth() };
    });
  };

  const displayValue = value ? value.replace(/-/g, '/') : '';

  const toggleOpen = () => {
    if (disabled) return;
    if (!open) setView(initView());
    setOpen(current => !current);
  };

  return (
    <div ref={ref} className="dp-root">
      <div
        className={`dp-trigger form-input ${disabled ? 'dp-disabled' : ''}`}
        onClick={toggleOpen}
      >
        <span className={displayValue ? 'dp-value' : 'dp-placeholder'}>
          {displayValue || '选择日期'}
        </span>
        <Calendar size={15} className="dp-icon" />
      </div>

      {open && (
        <div className="dp-popup">
          {/* Month navigation */}
          <div className="dp-header">
            <button className="dp-nav-btn" onClick={() => navigate(-1)}>
              <ChevronLeft size={16} />
            </button>
            <span className="dp-month-label">{view.y}年{view.m + 1}月</span>
            <button className="dp-nav-btn" onClick={() => navigate(1)}>
              <ChevronRight size={16} />
            </button>
          </div>

          {/* Calendar grid */}
          <div className="dp-grid">
            {WEEKDAYS.map((w, i) => (
              <span key={w} className={`dp-weekday${i >= 5 ? ' weekend' : ''}`}>{w}</span>
            ))}
            {cells.map((day, i) => {
              if (!day) return <span key={`e${i}`} className="dp-empty" />;
              const ds = `${view.y}-${String(view.m + 1).padStart(2, '0')}-${String(day).padStart(2, '0')}`;
              const isSelected = ds === value;
              const isToday = ds === todayStr;
              const isWeekend = i % 7 >= 5;
              return (
                <button
                  key={day}
                  className={[
                    'dp-day',
                    isSelected ? 'selected' : '',
                    isToday ? 'today' : '',
                    isWeekend ? 'weekend' : '',
                  ].filter(Boolean).join(' ')}
                  onClick={() => select(day)}
                >
                  {day}
                </button>
              );
            })}
          </div>

          {/* Footer */}
          <div className="dp-footer">
            <button className="dp-footer-btn" onClick={() => { onChange(''); setOpen(false); }}>清除</button>
            <button className="dp-footer-btn primary" onClick={() => {
              setView({ y: today.getFullYear(), m: today.getMonth() });
              select(today.getDate());
            }}>今天</button>
          </div>
        </div>
      )}
    </div>
  );
};

export default DatePicker;
