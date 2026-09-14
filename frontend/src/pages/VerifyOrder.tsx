import { useState, useEffect } from 'react';
import { useSearchParams } from 'react-router-dom';
import { CheckCircle, XCircle, Clock, AlertCircle, Loader2, ScanLine } from 'lucide-react';
import wsClient from '../api/client';
import type { BackendResponse } from '../types';
import './VerifyOrder.css';

interface VerifyResult {
  order_no: string;
  order_status: string; // 订单状态（status 为协议字段 OK/ERR）
  title: string;
  venue: string;
  event_date: string;
  seat_label?: string;
  seat_tier?: string;
}

const STATUS_INFO: Record<string, { label: string; color: string; bg: string; Icon: typeof Clock }> = {
  CONFIRMED: { label: '有效票券',  color: '#1a7c4f', bg: '#d4f5e9', Icon: CheckCircle },
  CANCELLED: { label: '已取消',   color: '#c0392b', bg: '#fde8e8', Icon: XCircle },
  EXPIRED:   { label: '已过期',   color: '#666',    bg: '#f0f0f0', Icon: AlertCircle },
  PENDING:   { label: '待确认',   color: '#b7791f', bg: '#fef3cd', Icon: Clock },
};

const TIER_LABEL: Record<string, string> = { VIP: 'VIP', Standard: '标准', Economy: '经济' };

const VerifyOrder = () => {
  const [params] = useSearchParams();
  const orderNo = params.get('no') || '';
  const [loading, setLoading] = useState(Boolean(orderNo));
  const [result, setResult] = useState<VerifyResult | null>(null);
  const [error, setError] = useState('');

  useEffect(() => {
    if (!orderNo) return;
    // 等待 WS 连接后发送验证请求
    const tryVerify = async () => {
      try {
        await wsClient.connect();
        const resp = await wsClient.send<BackendResponse & VerifyResult>({
          type: 18,
          order_no: orderNo,
        });
        setResult(resp as unknown as VerifyResult);
      } catch (e) {
        setError(e instanceof Error ? e.message : '验证失败，请检查网络');
      } finally {
        setLoading(false);
      }
    };
    tryVerify();
  }, [orderNo]);

  const formatDate = (d: string) => {
    if (!d) return '—';
    return new Date(d).toLocaleDateString('zh-CN', {
      year: 'numeric', month: 'long', day: 'numeric', weekday: 'long',
    });
  };

  const si = result ? (STATUS_INFO[result.order_status] ?? STATUS_INFO.PENDING) : null;
  const displayError = orderNo ? error : '缺少订单编号';

  return (
    <div className="verify-page">
      <div className="verify-card">
        {/* Brand */}
        <div className="verify-brand">
          <ScanLine size={28} className="verify-brand-icon" />
          <span>HyperTicket 票务验证</span>
        </div>

        {loading && (
          <div className="verify-loading">
            <Loader2 size={32} className="spinner" />
            <p>正在验证票务信息…</p>
          </div>
        )}

        {!loading && displayError && (
          <div className="verify-error">
            <XCircle size={40} />
            <h2>验证失败</h2>
            <p>{displayError}</p>
            <code className="verify-no">{orderNo}</code>
          </div>
        )}

        {!loading && result && si && (
          <div className="verify-result" style={{ '--st-color': si.color, '--st-bg': si.bg } as React.CSSProperties}>
            <div className="verify-status-banner">
              <si.Icon size={36} className="verify-status-icon" />
              <span className="verify-status-label">{si.label}</span>
            </div>

            <div className="verify-info">
              <h2 className="verify-title">{result.title}</h2>
              <div className="verify-row">
                <span className="verify-row-label">演出日期</span>
                <span>{formatDate(result.event_date)}</span>
              </div>
              <div className="verify-row">
                <span className="verify-row-label">演出场馆</span>
                <span>{result.venue || '—'}</span>
              </div>
              {result.seat_label && (
                <div className="verify-row">
                  <span className="verify-row-label">座位</span>
                  <span>
                    <strong>{result.seat_label}</strong>
                    {result.seat_tier && ` · ${TIER_LABEL[result.seat_tier] ?? result.seat_tier}`}
                  </span>
                </div>
              )}
              <div className="verify-row">
                <span className="verify-row-label">订单编号</span>
                <code>{result.order_no}</code>
              </div>
            </div>
          </div>
        )}

        <p className="verify-footer">此页面仅供入场核验使用</p>
      </div>
    </div>
  );
};

export default VerifyOrder;
