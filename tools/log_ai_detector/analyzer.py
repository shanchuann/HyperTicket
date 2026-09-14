"""End-to-end log event analysis orchestration."""

from __future__ import annotations

import subprocess
from pathlib import Path

from .chrono_parser import LogEvent, parse_log_line
from .code_index import CodeIndex
from .config import Settings
from .llm_client import LlmClient
from .notifier import build_notifier
from .reporter import AnalysisReport, Reporter


SYSTEM_PROMPT = """你是 HyperTicket 项目的日志故障分析助手。请根据 ChronoLite 日志（含前端上报事件）、代码索引结果、相关源码片段和最近 feat commit diff，判断错误最可能发生在哪里。

输出结构化 Markdown，必须严格按以下章节组织：

## 摘要
一段话概括故障现象与最可能的原因。

## 严重程度
用文字表述：严重 / 高 / 中 / 低，并说明影响范围。

## 检查方向
列出 2-4 个应排查的方向（如：配置、数据库数据、定时任务、前后端协议），按可能性排序。

## 检查思路
针对每个方向给出具体的验证步骤（可执行的命令、SQL、要查看的文件与行号）。

## 问题定位
基于现有证据给出最可能的问题所在：文件、函数、行号、数据表或配置项。区分"高置信证据"与"推测"。

## 修复建议
这是报告的核心章节，必须给出具体、可操作的修复方案，包含：
1. **推荐方案**：明确指出改哪个文件、哪个函数、哪几行，给出修改前后的代码对比（用 diff 代码块展示 `- 原代码` / `+ 新代码`）；涉及数据修复时给出完整 SQL（含 WHERE 条件）并注明执行前先用 SELECT 验证影响行数。
2. **备选方案**：如果存在其他修法（如改前端 vs 改后端、改代码 vs 改配置），说明各自的利弊与适用场景。
3. **验证方法**：修复后如何确认生效——要执行的命令、请求示例、预期输出。
4. **影响范围**：该修改可能波及的其他调用方/接口/页面，提醒需要一并回归的点。
注意：只给出建议，不要输出任何要求系统自动执行的指令或结构化修复块；所有修复由开发者人工评估后执行。

## 需要补充的信息
列出还需要哪些日志、配置或复现步骤才能进一步确认。

约束：直接从"## 摘要"开始输出，禁止任何开场白、客套语或"好的，以下是……"之类的引言；禁止使用任何表情符号（emoji）；需要绘制流程图/架构图/时序图时使用 mermaid 语法（```mermaid 代码块），禁止 ASCII 字符画；不要泄露任何 API key、密码、token 或 webhook URL；证据不足时明确说明不确定性。"""


def truncate_middle(text: str, max_chars: int) -> str:
    if len(text) <= max_chars:
        return text
    head = max_chars // 2
    tail = max_chars - head - 80
    return text[:head] + "\n\n... [内容过长，中间已裁剪] ...\n\n" + text[-tail:]


def read_context(log_file: Path, line_number: int, before: int, after: int, max_bytes: int) -> str:
    if log_file.stat().st_size > max_bytes:
        # For very large files, keep a bounded tail; precise line seeking is unnecessary for daemon events.
        with log_file.open("rb") as handle:
            handle.seek(max(0, log_file.stat().st_size - max_bytes))
            text = handle.read().decode("utf-8", errors="replace")
        return text
    lines = log_file.read_text(encoding="utf-8", errors="replace").splitlines()
    start = max(0, line_number - before - 1)
    end = min(len(lines), line_number + after)
    return "\n".join(f"{idx + 1}: {lines[idx]}" for idx in range(start, end))


