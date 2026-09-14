"""Polling tailer that follows ChronoLite log files across rotations."""

from __future__ import annotations

import json
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Iterator

from .chrono_parser import LogEvent, parse_log_line
from .config import Settings


@dataclass(frozen=True)
class TailEvent:
    event: LogEvent
    context: str


class LogTailer:
    def __init__(self, settings: Settings):
        self.settings = settings
        self.state = self._load_state()
        self.recent_lines: dict[str, list[str]] = {}

    def follow_once(self) -> Iterator[TailEvent]:
        for path in sorted(self.settings.log_dir.glob("*.log")):
            if path.resolve() == self.settings.daemon_log_file.resolve():
                continue
            yield from self._read_new_lines(path)
        self._save_state()

    def _read_new_lines(self, path: Path) -> Iterator[TailEvent]:
        stat = path.stat()
        key = str(path.resolve())
        file_state = self.state.setdefault("files", {}).setdefault(key, {})
        old_inode = file_state.get("inode")
        offset = int(file_state.get("offset", 0))
        if old_inode != stat.st_ino or offset > stat.st_size:
            offset = 0
        line_number = int(file_state.get("line_number", 0))
        if offset == 0:
            line_number = 0
        with path.open("r", encoding="utf-8", errors="replace") as handle:
            handle.seek(offset)
            for raw_line in handle:
                line_number += 1
                line = raw_line.rstrip("\n")
                self._remember_line(key, line)
                event = parse_log_line(line, key, line_number)
                if event and event.level in self.settings.levels:
                    context = "\n".join(self.recent_lines.get(key, [])[-self.settings.context_before_lines :])
                    yield TailEvent(event=event, context=context)
            file_state["offset"] = handle.tell()
            file_state["line_number"] = line_number
        file_state["inode"] = stat.st_ino
        file_state["last_seen"] = time.time()

    def _remember_line(self, key: str, line: str) -> None:
        lines = self.recent_lines.setdefault(key, [])
        lines.append(line)
        max_lines = self.settings.context_before_lines + self.settings.context_after_lines + 20
        if len(lines) > max_lines:
            del lines[: len(lines) - max_lines]

    def _load_state(self) -> dict:
        if not self.settings.state_file.exists():
            return {"files": {}, "recent_events": {}}
        try:
            return json.loads(self.settings.state_file.read_text(encoding="utf-8"))
        except (json.JSONDecodeError, OSError):
            return {"files": {}, "recent_events": {}}

    def _save_state(self) -> None:
        self.settings.state_file.write_text(json.dumps(self.state, indent=2), encoding="utf-8")
