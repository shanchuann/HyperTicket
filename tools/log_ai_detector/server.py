"""Lightweight HTTP API server for the log AI detector UI.

Serves the standalone UI (tools/log-ai-detector-ui/dist/) under /log-ai-detector/
and exposes all API endpoints under /log-ai-detector/api/  (also /api/ for dev proxy).

Endpoints:
  GET  /log-ai-detector/api/status
  GET  /log-ai-detector/api/models
  POST /log-ai-detector/api/model         { "model": "..." }
  GET  /log-ai-detector/api/reports       ?limit=100
  GET  /log-ai-detector/api/reports/:name
  GET  /log-ai-detector/api/logs          ?n=200
  POST /log-ai-detector/api/analyze       { "file": "..." }
  GET  /log-ai-detector/api/config
  GET  /log-ai-detector/                  (SPA shell + static assets)
"""

from __future__ import annotations

import json
import mimetypes
import os
import re
import threading
from datetime import datetime, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any
from urllib.parse import urlparse, parse_qs

from .config import Settings
from .analyzer import analyze_file_once


# ── Static file root ──────────────────────────────────────────────────────────

# __file__ = tools/log_ai_detector/server.py → parents[1] = tools/
_UI_DIST = Path(__file__).resolve().parents[1] / "log-ai-detector-ui" / "dist"

_CORS_HEADERS = {
    "Access-Control-Allow-Origin": "*",
    "Access-Control-Allow-Methods": "GET, POST, OPTIONS",
    "Access-Control-Allow-Headers": "Content-Type",
}


def _utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def _shorten_summary(text: str, max_chars: int = 12) -> str:
    """压缩为极简标题（目标 5 字左右）：匹配常见故障模式给出短语，否则截断。"""
    s = re.sub(r"[`*_]", "", text)
    s = re.sub(r"\s+", " ", s).strip()
    # 常见故障模式 → 短标题（按优先级）
    patterns = [
        (r"列表为空|返回成功但.{0,6}为空|num=0", "列表数据为空"),
        (r"Access denied|认证被拒|密码.{0,4}(错误|不匹配)", "数据库认证失败"),
        (r"(连接|connect).{0,8}(失败|拒绝|超时)|Connection refused", "连接失败"),
        (r"超时|timeout", "请求超时"),
        (r"SQL|数据库.{0,4}(错误|异常)", "数据库错误"),
        (r"崩溃|crash|段错误|core dump", "进程崩溃"),
        (r"内存|memory", "内存异常"),
        (r"未处理的 Promise|unhandled", "未处理异常"),
        (r"undefined|null|NaN|TypeError", "前端类型错误"),
        (r"404|路径.{0,4}不存在|Not [Ff]ound", "资源不存在"),
        (r"401|403|UNAUTHORIZED|鉴权|token.{0,6}(失效|过期)", "鉴权失败"),
        (r"超卖|库存", "库存异常"),
    ]
    for pat, label in patterns:
        if re.search(pat, s):
            return label
    # 无匹配：取首个逗号/句号前的主干，硬截断
    s = re.split(r"[，。；,;:：]", s, maxsplit=1)[0].strip()
    if len(s) > max_chars:
        s = s[:max_chars] + "…"
    return s


def _report_meta(path: Path) -> dict[str, Any]:
    name = path.name
    m = re.match(r"^(\d{8}-\d{6}Z)-(ERROR|FATAL|WARN|INFO|DEBUG)-([0-9a-f]+)\.md$", name)
    summary = ""
    if path.stat().st_size < 128_000:
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
            # 列表标题优先展示主要错误原因：取 LLM 分析"摘要"章节的首句
            sm = re.search(r"^#{2,3}\s*摘要\s*$\n+(.+)$", text, re.MULTILINE)
            if sm:
                summary = _shorten_summary(sm.group(1))
            else:
                # 回退：触发日志的 message 部分
                lm = re.search(r"^## 2\. 触发日志\s*\n+```text\n[^\n]*?\d+:\s*(.+)$", text, re.MULTILINE)
                if lm:
                    summary = _shorten_summary(lm.group(1))
                else:
                    for line in text.splitlines():
                        stripped = line.strip("# ").strip()
                        if stripped and not stripped.startswith("HyperTicket"):
                            summary = _shorten_summary(stripped)
                            break
        except OSError:
            pass
    return {
        "name": name,
        "level": m.group(2) if m else "UNKNOWN",
        "created_at": m.group(1) if m else "",
        "fingerprint": m.group(3) if m else "",
        "summary": summary,
        "size": path.stat().st_size,
    }


def _switch_model(settings: Settings, model: str) -> None:
    if model not in settings.available_models:
        raise ValueError(f"Unknown model: {model}. Available: {list(settings.available_models)}")
    settings.selected_model_file.write_text(model, encoding="utf-8")


