"""Command-line entrypoint and daemon loop for log AI detection."""

from __future__ import annotations

import argparse
import atexit
import os
import signal
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

from .analyzer import Analyzer, analyze_file_once
from .config import Settings
from .log_tailer import LogTailer


def log(settings: Settings, message: str) -> None:
    settings.daemon_log_file.parent.mkdir(parents=True, exist_ok=True)
    line = f"{datetime.now(timezone.utc).isoformat()} {message}\n"
    with settings.daemon_log_file.open("a", encoding="utf-8") as handle:
        handle.write(line)
    print(message, flush=True)


def check_config(settings: Settings) -> int:
    errors = settings.validate(require_llm=True)
    print("Configuration:")
    for key, value in settings.sanitized_summary().items():
        print(f"  {key}: {value}")
    if errors:
        print("\nErrors:")
        for error in errors:
            print(f"  - {error}")
        return 2
    print("\nOK")
    return 0


def _write_pid(settings: Settings) -> None:
    """写入 PID 文件（供 server.py 状态查询），进程退出时自动清理。"""
    settings.pid_file.write_text(str(os.getpid()), encoding="utf-8")

    def _cleanup() -> None:
        try:
            if settings.pid_file.exists() and settings.pid_file.read_text().strip() == str(os.getpid()):
                settings.pid_file.unlink()
        except OSError:
            pass

    atexit.register(_cleanup)
    # atexit 不会在 SIGTERM 下执行；转换为 SystemExit 以走正常退出路径。
    signal.signal(signal.SIGTERM, lambda *_: sys.exit(0))


def run_daemon(settings: Settings) -> int:
    errors = settings.validate(require_llm=True)
    if errors:
        raise RuntimeError("Invalid configuration:\n" + "\n".join(errors))
    settings.report_dir.mkdir(parents=True, exist_ok=True)
    _write_pid(settings)
    tailer = LogTailer(settings)
    analyzer = Analyzer(settings)
    recent: dict[str, float] = {}
    log(settings, f"log-ai-detector started; watching {settings.log_dir}")
    while True:
        for tail_event in tailer.follow_once():
            fp = tail_event.event.fingerprint()
            now = time.time()
            if fp in recent and now - recent[fp] < settings.debounce_seconds:
                continue
            recent[fp] = now
            try:
                report = analyzer.analyze(tail_event.event, tail_event.context)
                log(settings, f"generated report: {report.path}")
            except Exception as exc:
                log(settings, f"analysis failed for {tail_event.event.log_file}: {exc}")
        time.sleep(settings.poll_interval_seconds)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Monitor HyperTicket logs and analyze ERROR/FATAL entries with an LLM")
    parser.add_argument("--project-root", default=None)
    parser.add_argument("--check-config", action="store_true")
    parser.add_argument("--once", action="store_true", help="analyze existing ERROR/FATAL entries in a file once")
    parser.add_argument("--file", type=Path, help="log file for --once")
    parser.add_argument("--serve", action="store_true", help="run the HTTP API server alongside the daemon")
    parser.add_argument("--serve-only", action="store_true", help="run the HTTP API server only (no monitoring)")
    parser.add_argument("--host", default="127.0.0.1", help="HTTP server bind host (default: 127.0.0.1)")
    parser.add_argument("--port", type=int, default=7070, help="HTTP server port (default: 7070)")
    args = parser.parse_args(argv)

    settings = Settings.load(Path(args.project_root).resolve() if args.project_root else None)
    if args.check_config:
        return check_config(settings)
    if args.once:
        if not args.file:
            parser.error("--once requires --file")
        reports = analyze_file_once(settings, args.file.resolve())
        for report in reports:
            print(report.path)
        return 0

    if args.serve_only:
        from .server import serve
        serve(settings, host=args.host, port=args.port)
        return 0

    if args.serve:
        import threading
        from .server import serve
        t = threading.Thread(target=serve, args=(settings, args.host, args.port), daemon=True)
        t.start()

    return run_daemon(settings)


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(130)
