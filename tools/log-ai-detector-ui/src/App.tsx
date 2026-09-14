import { useState, useEffect, useCallback, useRef } from 'react';
import {
  Activity, AlertTriangle, RefreshCw, FileText, Terminal,
  Settings, ChevronDown, X, Search, Play, CheckCircle,
  XCircle, Loader2, Clock, Cpu, Sun, Moon,
} from 'lucide-react';
import { api } from './api';
import type { DetectorStatus, ReportMeta, ReportDetail } from './api';
import { renderMarkdown } from './markdown';
import './App.css';

// ── Helpers ────────────────────────────────────────────────────────────────

function levelClass(level: string) {
  if (level === 'FATAL') return 'level-fatal';
  if (level === 'ERROR') return 'level-error';
  if (level === 'WARN')  return 'level-warn';
  return 'level-info';
}

function fmtStamp(raw: string) {
  if (!raw || raw.length < 15) return raw;
  const y = raw.slice(0, 4), mo = raw.slice(4, 6), d = raw.slice(6, 8);
  const h = raw.slice(9, 11), m = raw.slice(11, 13), s = raw.slice(13, 15);
  return `${y}/${mo}/${d} ${h}:${m}:${s}`;
}

function fmtBytes(n: number) {
  if (n < 1024) return `${n} B`;
  if (n < 1024 * 1024) return `${(n / 1024).toFixed(1)} KB`;
  return `${(n / 1024 / 1024).toFixed(2)} MB`;
}

// ── Toast ──────────────────────────────────────────────────────────────────

type ToastItem = { id: number; msg: string; type: 'success' | 'error' | 'info' };
let _toastId = 1;

function useToastState() {
  const [toasts, setToasts] = useState<ToastItem[]>([]);
  const push = useCallback((msg: string, type: ToastItem['type'] = 'info') => {
    const id = _toastId++;
    setToasts(prev => [...prev, { id, msg, type }]);
    setTimeout(() => setToasts(prev => prev.filter(t => t.id !== id)), 3500);
  }, []);
  const dismiss = (id: number) => setToasts(prev => prev.filter(t => t.id !== id));
  return { toasts, push, dismiss,
    success: (m: string) => push(m, 'success'),
    error:   (m: string) => push(m, 'error'),
    info:    (m: string) => push(m, 'info'),
  };
}

const ToastContainer = ({ toasts, dismiss }: { toasts: ToastItem[]; dismiss: (id: number) => void }) => (
  <div className="toast-container" aria-live="polite">
    {toasts.map(t => (
      <div key={t.id} className={`toast toast-${t.type}`} role="alert">
        <span className="toast-dot" />
        <span className="toast-msg">{t.msg}</span>
        <button className="toast-close" onClick={() => dismiss(t.id)} aria-label="关闭">
          <X size={12} />
        </button>
      </div>
    ))}
  </div>
);

// ── Model Switcher ─────────────────────────────────────────────────────────

const ModelSwitcher = ({ models, active, onSwitch }: {
  models: string[]; active: string; onSwitch: (m: string) => Promise<void>;
}) => {
  const [open, setOpen] = useState(false);
  const [busy, setBusy] = useState(false);
  const ref = useRef<HTMLDivElement>(null);

  useEffect(() => {
    const h = (e: MouseEvent) => { if (ref.current && !ref.current.contains(e.target as Node)) setOpen(false); };
    document.addEventListener('mousedown', h);
    return () => document.removeEventListener('mousedown', h);
  }, []);

  return (
    <div className="model-switcher" ref={ref}>
      <button
        className="model-btn"
        onClick={() => setOpen(v => !v)}
        disabled={busy}
        aria-haspopup="listbox"
        aria-expanded={open}
      >
        {busy ? <Loader2 size={14} className="spinning" /> : <Cpu size={14} />}
        <span>{active}</span>
        <ChevronDown size={13} className={`chevron ${open ? 'open' : ''}`} />
      </button>
      {open && (
        <ul className="model-dropdown" role="listbox">
          {models.map(m => (
            <li key={m} role="option" aria-selected={m === active}
              className={`model-option ${m === active ? 'active' : ''}`}
              onClick={async () => {
                if (m === active || busy) return;
                setBusy(true); setOpen(false);
                try { await onSwitch(m); } finally { setBusy(false); }
              }}
            >
              {m === active && <CheckCircle size={12} />}
              <span>{m}</span>
            </li>
          ))}
        </ul>
      )}
    </div>
  );
};

