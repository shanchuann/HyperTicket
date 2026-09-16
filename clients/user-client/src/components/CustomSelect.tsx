import { useEffect, useId, useLayoutEffect, useRef, useState } from 'react';
import { createPortal } from 'react-dom';
import { Check, ChevronDown } from 'lucide-react';
import './CustomSelect.css';

export type SelectOption = { value: string; label: string };

type Props = {
  label: string;
  value: string;
  options: SelectOption[];
  onChange: (value: string) => void;
};

export default function CustomSelect({ label, value, options, onChange }: Props) {
  const [open, setOpen] = useState(false);
  const [active, setActive] = useState(0);
  const [position, setPosition] = useState({ top: 0, left: 0, width: 0 });
  const triggerRef = useRef<HTMLButtonElement>(null);
  const menuRef = useRef<HTMLDivElement>(null);
  const listId = useId();
  const selectedIndex = Math.max(0, options.findIndex(option => option.value === value));
  const selected = options[selectedIndex];

  const placeMenu = () => {
    const rect = triggerRef.current?.getBoundingClientRect();
    if (!rect) return;
    setPosition({ top: rect.bottom + 6, left: rect.left, width: rect.width });
  };

  useLayoutEffect(() => { if (open) placeMenu(); }, [open]);

  useEffect(() => {
    if (!open) return;
    setActive(selectedIndex);
    const closeOutside = (event: PointerEvent) => {
      const target = event.target as Node;
      if (!triggerRef.current?.contains(target) && !menuRef.current?.contains(target)) setOpen(false);
    };
    const reposition = () => placeMenu();
    document.addEventListener('pointerdown', closeOutside);
    window.addEventListener('resize', reposition);
    window.addEventListener('scroll', reposition, true);
    return () => {
      document.removeEventListener('pointerdown', closeOutside);
      window.removeEventListener('resize', reposition);
      window.removeEventListener('scroll', reposition, true);
    };
  }, [open, selectedIndex]);

  const choose = (index: number) => {
    onChange(options[index].value);
    setOpen(false);
    triggerRef.current?.focus();
  };

  const onKeyDown = (event: React.KeyboardEvent) => {
    if (event.key === 'Escape') { setOpen(false); return; }
    if (event.key === 'ArrowDown' || event.key === 'ArrowUp') {
      event.preventDefault();
      if (!open) { setOpen(true); return; }
      const step = event.key === 'ArrowDown' ? 1 : -1;
      setActive(index => (index + step + options.length) % options.length);
    }
    if ((event.key === 'Enter' || event.key === ' ') && open) {
      event.preventDefault();
      choose(active);
    }
  };

  return <div className="custom-select-field">
    <span>{label}</span>
    <button
      ref={triggerRef}
      className={`custom-select-trigger ${open ? 'open' : ''}`}
      type="button"
      role="combobox"
      aria-controls={listId}
      aria-expanded={open}
      aria-haspopup="listbox"
      onClick={() => setOpen(current => !current)}
      onKeyDown={onKeyDown}
    >
      <b>{selected?.label}</b><ChevronDown size={17}/>
    </button>
    {open && createPortal(
      <div
        ref={menuRef}
        id={listId}
        className="custom-select-menu"
        role="listbox"
        style={{ position: 'fixed', ...position }}
      >
        {options.map((option, index) => <button
          type="button"
          role="option"
          aria-selected={option.value === value}
          className={index === active ? 'active' : ''}
          key={option.value || 'all'}
          onPointerMove={() => setActive(index)}
          onClick={() => choose(index)}
        >
          <span>{option.label}</span>{option.value === value && <Check size={16}/>} 
        </button>)}
      </div>,
      document.body
    )}
  </div>;
}