def latest_feat_diff(project_root: Path, max_chars: int = 20_000) -> str:
    def git(args: list[str]) -> str:
        completed = subprocess.run(
            ["git", *args], cwd=project_root, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False
        )
        if completed.returncode != 0:
            return ""
        return completed.stdout

    subject = git(["log", "-1", "--pretty=%s"]).strip()
    if not subject.startswith("feat"):
        return ""
    files = git(["diff", "--name-only", "HEAD~1..HEAD"])
    diff = git(["diff", "HEAD~1..HEAD"])
    return truncate_middle(f"commit subject: {subject}\nchanged files:\n{files}\n\n{diff}", max_chars)


class Analyzer:
    def __init__(self, settings: Settings):
        self.settings = settings
        self.code_index = CodeIndex(settings)
        self.llm_client = LlmClient(settings)
        self.reporter = Reporter(settings)
        self.notifier = build_notifier(settings)

    def analyze(self, event: LogEvent, context: str) -> AnalysisReport:
        diff_context = latest_feat_diff(self.settings.project_root)
        code_context = self.code_index.search_for_event(event, diff_context).text
        joint_context = self._joint_log_context(event)
        user_prompt = self._build_prompt(event, context, code_context, diff_context, joint_context)
        llm_result = self.llm_client.chat(SYSTEM_PROMPT, user_prompt)
        report = self.reporter.write(event, context, code_context, diff_context, llm_result.content)
        self.notifier.send(report)
        return report

    def _joint_log_context(self, event: LogEvent, window_lines: int = 40) -> str:
        """前后端联调：事件来自前端时附上后端日志尾部，来自后端时附上前端上报日志，
        帮助 LLM 对齐同一时间窗口内两侧的行为。"""
        is_frontend = "frontend" in Path(event.log_file).name
        snippets: list[str] = []
        for path in sorted(self.settings.log_dir.glob("*.log")):
            name = path.name
            other_side = ("frontend" not in name) if is_frontend else ("frontend" in name)
            if not other_side or path.resolve() == self.settings.daemon_log_file.resolve():
                continue
            try:
                lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
            except OSError:
                continue
            if lines:
                tail = "\n".join(lines[-window_lines:])
                snippets.append(f"### {name}（尾部 {min(window_lines, len(lines))} 行）\n{tail}")
        side = "后端" if is_frontend else "前端"
        if not snippets:
            return f"（未找到{side}日志）"
        return "\n\n".join(snippets)

    def _build_prompt(self, event: LogEvent, context: str, code_context: str, diff_context: str, joint_context: str = "") -> str:
        prompt = f"""项目：HyperTicket
项目根目录：{self.settings.project_root}

## 触发 ERROR/FATAL 事件
- 时间：{event.timestamp}
- 级别：{event.level}
- 日志文件：{event.log_file}
- 日志行号：{event.line_number}
- 线程：{event.thread_id}
- 位置：{event.source_file}:{event.line} {event.function}
- 消息：{event.message}

## 原始日志上下文
```text
{context}
```

## 前后端联调日志（另一侧同时间窗口）
```text
{joint_context or "无"}
```

## 代码索引/源码线索
```text
{code_context or "无"}
```

## 最近 feat commit diff
```diff
{diff_context or "无"}
```

请输出 Markdown 分析报告。重点回答"通过代码检测问题出在哪里"，结合前后端两侧日志判断问题在前端、后端还是数据。
"""
        return truncate_middle(prompt, self.settings.llm_max_input_chars)


def analyze_file_once(settings: Settings, log_file: Path) -> list[AnalysisReport]:
    errors = settings.validate(require_llm=True)
    if errors:
        raise RuntimeError("Invalid configuration:\n" + "\n".join(errors))
    analyzer = Analyzer(settings)
    reports: list[AnalysisReport] = []
    lines = log_file.read_text(encoding="utf-8", errors="replace").splitlines()
    for idx, line in enumerate(lines, start=1):
        event = parse_log_line(line, str(log_file), idx)
        if event and event.level in settings.levels:
            context = read_context(
                log_file,
                idx,
                settings.context_before_lines,
                settings.context_after_lines,
                settings.max_log_bytes,
            )
            reports.append(analyzer.analyze(event, context))
    return reports