// ── Status Bar ─────────────────────────────────────────────────────────────

const StatusBar = ({ status, loading, onRefresh }: {
  status: DetectorStatus | null; loading: boolean; onRefresh: () => void;
}) => (
  <div className="status-bar">
    <div className="status-left">
      <span className={`status-dot ${status?.running ? 'dot-on' : 'dot-off'}`} />
      <span className="status-label">{status?.running ? '监控运行中' : '监控未启动'}</span>
      {status && (
        <>
          <span className="status-sep" />
          <Cpu size={12} />
          <span className="status-model">{status.model}</span>
          <span className="status-sep" />
          <Clock size={12} />
          <span className="status-ts">{status.timestamp.slice(0, 19).replace('T', ' ')} UTC</span>
        </>
      )}
    </div>
    <button className="icon-btn" onClick={onRefresh} disabled={loading} aria-label="刷新状态">
      <RefreshCw size={14} className={loading ? 'spinning' : ''} />
    </button>
  </div>
);

// ── Report List ────────────────────────────────────────────────────────────

const ReportList = ({ reports, loading, search, onSearch, onSelect, selected }: {
  reports: ReportMeta[]; loading: boolean; search: string;
  onSearch: (v: string) => void; onSelect: (r: ReportMeta) => void; selected: string | null;
}) => {
  const filtered = reports.filter(r =>
    r.name.toLowerCase().includes(search.toLowerCase()) ||
    r.summary.toLowerCase().includes(search.toLowerCase())
  );

  return (
    <div className="report-list">
      <div className="panel-header">
        <h2 className="panel-title"><FileText size={15} />分析报告</h2>
        <span className="count-badge">{reports.length}</span>
      </div>
      <div className="search-wrap">
        <Search size={13} className="search-icon" />
        <input className="search-input" type="text" placeholder="搜索报告..."
          value={search} onChange={e => onSearch(e.target.value)} aria-label="搜索报告" />
        {search && (
          <button className="search-clear" onClick={() => onSearch('')} aria-label="清除">
            <X size={11} />
          </button>
        )}
      </div>
      {loading ? (
        <div className="panel-center"><Loader2 size={18} className="spinning" /></div>
      ) : filtered.length === 0 ? (
        <div className="panel-center">
          <AlertTriangle size={18} />
          <span>{search ? '无匹配' : '暂无报告'}</span>
        </div>
      ) : (
        <ul className="report-items">
          {filtered.map(r => (
            <li key={r.name}
              className={`report-item ${selected === r.name ? 'selected' : ''}`}
              onClick={() => onSelect(r)}
              role="button" tabIndex={0}
              onKeyDown={e => e.key === 'Enter' && onSelect(r)}
            >
              <span className={`level-badge ${levelClass(r.level)}`}>{r.level}</span>
              <div className="report-meta">
                <span className="report-summary">{r.summary || r.name}</span>
                <span className="report-time">{fmtStamp(r.created_at)}</span>
              </div>
            </li>
          ))}
        </ul>
      )}
    </div>
  );
};

// ── Report Viewer ──────────────────────────────────────────────────────────

