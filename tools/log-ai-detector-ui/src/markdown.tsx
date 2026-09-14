/**
 * Minimal markdown renderer for LLM answers.
 * Handles: headings, bold, inline code, fenced code blocks (incl. mermaid),
 * lists, links, tables, horizontal rules.
 * Mermaid diagrams render via the mermaid library; everything else is plain
 * React elements (no dangerouslySetInnerHTML except mermaid's own SVG output).
 */

import { useEffect, useRef, useState } from 'react';
import type { ReactNode } from 'react';

// mermaid 体积大（~500KB gzip），动态导入：仅当内容包含图表时才加载
let mermaidPromise: Promise<typeof import('mermaid')> | null = null;
let mermaidSeq = 0;

function loadMermaid() {
  if (!mermaidPromise) {
    mermaidPromise = import('mermaid').then(mod => {
      mod.default.initialize({
        startOnLoad: false,
        theme: document.documentElement.getAttribute('data-theme') === 'dark' ? 'dark' : 'neutral',
        securityLevel: 'strict',
        fontFamily: '"PingFang SC", "Microsoft YaHei", sans-serif',
      });
      return mod;
    });
  }
  return mermaidPromise;
}

export const MermaidBlock = ({ code }: { code: string }) => {
  const [svg, setSvg] = useState('');
  const [error, setError] = useState('');
  const idRef = useRef(`mmd-${++mermaidSeq}`);

  useEffect(() => {
    let cancelled = false;
    loadMermaid()
      .then(mod => mod.default.render(idRef.current, code))
      .then(({ svg: out }) => { if (!cancelled) setSvg(out); })
      .catch((e: Error) => { if (!cancelled) setError(e.message); });
    return () => { cancelled = true; };
  }, [code]);

  if (error) {
    // 渲染失败时回退为代码块展示原文，便于排查语法错误
    return (
      <div className="md-mermaid-error">
        <p>Mermaid 渲染失败：{error.split('\n')[0]}</p>
        <pre className="md-pre"><code>{code}</code></pre>
      </div>
    );
  }
  if (!svg) return <div className="md-mermaid md-mermaid-loading">图表渲染中...</div>;
  // mermaid 输出的是自身生成的 SVG（securityLevel: strict 已过滤脚本）
  return <div className="md-mermaid" dangerouslySetInnerHTML={{ __html: svg }} />;
};

let keyCounter = 0;
const k = () => `md-${keyCounter++}`;

const HR_RE = /^\s*([-*_])\s*(\1\s*){2,}$/;

function renderInline(text: string): ReactNode[] {
  const out: ReactNode[] = [];
  // Order: inline code first (protects contents), then bold, then links.
  const re = /(`[^`]+`)|(\*\*[^*]+\*\*)|(\[[^\]]+\]\([^)]+\))/g;
  let last = 0;
  let m: RegExpExecArray | null;
  while ((m = re.exec(text)) !== null) {
    if (m.index > last) out.push(text.slice(last, m.index));
    const tok = m[0];
    if (tok.startsWith('`')) {
      out.push(<code key={k()} className="md-code">{tok.slice(1, -1)}</code>);
    } else if (tok.startsWith('**')) {
      out.push(<strong key={k()}>{tok.slice(2, -2)}</strong>);
    } else {
      const lm = /\[([^\]]+)\]\(([^)]+)\)/.exec(tok);
      if (lm) out.push(<a key={k()} href={lm[2]} target="_blank" rel="noreferrer">{lm[1]}</a>);
    }
    last = m.index + tok.length;
  }
  if (last < text.length) out.push(text.slice(last));
  return out;
}

export function renderMarkdown(md: string): ReactNode[] {
  // 统一换行符，避免 \r 干扰行级正则
  const lines = md.replace(/\r\n?/g, '\n').split('\n');
  const blocks: ReactNode[] = [];
  let i = 0;

  while (i < lines.length) {
    const line = lines[i];

    // fenced code block (mermaid blocks render as diagrams)
    if (line.startsWith('```')) {
      const lang = line.slice(3).trim().toLowerCase();
      const buf: string[] = [];
      i++;
      while (i < lines.length && !lines[i].startsWith('```')) { buf.push(lines[i]); i++; }
      i++; // skip closing fence
      const code = buf.join('\n');
      if (lang === 'mermaid') {
        blocks.push(<MermaidBlock key={k()} code={code} />);
      } else {
        blocks.push(<pre key={k()} className="md-pre"><code>{code}</code></pre>);
      }
      continue;
    }

    // horizontal rule (---, ***, ___); must check before table/list/paragraph
    if (HR_RE.test(line)) {
      blocks.push(<hr key={k()} className="md-hr" />);
      i++;
      continue;
    }

    // table (header row + separator row)
    if (line.includes('|') && i + 1 < lines.length && /^\s*\|?[\s:|-]+\|?\s*$/.test(lines[i + 1]) && lines[i + 1].includes('-')) {
      const parseRow = (row: string) => row.split('|').map(c => c.trim()).filter((c, idx, arr) => !(c === '' && (idx === 0 || idx === arr.length - 1)));
      const header = parseRow(line);
      i += 2;
      const rows: string[][] = [];
      while (i < lines.length && lines[i].includes('|')) { rows.push(parseRow(lines[i])); i++; }
      blocks.push(
        <table key={k()} className="md-table">
          <thead><tr>{header.map(h => <th key={k()}>{renderInline(h)}</th>)}</tr></thead>
          <tbody>{rows.map(r => <tr key={k()}>{r.map(c => <td key={k()}>{renderInline(c)}</td>)}</tr>)}</tbody>
        </table>
      );
      continue;
    }

    // heading
    const hm = /^(#{1,4})\s+(.*)$/.exec(line);
    if (hm) {
      const level = hm[1].length;
      const content = renderInline(hm[2]);
      if (level === 1) blocks.push(<h2 key={k()} className="md-h2">{content}</h2>);
      else if (level === 2) blocks.push(<h3 key={k()} className="md-h3">{content}</h3>);
      else blocks.push(<h4 key={k()} className="md-h4">{content}</h4>);
      i++;
      continue;
    }

    // unordered / ordered list
    if (/^\s*[-*]\s+/.test(line) || /^\s*\d+\.\s+/.test(line)) {
      const ordered = /^\s*\d+\.\s+/.test(line);
      const items: ReactNode[] = [];
      while (i < lines.length && (/^\s*[-*]\s+/.test(lines[i]) || /^\s*\d+\.\s+/.test(lines[i]))) {
        items.push(<li key={k()}>{renderInline(lines[i].replace(/^\s*([-*]|\d+\.)\s+/, ''))}</li>);
        i++;
      }
      blocks.push(ordered
        ? <ol key={k()} className="md-list">{items}</ol>
        : <ul key={k()} className="md-list">{items}</ul>);
      continue;
    }

    // blank line
    if (line.trim() === '') { i++; continue; }

    // paragraph: accumulate until blank/structural line
    const buf: string[] = [line];
    i++;
    while (
      i < lines.length && lines[i].trim() !== '' &&
      !lines[i].startsWith('```') && !/^#{1,4}\s/.test(lines[i]) &&
      !/^\s*[-*]\s+/.test(lines[i]) && !/^\s*\d+\.\s+/.test(lines[i]) &&
      !lines[i].includes('|') && !HR_RE.test(lines[i])
    ) { buf.push(lines[i]); i++; }
    blocks.push(<p key={k()} className="md-p">{renderInline(buf.join(' '))}</p>);
  }

  return blocks;
}
