"""Markdown report generation."""

from __future__ import annotations

import re
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path

from .chrono_parser import LogEvent
from .config import Settings

# 表情符号及杂项符号区段；报告要求纯文本，LLM 偶尔无视指令时兜底过滤。
_EMOJI_RE = re.compile(
    "["
    "\U0001F300-\U0001FAFF"   # 符号、表情、交通、补充区
    "\U00002600-\U000027BF"   # 杂项符号 + 装饰符号（含 ⚠✅❌）
    "\U0001F1E6-\U0001F1FF"   # 区域指示（旗帜）
    "\U00002B00-\U00002BFF"   # 杂项符号与箭头（含 ⭐）
    "\U0000FE0F"              # 变体选择符
    "]+"
)


def strip_emoji(text: str) -> str:
    return _EMOJI_RE.sub("", text)


# LLM 偶发的开场白（"好的，以下是……"），报告只保留正文。
_PREAMBLE_RE = re.compile(
    r"^\s*(好的|以下|下面|根据|遵命|当然)[^\n]{0,80}(报告|分析)[。：:．]?\s*\n+",
)


def strip_preamble(markdown: str) -> str:
    """剥离首个标题之前的客套引言；若无标题则原样返回。"""
    stripped = _PREAMBLE_RE.sub("", markdown, count=1)
    # 引言变体较多，双保险：若首个非空行不是标题且 2 行内出现标题，删掉标题前内容
    lines = stripped.split("\n")
    for idx, line in enumerate(lines[:3]):
        if line.strip().startswith("#"):
            return "\n".join(lines[idx:])
        if line.strip() and not line.strip().startswith("#"):
            for j in range(idx + 1, min(idx + 3, len(lines))):
                if lines[j].strip().startswith("#"):
                    return "\n".join(lines[j:])
            break
    return stripped


@dataclass(frozen=True)
class AnalysisReport:
    path: Path
    summary: str
    event: LogEvent
    fingerprint: str
    created_at: str


def _utc_stamp() -> str:
    return datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%SZ")


def _iso_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def first_heading_or_line(markdown: str) -> str:
    for line in markdown.splitlines():
        stripped = line.strip("# ").strip()
        if stripped:
            return stripped[:160]
    return "LLM analysis completed"


class Reporter:
    def __init__(self, settings: Settings):
        self.settings = settings
        self.settings.report_dir.mkdir(parents=True, exist_ok=True)

    def write(
        self,
        event: LogEvent,
        context: str,
        code_context: str,
        diff_context: str,
        llm_markdown: str,
    ) -> AnalysisReport:
        fingerprint = event.fingerprint()
        created_at = _iso_now()
        filename = f"{_utc_stamp()}-{event.level}-{fingerprint[:8]}.md"
        path = self.settings.report_dir / filename
        llm_markdown = strip_preamble(strip_emoji(llm_markdown))
        summary = first_heading_or_line(llm_markdown)
        content = self._render(
            event=event,
            context=context,
            code_context=code_context,
            diff_context=diff_context,
            llm_markdown=llm_markdown,
            fingerprint=fingerprint,
            created_at=created_at,
        )
        path.write_text(content, encoding="utf-8")
        return AnalysisReport(path=path, summary=summary, event=event, fingerprint=fingerprint, created_at=created_at)

    def _render(
        self,
        event: LogEvent,
        context: str,
        code_context: str,
        diff_context: str,
        llm_markdown: str,
        fingerprint: str,
        created_at: str,
    ) -> str:
        return f"""# HyperTicket 日志错误分析报告

## 1. 概览

- 生成时间：{created_at}
- 日志时间：{event.timestamp}
- 日志级别：{event.level}
- 日志文件：`{event.log_file}`
- 日志行号：{event.line_number if event.line_number is not None else "未知"}
- 线程 ID：`{event.thread_id}`
- 触发位置：`{event.source_file}:{event.line}` `{event.function}`
- fingerprint：`{fingerprint}`

## 2. 触发日志

```text
{event.raw}
```

## 3. 日志上下文

```text
{context}
```

## 4. 相关代码线索

```text
{code_context or "未获得代码索引结果。"}
```

## 5. 最近 feat commit / diff 线索

```diff
{diff_context or "未发现可用的 feat commit diff。"}
```

## 6. 大模型分析结论

{llm_markdown}
"""