const ReportViewer = ({ name }: { name: string | null }) => {
  const [detail, setDetail] = useState<ReportDetail | null>(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState('');

  useEffect(() => {
    if (!name) { setDetail(null); return; }
    setLoading(true); setError('');
    api.reportDetail(name).then(setDetail).catch(e => setError(e.message)).finally(() => setLoading(false));
  }, [name]);

  if (!name) return <div className="viewer-empty"><FileText size={28} /><span>选择左侧报告</span></div>;
  if (loading) return <div className="viewer-empty"><Loader2 size={22} className="spinning" /></div>;
  if (error) return <div className="viewer-empty viewer-error"><XCircle size={18} /><span>{error}</span></div>;
  if (!detail) return null;

  return (
    <div className="viewer">
      <div className="viewer-header">
        <span className={`level-badge ${levelClass(detail.level)}`}>{detail.level}</span>
        <span className="viewer-name">{detail.name}</span>
        <span className="viewer-size">{fmtBytes(detail.size)}</span>
      </div>
      {detail.summary && <p className="viewer-summary">{detail.summary}</p>}
      <div className="viewer-content md-body">{renderMarkdown(detail.content)}</div>
    </div>
  );
};

// ── Daemon Logs ────────────────────────────────────────────────────────────

const DaemonLogs = () => {
  const [lines, setLines] = useState<string[]>([]);
  const [loading, setLoading] = useState(false);
  const endRef = useRef<HTMLDivElement>(null);

  const refresh = useCallback(async () => {
    setLoading(true);
    try { const { lines: l } = await api.daemonLogs(200); setLines(l); } catch { /**/ } finally { setLoading(false); }
  }, []);

  useEffect(() => { refresh(); }, [refresh]);
  useEffect(() => { endRef.current?.scrollIntoView({ behavior: 'smooth' }); }, [lines]);

  return (
    <div className="daemon-logs">
      <div className="panel-header">
        <h2 className="panel-title"><Terminal size={15} />运行日志</h2>
        <button className="icon-btn" onClick={refresh} disabled={loading} aria-label="刷新日志">
          <RefreshCw size={13} className={loading ? 'spinning' : ''} />
        </button>
      </div>
      <pre className="log-output" role="log" aria-live="polite">
        {lines.length === 0 ? '暂无运行日志' : lines.map((l, i) => <span key={i}>{l}{'\n'}</span>)}
        <div ref={endRef} />
      </pre>
    </div>
  );
};

// ── Analyze Panel ──────────────────────────────────────────────────────────

const AnalyzePanel = ({ toast }: { toast: ReturnType<typeof useToastState> }) => {
  const [file, setFile] = useState('');
  const [running, setRunning] = useState(false);
  const [result, setResult] = useState<{ count: number; reports: string[] } | null>(null);

  const run = async () => {
    if (!file.trim()) return;
    setRunning(true); setResult(null);
    try {
      const res = await api.analyze(file.trim());
      setResult(res);
      res.count === 0 ? toast.info('未发现 ERROR/FATAL 日志') : toast.success(`已生成 ${res.count} 份报告`);
    } catch (e: unknown) {
      toast.error((e as Error).message);
    } finally { setRunning(false); }
  };

  return (
    <div className="analyze-panel">
      <div className="panel-header"><h2 className="panel-title"><Play size={15} />单次分析</h2></div>
      <p className="analyze-hint">指定日志文件路径，立即扫描其中所有 ERROR / FATAL 并生成报告。</p>
      <div className="analyze-form">
        <input className="analyze-input" type="text"
          placeholder="日志文件路径，例如：logs/hyperticket.admin.20260715-…log"
          value={file} onChange={e => setFile(e.target.value)}
          onKeyDown={e => e.key === 'Enter' && run()} disabled={running} aria-label="日志文件路径" />
        <button className="analyze-btn" onClick={run} disabled={running || !file.trim()}>
          {running ? <Loader2 size={15} className="spinning" /> : <Play size={15} />}
          {running ? '分析中' : '开始分析'}
        </button>
      </div>
      {result && (
        <div className="analyze-result">
          {result.count === 0 ? (
            <div className="result-empty"><CheckCircle size={15} />未发现 ERROR / FATAL 日志</div>
          ) : (
            <div className="result-ok">
              <CheckCircle size={15} /><span>生成 {result.count} 份报告</span>
              <ul className="result-paths">
                {result.reports.map(p => <li key={p} className="result-path">{p}</li>)}
              </ul>
            </div>
          )}
        </div>
      )}
    </div>
  );
};

// ── Config Panel ───────────────────────────────────────────────────────────

const ConfigPanel = () => {
  const [cfg, setCfg] = useState<Record<string, string> | null>(null);
  const [loading, setLoading] = useState(false);

  useEffect(() => {
    setLoading(true);
    api.config().then(setCfg).catch(() => {}).finally(() => setLoading(false));
  }, []);

  return (
    <div className="config-panel">
      <div className="panel-header"><h2 className="panel-title"><Settings size={15} />运行配置</h2></div>
      {loading ? (
        <div className="panel-center"><Loader2 size={18} className="spinning" /></div>
      ) : !cfg ? (
        <p className="config-na">配置不可用，请确认 API 服务已启动。</p>
      ) : (
        <dl className="config-list">
          {Object.entries(cfg).map(([k, v]) => (
            <div key={k} className="config-row">
              <dt className="config-key">{k}</dt>
              <dd className="config-val">{v || <span className="config-empty">—</span>}</dd>
            </div>
          ))}
        </dl>
      )}
    </div>
  );
};

// ── App ────────────────────────────────────────────────────────────────────

type Tab = 'reports' | 'logs' | 'analyze' | 'config';
const TABS: { id: Tab; label: string; Icon: React.ComponentType<{ size?: number }> }[] = [
  { id: 'reports',  label: '报告',    Icon: FileText  },
  { id: 'logs',     label: '运行日志', Icon: Terminal  },
  { id: 'analyze',  label: '单次分析', Icon: Play      },
  { id: 'config',   label: '配置',    Icon: Settings  },
];

export default function App() {
  const toast = useToastState();

  const [theme, setTheme] = useState<'light' | 'dark'>(() =>
    (localStorage.getItem('ld-theme') as 'light' | 'dark') || 'light'
  );
  useEffect(() => {
    document.documentElement.setAttribute('data-theme', theme);
    localStorage.setItem('ld-theme', theme);
  }, [theme]);

  const [status, setStatus] = useState<DetectorStatus | null>(null);
  const [statusLoading, setStatusLoading] = useState(false);
  const [models, setModels] = useState<{ models: string[]; active: string }>({ models: [], active: '' });
  const [apiError, setApiError] = useState('');

  const [tab, setTab] = useState<Tab>('reports');
  const [reports, setReports] = useState<ReportMeta[]>([]);
  const [reportsLoading, setReportsLoading] = useState(false);
  const [reportsSearch, setReportsSearch] = useState('');
  const [selectedReport, setSelectedReport] = useState<string | null>(null);

  const refreshStatus = useCallback(async () => {
    setStatusLoading(true); setApiError('');
    try {
      const [s, m] = await Promise.all([api.status(), api.models()]);
      setStatus(s); setModels(m);
    } catch (e: unknown) { setApiError((e as Error).message); }
    finally { setStatusLoading(false); }
  }, []);

  const refreshReports = useCallback(async () => {
    setReportsLoading(true);
    try { const { reports: r } = await api.reports(); setReports(r); } catch { /**/ }
    finally { setReportsLoading(false); }
  }, []);

  useEffect(() => { refreshStatus(); refreshReports(); }, [refreshStatus, refreshReports]);

  const handleSwitchModel = async (model: string) => {
    try {
      await api.switchModel(model);
      toast.success(`模型已切换至 ${model}`);
      await refreshStatus();
    } catch (e: unknown) { toast.error((e as Error).message); }
  };

  return (
    <>
      <div className="app">
        {/* Top nav */}
        <header className="nav">
          <div className="nav-brand">
            <Activity size={18} className="nav-icon" />
            <span className="nav-title">HyperTicket 日志检测</span>
          </div>
          <div className="nav-right">
            {models.models.length > 0 && (
              <ModelSwitcher models={models.models} active={models.active} onSwitch={handleSwitchModel} />
            )}
            <button className="icon-btn" onClick={() => setTheme(t => t === 'light' ? 'dark' : 'light')}
              aria-label="切换主题">
              {theme === 'light' ? <Moon size={16} /> : <Sun size={16} />}
            </button>
          </div>
        </header>

        <main className="main">
          {/* Status bar */}
          <StatusBar status={status} loading={statusLoading}
            onRefresh={() => { refreshStatus(); refreshReports(); }} />

          {/* API error */}
          {apiError && (
            <div className="api-error" role="alert">
              <XCircle size={14} />
              <span>无法连接 API 服务：{apiError}</span>
              <button className="icon-btn" onClick={() => setApiError('')} aria-label="关闭">
                <X size={12} />
              </button>
            </div>
          )}

          {/* Tabs */}
          <div className="tabs" role="tablist">
            {TABS.map(({ id, label, Icon }) => (
              <button key={id} role="tab" aria-selected={tab === id}
                className={`tab ${tab === id ? 'active' : ''}`}
                onClick={() => setTab(id)}>
                <Icon size={14} />{label}
              </button>
            ))}
          </div>

          {/* Tab content */}
          <div className="tab-content">
            {tab === 'reports' && (
              <div className="reports-layout">
                <ReportList reports={reports} loading={reportsLoading}
                  search={reportsSearch} onSearch={setReportsSearch}
                  onSelect={r => setSelectedReport(r.name)} selected={selectedReport} />
                <div className="viewer-wrap"><ReportViewer name={selectedReport} /></div>
              </div>
            )}
            {tab === 'logs'    && <DaemonLogs />}
            {tab === 'analyze' && <AnalyzePanel toast={toast} />}
            {tab === 'config'  && <ConfigPanel />}
          </div>
        </main>
      </div>

      <ToastContainer toasts={toast.toasts} dismiss={toast.dismiss} />
    </>
  );
}
