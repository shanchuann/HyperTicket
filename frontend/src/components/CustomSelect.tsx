import { useState, useRef, useEffect } from 'react';
import { ChevronDown, Check } from 'lucide-react';
import './CustomSelect.css';

export interface SelectOption {
  value: string;
  label: string;
}

interface Props {
  options: SelectOption[];
  value: string;
  onChange: (value: string) => void;
  disabled?: boolean;
  placeholder?: string;
}

const CustomSelect = ({ options, value, onChange, disabled, placeholder = '请选择' }: Props) => {
  const [open, setOpen] = useState(false);
  const ref = useRef<HTMLDivElement>(null);
  const selected = options.find(o => o.value === value);

  useEffect(() => {
    if (!open) return;
    const h = (e: MouseEvent) => { if (!ref.current?.contains(e.target as Node)) setOpen(false); };
    document.addEventListener('mousedown', h);
    return () => document.removeEventListener('mousedown', h);
  }, [open]);

  const pick = (v: string) => { onChange(v); setOpen(false); };

  return (
    <div ref={ref} className={`csel-root ${disabled ? 'csel-disabled' : ''}`}>
      <div className="csel-trigger form-input" onClick={() => !disabled && setOpen(o => !o)}>
        <span className={selected ? 'csel-value' : 'csel-ph'}>
          {selected?.label ?? placeholder}
        </span>
        <ChevronDown size={15} className={`csel-arrow ${open ? 'open' : ''}`} />
      </div>
      {open && (
        <div className="csel-popup">
          {options.map(opt => (
            <div
              key={opt.value}
              className={`csel-item ${opt.value === value ? 'selected' : ''}`}
              onClick={() => pick(opt.value)}
            >
              <span>{opt.label}</span>
              {opt.value === value && <Check size={14} className="csel-check" />}
            </div>
          ))}
        </div>
      )}
    </div>
  );
};

export default CustomSelect;
