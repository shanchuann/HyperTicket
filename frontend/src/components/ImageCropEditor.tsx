import { useState, useRef, useCallback, useEffect } from 'react';
import { X, ZoomIn, ZoomOut, Check } from 'lucide-react';
import './ImageCropEditor.css';

// Output canvas dimensions (2× for sharpness)
const OUT_W = 960;
const OUT_H = 384;
// Viewport display dimensions (fits inside 480px modal)
const VP_W = 448;
const VP_H = 179; // same 5:2.33 ratio as out

interface Props {
  src: string;
  onConfirm: (dataUrl: string) => void;
  onCancel: () => void;
}

interface Pos { x: number; y: number; }

const clampPos = (pos: Pos, imgW: number, imgH: number, scale: number): Pos => ({
  x: Math.min(0, Math.max(VP_W - imgW * scale, pos.x)),
  y: Math.min(0, Math.max(VP_H - imgH * scale, pos.y)),
});

const ImageCropEditor = ({ src, onConfirm, onCancel }: Props) => {
  const [naturalSize, setNaturalSize] = useState({ w: 0, h: 0 });
  const [scale, setScale] = useState(1);
  const [pos, setPos] = useState<Pos>({ x: 0, y: 0 });

  const dragging = useRef(false);
  const lastPt = useRef<Pos>({ x: 0, y: 0 });
  const imgRef = useRef<HTMLImageElement>(null);

  const initFromSize = (w: number, h: number) => {
    const s = Math.max(VP_W / w, VP_H / h);
    setScale(s);
    setPos({ x: (VP_W - w * s) / 2, y: (VP_H - h * s) / 2 });
    setNaturalSize({ w, h });
  };

  const onLoad = () => {
    const img = imgRef.current!;
    initFromSize(img.naturalWidth, img.naturalHeight);
  };

  const applyScale = useCallback((next: number, cx = VP_W / 2, cy = VP_H / 2) => {
    setScale(prev => {
      const clamped = Math.min(8, Math.max(0.2, next));
      // zoom toward the center point
      setPos(p => {
        const ratio = clamped / prev;
        return clampPos(
          { x: cx - (cx - p.x) * ratio, y: cy - (cy - p.y) * ratio },
          naturalSize.w, naturalSize.h, clamped
        );
      });
      return clamped;
    });
  }, [naturalSize]);

  // Mouse drag
  const onMouseDown = (e: React.MouseEvent) => {
    dragging.current = true;
    lastPt.current = { x: e.clientX, y: e.clientY };
    e.preventDefault();
  };

  const onMouseMove = useCallback((e: MouseEvent) => {
    if (!dragging.current) return;
    const dx = e.clientX - lastPt.current.x;
    const dy = e.clientY - lastPt.current.y;
    lastPt.current = { x: e.clientX, y: e.clientY };
    setPos(p => clampPos({ x: p.x + dx, y: p.y + dy }, naturalSize.w, naturalSize.h, scale));
  }, [naturalSize, scale]);

  const onMouseUp = useCallback(() => { dragging.current = false; }, []);

  // Touch drag
  const lastTouch = useRef<Pos>({ x: 0, y: 0 });
  const onTouchStart = (e: React.TouchEvent) => {
    const t = e.touches[0];
    lastTouch.current = { x: t.clientX, y: t.clientY };
  };
  const onTouchMove = (e: React.TouchEvent) => {
    const t = e.touches[0];
    const dx = t.clientX - lastTouch.current.x;
    const dy = t.clientY - lastTouch.current.y;
    lastTouch.current = { x: t.clientX, y: t.clientY };
    setPos(p => clampPos({ x: p.x + dx, y: p.y + dy }, naturalSize.w, naturalSize.h, scale));
    e.preventDefault();
  };

  // Wheel zoom
  const onWheel = (e: React.WheelEvent) => {
    e.preventDefault();
    const rect = (e.currentTarget as HTMLElement).getBoundingClientRect();
    const cx = e.clientX - rect.left;
    const cy = e.clientY - rect.top;
    applyScale(scale * (e.deltaY < 0 ? 1.1 : 0.9), cx, cy);
  };

  useEffect(() => {
    window.addEventListener('mousemove', onMouseMove);
    window.addEventListener('mouseup', onMouseUp);
    return () => {
      window.removeEventListener('mousemove', onMouseMove);
      window.removeEventListener('mouseup', onMouseUp);
    };
  }, [onMouseMove, onMouseUp]);

  const confirm = () => {
    const canvas = document.createElement('canvas');
    canvas.width = OUT_W;
    canvas.height = OUT_H;
    const ctx = canvas.getContext('2d')!;
    const img = new Image();
    img.onload = () => {
      // visible source rect in natural image coordinates
      const sx = -pos.x / scale;
      const sy = -pos.y / scale;
      const sw = VP_W / scale;
      const sh = VP_H / scale;
      ctx.drawImage(img, sx, sy, sw, sh, 0, 0, OUT_W, OUT_H);
      onConfirm(canvas.toDataURL('image/jpeg', 0.88));
    };
    img.src = src;
  };

  const minScale = naturalSize.w > 0
    ? Math.max(VP_W / naturalSize.w, VP_H / naturalSize.h)
    : 0.1;

  return (
    <div className="ice-overlay" onClick={e => e.target === e.currentTarget && onCancel()}>
      <div className="ice-card">
        <div className="ice-header">
          <span className="ice-title">调整封面位置</span>
          <button className="ice-close" onClick={onCancel}><X size={18} /></button>
        </div>

        <p className="ice-hint">拖动图片调整位置，滚轮或按钮缩放</p>

        {/* Viewport */}
        <div
          className="ice-viewport"
          style={{ width: VP_W, height: VP_H }}
          onMouseDown={onMouseDown}
          onWheel={onWheel}
          onTouchStart={onTouchStart}
          onTouchMove={onTouchMove}
        >
          <img
            ref={imgRef}
            src={src}
            alt=""
            className="ice-img"
            style={{
              width: naturalSize.w * scale,
              height: naturalSize.h * scale,
              transform: `translate(${pos.x}px, ${pos.y}px)`,
            }}
            onLoad={onLoad}
            draggable={false}
          />
          {/* Grid overlay */}
          <div className="ice-grid-overlay" />
        </div>

        {/* Zoom controls */}
        <div className="ice-zoom-row">
          <button className="ice-zoom-btn" onClick={() => applyScale(scale / 1.2)}
            disabled={scale <= minScale}><ZoomOut size={16} /></button>
          <input
            type="range"
            className="ice-slider"
            min={minScale * 100}
            max={800}
            step={1}
            value={Math.round(scale * 100)}
            onChange={e => applyScale(+e.target.value / 100)}
          />
          <button className="ice-zoom-btn" onClick={() => applyScale(scale * 1.2)}
            disabled={scale >= 8}><ZoomIn size={16} /></button>
          <span className="ice-zoom-label">{Math.round(scale / minScale * 100)}%</span>
        </div>

        {/* Actions */}
        <div className="ice-actions">
          <button className="ice-btn-cancel" onClick={onCancel}>取消</button>
          <button className="ice-btn-confirm" onClick={confirm}>
            <Check size={16} />确认使用
          </button>
        </div>
      </div>
    </div>
  );
};

export default ImageCropEditor;
