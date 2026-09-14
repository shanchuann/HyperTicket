"""Parser for HyperTicket ChronoLite log lines."""

from __future__ import annotations

import hashlib
import re
from dataclasses import dataclass

LOG_LINE_RE = re.compile(
    r"^(?P<timestamp>\d{4}/\d{2}/\d{2}\s+\d{2}:\d{2}:\d{2})\s+"
    r"(?P<thread_id>\S+)\s+"
    r"(?P<level>TRACE|DEBUG|INFO|WARN|ERROR|FATAL)\s+"
    r"(?P<source_file>\S+)\s+"
    r"(?P<function>\S+)\s+"
    r"(?P<line>\d+):\s*"
    r"(?P<message>.*)$"
)


@dataclass(frozen=True)
class LogEvent:
    timestamp: str
    thread_id: str
    level: str
    source_file: str
    function: str
    line: int
    message: str
    raw: str
    log_file: str = ""
    line_number: int | None = None

    @property
    def location(self) -> str:
        return f"{self.source_file}:{self.line} {self.function}"

    def fingerprint(self) -> str:
        normalized_message = re.sub(r"\b\d+\b", "<num>", self.message)[:200]
        body = "|".join(
            [self.level, self.source_file, self.function, str(self.line), normalized_message]
        )
        return hashlib.sha256(body.encode("utf-8")).hexdigest()


def parse_log_line(line: str, log_file: str = "", line_number: int | None = None) -> LogEvent | None:
    match = LOG_LINE_RE.match(line.rstrip("\n"))
    if not match:
        return None
    values = match.groupdict()
    return LogEvent(
        timestamp=values["timestamp"],
        thread_id=values["thread_id"],
        level=values["level"].strip(),
        source_file=values["source_file"],
        function=values["function"],
        line=int(values["line"]),
        message=values["message"],
        raw=line.rstrip("\n"),
        log_file=log_file,
        line_number=line_number,
    )
