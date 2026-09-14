/**
 * Doc QA API client. Base: same origin under /doc-qa/api/
 */

const BASE = '/doc-qa/api';

async function request<T>(path: string, init?: RequestInit): Promise<T> {
  const res = await fetch(`${BASE}${path}`, {
    headers: { 'Content-Type': 'application/json', ...(init?.headers ?? {}) },
    ...init,
  });
  const data = await res.json();
  if (!res.ok) throw new Error((data as { error?: string }).error ?? `HTTP ${res.status}`);
  return data as T;
}

export interface QaStatus {
  model: string;
  models: string[];
  file_count: number;
  files: string[];
}

export interface AskResult {
  answer: string;
  files: string[];
}

export interface DocFile {
  path: string;
  content: string;
}

export const api = {
  status: () => request<QaStatus>('/status'),
  file: (path: string) => request<DocFile>(`/file?path=${encodeURIComponent(path)}`),
  switchModel: (model: string) =>
    request<{ active: string }>('/model', {
      method: 'POST',
      body: JSON.stringify({ model }),
    }),
  ask: (question: string, session: string) =>
    request<AskResult>('/ask', {
      method: 'POST',
      body: JSON.stringify({ question, session }),
    }),
  clear: (session: string) =>
    request<{ ok: boolean }>('/clear', {
      method: 'POST',
      body: JSON.stringify({ session }),
    }),
};