def _daemon_running(settings: Settings) -> bool:
    if not settings.pid_file.exists():
        return False
    try:
        pid = int(settings.pid_file.read_text().strip())
        os.kill(pid, 0)
        return True
    except (ValueError, OSError):
        return False


def _daemon_logs(settings: Settings, n: int = 200) -> list[str]:
    try:
        if not settings.daemon_log_file.exists():
            return []
        lines = settings.daemon_log_file.read_text(encoding="utf-8", errors="replace").splitlines()
        return lines[-n:]
    except OSError:
        return []


class _Handler(BaseHTTPRequestHandler):
    settings: Settings  # injected before serve()

    def log_message(self, _fmt: str, *_args: Any) -> None:
        pass  # silence default access log

    def _send_json(self, code: int, body: Any) -> None:
        data = json.dumps(body, ensure_ascii=False).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        for k, v in _CORS_HEADERS.items():
            self.send_header(k, v)
        self.end_headers()
        self.wfile.write(data)

    def _send_file(self, path: Path) -> None:
        data = path.read_bytes()
        mime, _ = mimetypes.guess_type(str(path))
        self.send_response(200)
        self.send_header("Content-Type", mime or "application/octet-stream")
        self.send_header("Content-Length", str(len(data)))
        if path.suffix == ".html":
            # SPA 外壳不缓存：确保引用到最新指纹的 JS/CSS bundle
            self.send_header("Cache-Control", "no-cache, must-revalidate")
        else:
            self.send_header("Cache-Control", "public, max-age=31536000, immutable")
        self.end_headers()
        self.wfile.write(data)

    def _send_spa(self) -> None:
        """Serve the SPA index.html for any unmatched /log-ai-detector/ path."""
        index = _UI_DIST / "index.html"
        if index.exists():
            self._send_file(index)
        else:
            self._send_json(503, {"error": "UI not built. Run: cd tools/log-ai-detector-ui && npm install && npm run build"})

    def do_OPTIONS(self) -> None:
        self.send_response(204)
        for k, v in _CORS_HEADERS.items():
            self.send_header(k, v)
        self.end_headers()

    # ── Routing ───────────────────────────────────────────────────────────────

    def _route_api(self, method: str, api_path: str, qs: dict[str, list[str]]) -> None:
        """Handle /log-ai-detector/api/<api_path>."""
        settings = self.settings

        # Optional bearer-token auth (LOG_AI_API_TOKEN). Health stays open.
        if settings.api_token and api_path not in ("", "health"):
            supplied = self.headers.get("Authorization", "")
            if supplied != f"Bearer {settings.api_token}":
                self._send_json(401, {"error": "Unauthorized"})
                return

        # GET endpoints ─────────────────────────────────────────────────────
        if method == "GET":
            if api_path == "status":
                self._send_json(200, {
                    "running": _daemon_running(settings),
                    "model": settings.llm_model,
                    "llm_base_url": settings.llm_base_url,
                    "levels": settings.levels,
                    "log_dir": str(settings.log_dir),
                    "report_dir": str(settings.report_dir),
                    "timestamp": _utc_now(),
                })

            elif api_path == "models":
                self._send_json(200, {"models": list(settings.available_models), "active": settings.llm_model})

            elif api_path == "reports":
                limit = int(qs.get("limit", ["100"])[0])
                reports: list[dict[str, Any]] = []
                if settings.report_dir.exists():
                    paths = sorted(
                        (p for p in settings.report_dir.glob("*.md") if p.name != "daemon.log"),
                        key=lambda p: p.name, reverse=True,
                    )[:limit]
                    reports = [_report_meta(p) for p in paths]
                self._send_json(200, {"reports": reports, "total": len(reports)})

            elif api_path.startswith("reports/"):
                name = api_path[len("reports/"):]
                if not re.match(r"^[\w\-.]+\.md$", name):
                    self._send_json(400, {"error": "Invalid report name"}); return
                report_path = settings.report_dir / name
                if not report_path.exists():
                    self._send_json(404, {"error": "Report not found"}); return
                content = report_path.read_text(encoding="utf-8", errors="replace")
                self._send_json(200, {"name": name, "content": content, **_report_meta(report_path)})

            elif api_path == "logs":
                n = int(qs.get("n", ["200"])[0])
                self._send_json(200, {"lines": _daemon_logs(settings, n)})

            elif api_path == "config":
                self._send_json(200, settings.sanitized_summary())

            elif api_path in ("", "health"):
                self._send_json(200, {"status": "ok", "service": "log-ai-detector"})

            else:
                self._send_json(404, {"error": "Not found"})

        # POST endpoints ────────────────────────────────────────────────────
        elif method == "POST":
            length = int(self.headers.get("Content-Length", 0))
            body: dict[str, Any] = {}
            if length > 0:
                try:
                    body = json.loads(self.rfile.read(length).decode("utf-8"))
                except (json.JSONDecodeError, UnicodeDecodeError):
                    self._send_json(400, {"error": "Invalid JSON"}); return

            if api_path == "model":
                model = body.get("model", "")
                if not model:
                    self._send_json(400, {"error": "model is required"}); return
                try:
                    _switch_model(settings, model)
                except ValueError as exc:
                    self._send_json(400, {"error": str(exc)}); return
                _Handler.settings = Settings.load(settings.project_root)  # type: ignore[attr-defined]
                self._send_json(200, {"active": model, "message": f"Switched to {model}"})

            elif api_path == "analyze":
                file_path_str = body.get("file", "")
                if not file_path_str:
                    self._send_json(400, {"error": "file is required"}); return
                file_path = Path(file_path_str)
                if not file_path.is_absolute():
                    file_path = settings.log_dir / file_path
                if not file_path.exists():
                    self._send_json(404, {"error": f"Log file not found: {file_path}"}); return
                try:
                    reports_list = analyze_file_once(settings, file_path)
                    self._send_json(200, {"ok": True, "reports": [str(r.path) for r in reports_list], "count": len(reports_list)})
                except Exception as exc:
                    self._send_json(500, {"error": str(exc)})

            elif api_path == "frontend-event":
                # 前端错误/状态上报：写成 ChronoLite 格式日志行，daemon 自动拾取分析。
                level = str(body.get("level", "ERROR")).upper()
                if level not in ("ERROR", "FATAL", "WARN", "INFO"):
                    level = "ERROR"
                source = re.sub(r"[^\w.\-]", "_", str(body.get("source", "frontend"))[:80]) or "frontend"
                message = str(body.get("message", ""))[:2000].replace("\n", " ⏎ ")
                context = body.get("context")
                if context is not None:
                    ctx_json = json.dumps(context, ensure_ascii=False)[:4000]
                    message = f"{message} | context={ctx_json}"
                if not message.strip():
                    self._send_json(400, {"error": "message is required"}); return
                now = datetime.now()
                line = (
                    f"{now.strftime('%Y/%m/%d %H:%M:%S')} 0 {level:<5} "
                    f"{source} frontendEvent 0: {message}\n"
                )
                fe_log = settings.log_dir / "hyperticket.frontend.log"
                settings.log_dir.mkdir(parents=True, exist_ok=True)
                with fe_log.open("a", encoding="utf-8") as fh:
                    fh.write(line)
                self._send_json(200, {"ok": True, "queued": level in settings.levels})
            else:
                self._send_json(404, {"error": "Not found"})

        else:
            self._send_json(405, {"error": "Method not allowed"})

    def _handle(self, method: str) -> None:
        parsed = urlparse(self.path)
        raw_path = parsed.path
        qs = parse_qs(parsed.query)

        # Strip trailing slash for routing logic (keep root intact)
        path = raw_path.rstrip("/") or "/"

        # Normalise the two supported mount points:
        #   /log-ai-detector/api/...  — from production proxy or direct
        #   /api/...                  — from Vite dev proxy (strips prefix)
        if path.startswith("/log-ai-detector/api/"):
            api_path = path[len("/log-ai-detector/api/"):]
            self._route_api(method, api_path, qs)

        elif path == "/log-ai-detector/api":
            self._route_api(method, "", qs)

        elif path.startswith("/api/"):
            api_path = path[len("/api/"):]
            self._route_api(method, api_path, qs)

        elif path == "/api":
            self._route_api(method, "", qs)

        # Static assets under /log-ai-detector/
        elif path.startswith("/log-ai-detector/"):
            asset_rel = path[len("/log-ai-detector/"):]
            asset_path = _UI_DIST / asset_rel
            if asset_path.is_file():
                self._send_file(asset_path)
            else:
                # SPA fallback — any unmatched path returns index.html
                self._send_spa()

        elif path in ("/log-ai-detector", "/"):
            self._send_spa()

        else:
            self._send_json(404, {"error": "Not found"})

    def do_GET(self)  -> None: self._handle("GET")
    def do_POST(self) -> None: self._handle("POST")


def serve(settings: Settings, host: str = "127.0.0.1", port: int = 7070) -> None:
    _Handler.settings = settings  # type: ignore[attr-defined]
    # ThreadingHTTPServer: /analyze 的同步 LLM 调用不会阻塞其他请求（如 UI 静态资源）。
    server = ThreadingHTTPServer((host, port), _Handler)
    if not (_UI_DIST / "index.html").exists():
        print("warning: UI not built — run: cd tools/log-ai-detector-ui && npm install && npm run build", flush=True)
    print(f"log-ai-detector  →  http://{host}:{port}/log-ai-detector/", flush=True)
    print(f"API              →  http://{host}:{port}/log-ai-detector/api/status", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
