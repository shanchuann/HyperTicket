/**
 * 前端监控上报：捕获运行时错误与关键状态异常，发送到 log-ai-detector。
 *
 * 上报的事件会被写成 ChronoLite 格式日志行（logs/hyperticket.frontend.log），
 * daemon 检测到 ERROR/FATAL 后自动触发 LLM 联调分析并生成报告。
 *
 * 过滤规则：浏览器扩展注入脚本（content_main.js 等非同源脚本）的报错与页面无关，不上报。
 */

const ENDPOINT = `${(import.meta as { env?: Record<string, string> }).env?.VITE_LOG_AI_URL || 'http://127.0.0.1:7070'}/log-ai-detector/api/frontend-event`;

type Level = 'ERROR' | 'FATAL' | 'WARN' | 'INFO';

interface FrontendEvent {
  level: Level;
  source: string;
  message: string;
  context?: Record<string, unknown>;
}

// 同一错误 30 秒内只上报一次，避免刷屏
const recent = new Map<string, number>();
const DEBOUNCE_MS = 30_000;

function shouldSend(key: string): boolean {
  const now = Date.now();
  const last = recent.get(key);
  if (last && now - last < DEBOUNCE_MS) return false;
  recent.set(key, now);
  return true;
}

/** 浏览器扩展注入的脚本报错与页面无关，跳过 */
function isExtensionError(filename: string | undefined): boolean {
  if (!filename) return false;
  return /content_main\.js|extension:\/\/|^chrome-|^moz-/.test(filename);
}

export function reportEvent(event: FrontendEvent): void {
  const key = `${event.source}|${event.message.slice(0, 120)}`;
  if (!shouldSend(key)) return;
  // sendBeacon 优先（页面卸载也能送达）；失败回退 fetch，静默失败不影响页面
  const payload = JSON.stringify({
    ...event,
    context: {
      ...event.context,
      url: location.pathname,
      ua: navigator.userAgent.slice(0, 80),
    },
  });
  try {
    const ok = navigator.sendBeacon?.(ENDPOINT, new Blob([payload], { type: 'application/json' }));
    if (!ok) {
      fetch(ENDPOINT, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: payload })
        .catch(() => {});
    }
  } catch {
    /* 监控不可用时静默，绝不影响业务 */
  }
}

/** 业务状态异常上报：如接口返回空列表但预期有数据、状态机进入非法状态等 */
export function reportStateAnomaly(source: string, message: string, context?: Record<string, unknown>): void {
  reportEvent({ level: 'ERROR', source, message: `[状态异常] ${message}`, context });
}

/** 安装全局错误钩子（入口调用一次） */
export function installMonitor(): void {
  window.addEventListener('error', (e) => {
    if (isExtensionError(e.filename)) return;
    reportEvent({
      level: 'ERROR',
      source: (e.filename || 'unknown').split('/').pop() || 'unknown',
      message: e.message,
      context: { line: e.lineno, col: e.colno, stack: e.error?.stack?.slice(0, 800) },
    });
  });

  window.addEventListener('unhandledrejection', (e) => {
    const reason = e.reason instanceof Error ? e.reason : new Error(String(e.reason));
    if (isExtensionError(reason.stack?.split('\n')[1])) return;
    reportEvent({
      level: 'ERROR',
      source: 'promise',
      message: `未处理的 Promise 拒绝: ${reason.message}`,
      context: { stack: reason.stack?.slice(0, 800) },
    });
  });
}
