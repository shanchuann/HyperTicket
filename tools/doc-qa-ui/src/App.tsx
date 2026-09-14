import { useState, useEffect, useRef, useCallback } from 'react';
import {
  BookOpen, Send, Loader2, Trash2, FileText, Sun, Moon,
  ChevronRight, ChevronDown, User, Sparkles, XCircle, X, Cpu, CheckCircle,
} from 'lucide-react';
import { api } from './api';
import type { QaStatus } from './api';
import { renderMarkdown } from './markdown';
import './App.css';

// ── Types ──────────────────────────────────────────────────────────────────

interface Message {
  id: number;
  role: 'user' | 'assistant' | 'error';
  content: string;
  files?: string[];
}

let msgId = 1;
const SESSION = `web-${Date.now().toString(36)}`;

// ── Model Switcher ──────────────────────────────────────────────────────────

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
        {busy ? <Loader2 size={13} className="spinning" /> : <Cpu size={13} />}
        <span>{active}</span>
        <ChevronDown size={12} className={`model-chevron ${open ? 'open' : ''}`} />
      </button>
      {open && (
        <ul className="model-dropdown" role="listbox" aria-label="选择模型">
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

// ── Suggested questions (empty state teaches the interface) ────────────────

const SUGGESTIONS = [
  'HyperTicket 的整体架构是怎样的？',
  'Server 端一个请求的完整处理链路？',
  '订票时如何防止超卖？',
  'ChronoLite 异步日志的双缓冲是怎么实现的？',
];

// ── File tree (grouped by top-level dir) ───────────────────────────────────

function groupFiles(files: string[]): Map<string, string[]> {
  const groups = new Map<string, string[]>();
  for (const f of files) {
    const dir = f.includes('/') ? f.split('/')[0] : '根目录';
    if (!groups.has(dir)) groups.set(dir, []);
    groups.get(dir)!.push(f);
  }
  return groups;
}

const FileIndex = ({ status, onOpen }: { status: QaStatus | null; onOpen: (path: string) => void }) => {
  const [openGroups, setOpenGroups] = useState<Set<string>>(new Set(['根目录']));

  if (!status) return null;
  const groups = groupFiles(status.files);

  const toggle = (dir: string) => {
    setOpenGroups(prev => {
      const next = new Set(prev);
      if (next.has(dir)) next.delete(dir); else next.add(dir);
      return next;
    });
  };

  return (
    <nav className="file-index" aria-label="文档索引">
      <div className="file-index-header">
        <BookOpen size={15} />
        <span>文档索引</span>
        <span className="file-count">{status.file_count}</span>
      </div>
      <div className="file-groups">
        {[...groups.entries()].map(([dir, items]) => (
          <div key={dir} className="file-group">
            <button
              className="file-group-toggle"
              onClick={() => toggle(dir)}
              aria-expanded={openGroups.has(dir)}
            >
              <ChevronRight size={13} className={`file-chevron ${openGroups.has(dir) ? 'open' : ''}`} />
              <span>{dir}</span>
              <span className="file-group-count">{items.length}</span>
            </button>
            {openGroups.has(dir) && (
              <ul className="file-list">
                {items.map(f => (
                  <li key={f}>
                    <button className="file-item" title={`查看 ${f}`} onClick={() => onOpen(f)}>
                      <FileText size={12} />
                      <span>{f.split('/').pop()}</span>
                    </button>
                  </li>
                ))}
              </ul>
            )}
          </div>
        ))}
      </div>
    </nav>
  );
};

// ── Document Viewer (side panel) ───────────────────────────────────────────

const DocViewer = ({ path, onClose }: { path: string; onClose: () => void }) => {
  const [content, setContent] = useState('');
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState('');

  useEffect(() => {
    setLoading(true);
    setError('');
    api.file(path)
      .then(d => setContent(d.content))
      .catch(e => setError((e as Error).message))
      .finally(() => setLoading(false));
  }, [path]);

  useEffect(() => {
    const onKey = (e: KeyboardEvent) => { if (e.key === 'Escape') onClose(); };
    document.addEventListener('keydown', onKey);
    return () => document.removeEventListener('keydown', onKey);
  }, [onClose]);

  return (
    <div className="doc-viewer" role="dialog" aria-label={`文档：${path}`}>
      <div className="doc-viewer-header">
        <FileText size={14} />
        <span className="doc-viewer-path">{path}</span>
        <button className="icon-btn" onClick={onClose} aria-label="关闭文档">
          <X size={15} />
        </button>
      </div>
      <div className="doc-viewer-body">
        {loading ? (
          <div className="doc-viewer-center"><Loader2 size={20} className="spinning" /></div>
        ) : error ? (
          <div className="doc-viewer-center doc-viewer-error"><XCircle size={16} /><span>{error}</span></div>
        ) : (
          <div className="msg-markdown">{renderMarkdown(content)}</div>
        )}
      </div>
    </div>
  );
};

// ── Message bubble ─────────────────────────────────────────────────────────

const MessageView = ({ msg, onOpenDoc }: { msg: Message; onOpenDoc: (path: string) => void }) => {
  if (msg.role === 'user') {
    return (
      <div className="msg msg-user">
        <div className="msg-avatar msg-avatar-user"><User size={14} /></div>
        <div className="msg-body msg-body-user">{msg.content}</div>
      </div>
    );
  }
  if (msg.role === 'error') {
    return (
      <div className="msg msg-assistant">
        <div className="msg-avatar msg-avatar-error"><XCircle size={14} /></div>
        <div className="msg-body msg-body-error">{msg.content}</div>
      </div>
    );
  }
  return (
    <div className="msg msg-assistant">
      <div className="msg-avatar msg-avatar-ai"><Sparkles size={14} /></div>
      <div className="msg-body">
        {msg.files && msg.files.length > 0 && (
          <div className="msg-sources">
            {msg.files.map(f => (
              <button key={f} className="msg-source" title={`查看 ${f}`} onClick={() => onOpenDoc(f)}>
                <FileText size={11} />
                {f.split('/').pop()}
              </button>
            ))}
          </div>
        )}
        <div className="msg-markdown">{renderMarkdown(msg.content)}</div>
      </div>
    </div>
  );
};

// ── App ────────────────────────────────────────────────────────────────────

export default function App() {
  const [theme, setTheme] = useState<'light' | 'dark'>(() =>
    (localStorage.getItem('qa-theme') as 'light' | 'dark') || 'light'
  );
  useEffect(() => {
    document.documentElement.setAttribute('data-theme', theme);
    localStorage.setItem('qa-theme', theme);
  }, [theme]);

  const [status, setStatus] = useState<QaStatus | null>(null);
  const [statusError, setStatusError] = useState('');
  const [messages, setMessages] = useState<Message[]>([]);
  const [input, setInput] = useState('');
  const [busy, setBusy] = useState(false);
  const [openDoc, setOpenDoc] = useState<string | null>(null);
  const threadRef = useRef<HTMLDivElement>(null);
  const inputRef = useRef<HTMLTextAreaElement>(null);

  useEffect(() => {
    api.status().then(setStatus).catch(e => setStatusError((e as Error).message));
  }, []);

  useEffect(() => {
    threadRef.current?.scrollTo({ top: threadRef.current.scrollHeight, behavior: 'smooth' });
  }, [messages, busy]);

  const send = useCallback(async (text: string) => {
    const question = text.trim();
    if (!question || busy) return;
    setInput('');
    setMessages(prev => [...prev, { id: msgId++, role: 'user', content: question }]);
    setBusy(true);
    try {
      const res = await api.ask(question, SESSION);
      setMessages(prev => [...prev, { id: msgId++, role: 'assistant', content: res.answer, files: res.files }]);
    } catch (e: unknown) {
      setMessages(prev => [...prev, { id: msgId++, role: 'error', content: (e as Error).message }]);
    } finally {
      setBusy(false);
      inputRef.current?.focus();
    }
  }, [busy]);

  const clearChat = async () => {
    if (busy) return;
    setMessages([]);
    try { await api.clear(SESSION); } catch { /* session is in-memory; ignore */ }
  };

  const onKeyDown = (e: React.KeyboardEvent<HTMLTextAreaElement>) => {
    if (e.key === 'Enter' && !e.shiftKey) {
      e.preventDefault();
      send(input);
    }
  };

  const handleSwitchModel = async (model: string) => {
    try {
      await api.switchModel(model);
      setStatus(prev => prev ? { ...prev, model } : prev);
    } catch {
      /* 失败保持原模型 */
    }
  };

  return (
    <div className="app">
      {/* Top bar */}
      <header className="topbar">
        <div className="topbar-brand">
          <BookOpen size={18} className="topbar-icon" />
          <span className="topbar-title">HyperTicket 文档问答</span>
        </div>
        <div className="topbar-right">
          {status && status.models.length > 0 && (
            <ModelSwitcher models={status.models} active={status.model} onSwitch={handleSwitchModel} />
          )}
          <button
            className="icon-btn"
            onClick={clearChat}
            disabled={busy || messages.length === 0}
            title="清除对话"
            aria-label="清除对话"
          >
            <Trash2 size={15} />
          </button>
          <button
            className="icon-btn"
            onClick={() => setTheme(t => t === 'light' ? 'dark' : 'light')}
            aria-label="切换主题"
          >
            {theme === 'light' ? <Moon size={15} /> : <Sun size={15} />}
          </button>
        </div>
      </header>

      <div className="layout">
        {/* Sidebar */}
        <aside className="sidebar">
          {statusError ? (
            <div className="sidebar-error">
              <XCircle size={16} />
              <p>无法连接服务：{statusError}</p>
              <p className="sidebar-error-hint">请先启动：python3 tools/doc_qa.py --serve</p>
            </div>
          ) : (
            <FileIndex status={status} onOpen={setOpenDoc} />
          )}
        </aside>

        {/* Chat area */}
        <main className="chat">
          <div className="thread" ref={threadRef}>
            {messages.length === 0 ? (
              <div className="empty">
                <div className="empty-icon"><BookOpen size={28} /></div>
                <h1 className="empty-title">向项目文档提问</h1>
                <p className="empty-desc">
                  基于分级索引：先从 {status?.file_count ?? '所有'} 个文档摘要中挑选相关文件，再加载全文回答。
                  每个回答都会标注引用的文档来源。
                </p>
                <div className="empty-suggestions">
                  {SUGGESTIONS.map(s => (
                    <button key={s} className="suggestion" onClick={() => send(s)} disabled={busy || !status}>
                      {s}
                    </button>
                  ))}
                </div>
              </div>
            ) : (
              <div className="messages">
                {messages.map(m => <MessageView key={m.id} msg={m} onOpenDoc={setOpenDoc} />)}
                {busy && (
                  <div className="msg msg-assistant">
                    <div className="msg-avatar msg-avatar-ai"><Sparkles size={14} /></div>
                    <div className="msg-body msg-thinking">
                      <Loader2 size={14} className="spinning" />
                      <span>检索文档并生成回答...</span>
                    </div>
                  </div>
                )}
              </div>
            )}
          </div>

          {/* Composer */}
          <div className="composer">
            <textarea
              ref={inputRef}
              className="composer-input"
              placeholder="输入问题，Enter 发送，Shift+Enter 换行"
              value={input}
              onChange={e => setInput(e.target.value)}
              onKeyDown={onKeyDown}
              rows={1}
              disabled={busy || !status}
              aria-label="提问输入框"
            />
            <button
              className="composer-send"
              onClick={() => send(input)}
              disabled={busy || !input.trim() || !status}
              aria-label="发送问题"
            >
              {busy ? <Loader2 size={16} className="spinning" /> : <Send size={16} />}
            </button>
          </div>
        </main>

        {/* Document viewer panel */}
        {openDoc && <DocViewer path={openDoc} onClose={() => setOpenDoc(null)} />}
      </div>
    </div>
  );
}
