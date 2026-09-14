"""Integration with a local codegraph index.

The colbymchenry/codegraph CLI may evolve, so exact commands are configurable via
LOG_AI_CODEGRAPH_*_CMD templates. Placeholders: {bin}, {project_root}, {index_dir},
{query}, {diff_file}, {changed_files}.
"""

from __future__ import annotations

import shlex
import shutil
import subprocess
import tempfile
from dataclasses import dataclass
from pathlib import Path

from .chrono_parser import LogEvent
from .config import Settings


@dataclass(frozen=True)
class CodeContext:
    text: str
    available: bool


class CodeIndex:
    def __init__(self, settings: Settings):
        self.settings = settings

    def ensure_index(self) -> CodeContext:
        if not shutil.which(self.settings.codegraph_bin):
            return CodeContext(
                text=f"codegraph binary not found: {self.settings.codegraph_bin}; fell back to source file search.",
                available=False,
            )
        if self.settings.codegraph_index_cmd:
            return self._run_template(self.settings.codegraph_index_cmd)
        commands = [
            "{bin} index {project_root} --output {index_dir}",
            "{bin} index {project_root}",
        ]
        return self._run_first(commands)

    def update_from_diff(self, diff_text: str, changed_files: list[str] | None = None) -> CodeContext:
        if not shutil.which(self.settings.codegraph_bin):
            return CodeContext(f"codegraph binary not found: {self.settings.codegraph_bin}", False)
        changed_files = changed_files or []
        if self.settings.codegraph_update_cmd:
            with tempfile.NamedTemporaryFile("w", encoding="utf-8", delete=False, suffix=".diff") as handle:
                handle.write(diff_text)
                diff_path = handle.name
            try:
                return self._run_template(
                    self.settings.codegraph_update_cmd,
                    diff_file=diff_path,
                    changed_files=" ".join(shlex.quote(item) for item in changed_files),
                )
            finally:
                Path(diff_path).unlink(missing_ok=True)
        # Safe fallback when exact incremental command is unknown: rebuild/update full index.
        return self.ensure_index()

    def search_for_event(self, event: LogEvent, diff_text: str | None = None) -> CodeContext:
        snippets = []
        if shutil.which(self.settings.codegraph_bin):
            for query in self._event_queries(event):
                result = self.search(query)
                if result.text:
                    snippets.append(f"## codegraph query: {query}\n{result.text}")
                if len(snippets) >= 3:
                    break
        fallback = self._fallback_file_context(event)
        if fallback:
            snippets.append("## fallback source context\n" + fallback)
        if diff_text:
            snippets.append("## diff hint\n" + diff_text[:8_000])
        return CodeContext("\n\n".join(snippets), bool(snippets))

    def search(self, query: str) -> CodeContext:
        if not shutil.which(self.settings.codegraph_bin):
            return CodeContext("", False)
        if self.settings.codegraph_search_cmd:
            return self._run_template(self.settings.codegraph_search_cmd, query=query)
        commands = [
            "{bin} search --index {index_dir} {query}",
            "{bin} search {query}",
            "{bin} query {query}",
        ]
        return self._run_first(commands, query=query)

    def _event_queries(self, event: LogEvent) -> list[str]:
        values = [event.function, event.source_file, event.message[:120]]
        return [value for value in values if value]

    def _fallback_file_context(self, event: LogEvent) -> str:
        candidates = list(self.settings.project_root.rglob(event.source_file))
        if not candidates:
            candidates = [p for p in self.settings.project_root.rglob(f"*{event.source_file}") if p.is_file()]
        if not candidates:
            return ""
        path = sorted(candidates, key=lambda p: ("backend" not in str(p), len(str(p))))[0]
        try:
            lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
        except OSError as exc:
            return f"Could not read {path}: {exc}"
        start = max(0, event.line - 20)
        end = min(len(lines), event.line + 20)
        body = "\n".join(f"{idx + 1}: {lines[idx]}" for idx in range(start, end))
        return f"{path}\n{body}"

    def _run_first(self, templates: list[str], **extra: str) -> CodeContext:
        errors: list[str] = []
        for template in templates:
            result = self._run_template(template, **extra)
            if result.available:
                return result
            errors.append(result.text)
        return CodeContext("\n".join(errors), False)

    def _run_template(self, template: str, **extra: str) -> CodeContext:
        values = {
            "bin": shlex.quote(self.settings.codegraph_bin),
            "project_root": shlex.quote(str(self.settings.codegraph_project)),
            "index_dir": shlex.quote(str(self.settings.codegraph_index_dir)),
            "query": shlex.quote(extra.get("query", "")),
            "diff_file": shlex.quote(extra.get("diff_file", "")),
            "changed_files": extra.get("changed_files", ""),
        }
        command = template.format(**values)
        try:
            completed = subprocess.run(
                command,
                shell=True,
                cwd=self.settings.project_root,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                timeout=120,
                check=False,
            )
        except Exception as exc:
            return CodeContext(f"Command failed to start: {command}\n{exc}", False)
        output = (completed.stdout + "\n" + completed.stderr).strip()
        if completed.returncode != 0:
            return CodeContext(f"Command failed ({completed.returncode}): {command}\n{output}", False)
        return CodeContext(output, True)
