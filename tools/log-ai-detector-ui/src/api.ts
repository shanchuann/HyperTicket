/**
 * API client for the log-ai-detector HTTP server.
 * Base URL: same origin under /log-ai-detector/api/
 */

const BASE = '/log-ai-detector/api';

async function request<T>(path: string, init?: RequestInit): Promise<T> {
  const res = await fetch(`${BASE}${path}`, {
    headers: { 'Content-Type': 'application/json', ...(init?.headers ?? {}) },
    ...init,
  });
  const data = await res.json();
  if (!res.ok) throw new Error((data as { error?: string }).error ?? `HTTP ${res.status}`);
  return data as T;
}

export interface DetectorStatus {
  running: boolean;
  model: string;
  llm_base_url: string;
  levels: string[];
  log_dir: string;
  report_dir: string;
  timestamp: string;
}

export interface ModelsInfo {
  models: string[];
  active: string;
}

export interface ReportMeta {
  name: string;
  level: 'ERROR' | 'FATAL' | 'WARN' | 'INFO' | 'DEBUG' | 'UNKNOWN';
  created_at: string;
  fingerprint: string;
  summary: string;
  size: number;
}

export interface ReportDetail extends ReportMeta {
  content: string;
}

export const api = {
  status:       () => request<DetectorStatus>('/status'),
  models:       () => request<ModelsInfo>('/models'),
  switchModel:  (model: string) => request<{ active: string; message: string }>('/model', {
    method: 'POST', body: JSON.stringify({ model }),
  }),
  reports:      (limit = 100) => request<{ reports: ReportMeta[]; total: number }>(`/reports?limit=${limit}`),
  reportDetail: (name: string) => request<ReportDetail>(`/reports/${encodeURIComponent(name)}`),
  daemonLogs:   (n = 200) => request<{ lines: string[] }>(`/logs?n=${n}`),
  config:       () => request<Record<string, string>>('/config'),
  analyze:      (file: string) => request<{ ok: boolean; reports: string[]; count: number }>('/analyze', {
    method: 'POST', body: JSON.stringify({ file }),
  }),
};
