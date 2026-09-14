#!/usr/bin/env python3
"""HyperTicket 文档问答机器人 — 分级索引 + 渐进式披露

流程：
1. 构建索引：每个 .md 文件生成一行摘要（路径 + 首段）
2. 第一轮 LLM 调用：根据问题从索引中选出相关文件（返回文件路径列表）
3. 第二轮 LLM 调用：加载选中文件全文，生成最终回答
4. 若回答中途发现需要更多文件，可追加加载（渐进式披露）

用法：
    python3 tools/doc_qa.py                       # 交互式 CLI
    python3 tools/doc_qa.py -q "问题"              # 单次提问
    python3 tools/doc_qa.py --serve               # Web UI（http://127.0.0.1:7171/doc-qa/）
"""

from __future__ import annotations

import argparse
import json
import os
import sys
import urllib.error
import urllib.request
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
EXCLUDE_DIRS = {".git", "node_modules", ".agents", ".claude"}
MAX_FILES_PER_QUERY = 6   # 每次最多加载的文件数
MAX_FILE_CHARS = 30_000   # 单文件截断阈值


# ── 配置 ──────────────────────────────────────────────────────────────────────

def _load_config() -> dict[str, str]:
    env: dict[str, str] = {}
    env_file = PROJECT_ROOT / ".env"
    if env_file.exists():
        for line in env_file.read_text(encoding="utf-8").splitlines():
            line = line.strip()
            if not line or line.startswith("#") or "=" not in line:
                continue
            k, v = line.split("=", 1)
            env[k.strip()] = v.strip().strip('"').strip("'")

    def get(key: str, default: str = "") -> str:
        return os.environ.get(key, env.get(key, default))

    # 模型选择持久化（与 log-ai-detector 独立，互不影响）
    model_file = PROJECT_ROOT / ".doc-qa-model"
    default_model = get("LOG_AI_LLM_MODEL", get("DEEPSEEK_MODEL", "deepseek-v4-flash"))
    try:
        saved = model_file.read_text(encoding="utf-8").strip()
    except OSError:
        saved = ""

    return {
        "base_url": get("LOG_AI_LLM_BASE_URL", get("DEEPSEEK_BASE_URL", "https://api.deepseek.com")),
        "api_key":  get("LOG_AI_LLM_API_KEY",  get("DEEPSEEK_API_KEY")),
        "model":    saved or default_model,
        "models":   get("LOG_AI_AVAILABLE_MODELS", "deepseek-v4-flash,deepseek-v4-pro"),
        "timeout":  get("LOG_AI_LLM_TIMEOUT_SECONDS", "60"),
    }


# ── 文档索引 ──────────────────────────────────────────────────────────────────

def _collect_files() -> list[Path]:
    files = []
    for p in sorted(PROJECT_ROOT.rglob("*.md")):
        if any(part in EXCLUDE_DIRS for part in p.parts):
            continue
        files.append(p)
    return files


def _first_paragraph(text: str, max_chars: int = 200) -> str:
    """提取文件首段非空文本作为摘要。"""
    for line in text.splitlines():
        line = line.strip().lstrip("#").strip()
        if len(line) > 20:
            return line[:max_chars]
    return text[:max_chars].replace("\n", " ")


def build_index(files: list[Path]) -> str:
    """构建单行摘要索引，供 LLM 选文件用。"""
    lines = []
    for p in files:
        rel = str(p.relative_to(PROJECT_ROOT))
        try:
            content = p.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        summary = _first_paragraph(content)
        lines.append(f"{rel} | {summary}")
    return "\n".join(lines)


def load_files(paths: list[str]) -> str:
    """加载指定文件的完整内容，拼接为文档块。"""
    parts = []
    for rel in paths:
        p = PROJECT_ROOT / rel
        if not p.exists():
            continue
        try:
            content = p.read_text(encoding="utf-8", errors="replace").strip()
        except OSError:
            continue
        if len(content) > MAX_FILE_CHARS:
            content = content[:MAX_FILE_CHARS] + "\n\n[内容过长，已截断]"
        parts.append(f"## 文件：{rel}\n\n{content}")
    return "\n\n---\n\n".join(parts)


# ── LLM 调用 ──────────────────────────────────────────────────────────────────

def _chat(messages: list[dict], config: dict[str, str]) -> str:
    url = config["base_url"].rstrip("/") + "/chat/completions"
    req = urllib.request.Request(
        url,
        data=json.dumps({"model": config["model"], "messages": messages, "temperature": 0.2}).encode(),
        headers={"Authorization": f"Bearer {config['api_key']}", "Content-Type": "application/json"},
        method="POST",
    )
    try:
        with urllib.request.urlopen(req, timeout=int(config["timeout"])) as resp:
            return json.loads(resp.read())["choices"][0]["message"]["content"]
    except urllib.error.HTTPError as exc:
        raise RuntimeError(f"API 错误 {exc.code}: {exc.read().decode('utf-8', errors='replace')[:400]}") from exc


# ── 两阶段问答 ────────────────────────────────────────────────────────────────

SELECT_SYSTEM = """\
你是文档检索助手。给定文档索引（每行格式：文件路径 | 摘要），\
根据用户问题选出最相关的文件路径（最多 {max_files} 个）。
只输出 JSON 数组，例如：["README.md", "backend/Server/README.md"]
不要输出任何其他内容。"""

ANSWER_SYSTEM = """\
你是 HyperTicket 项目的架构讲解助手。根据以下项目文档回答问题。
要求：引用具体文件名和章节；结合代码路径和模块名；文档中没有的信息明确说明；使用中文；\
禁止使用任何表情符号（emoji）。
需要绘制架构图、流程图、时序图时，必须使用 mermaid 语法（```mermaid 代码块），\
禁止使用 ASCII 字符画。"""


def ask(
    question: str,
    index: str,
    files: list[Path],
    config: dict[str, str],
    history: list[dict],
) -> tuple[str, list[str]]:
    # 阶段一：选文件
    select_resp = _chat([
        {"role": "system", "content": SELECT_SYSTEM.format(max_files=MAX_FILES_PER_QUERY)},
        {"role": "user",   "content": f"文档索引：\n{index}\n\n问题：{question}"},
    ], config)

    try:
        # 提取 JSON 数组（模型可能在前后加文字）
        start = select_resp.index("[")
        end   = select_resp.rindex("]") + 1
        selected: list[str] = json.loads(select_resp[start:end])
    except (ValueError, json.JSONDecodeError):
        # 降级：加载前 3 个文件
        selected = [str(p.relative_to(PROJECT_ROOT)) for p in files[:3]]

    # 阶段二：生成回答
    docs = load_files(selected)
    messages = (
        [{"role": "system", "content": f"{ANSWER_SYSTEM}\n\n# 相关文档\n\n{docs}"}]
        + history
        + [{"role": "user", "content": question}]
    )
    answer = _chat(messages, config)
    history.append({"role": "user",      "content": question})
    history.append({"role": "assistant", "content": answer})
    return answer, selected


# ── HTTP 服务（Web UI）───────────────────────────────────────────────────────

UI_DIST = Path(__file__).resolve().parent / "doc-qa-ui" / "dist"


def serve(host: str, port: int, config: dict[str, str]) -> None:
    import mimetypes
    import threading
    from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
    from urllib.parse import urlparse

    files = _collect_files()
    index = build_index(files)
    file_list = [str(p.relative_to(PROJECT_ROOT)) for p in files]
    # 会话历史：按 session id 隔离（内存态，重启即清）
    sessions: dict[str, list[dict]] = {}
    lock = threading.Lock()

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, _fmt, *_args):  # noqa: N802
            pass

        def _json(self, code: int, body) -> None:
            data = json.dumps(body, ensure_ascii=False).encode()
            self.send_response(code)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            self.wfile.write(data)

        def _file(self, path: Path) -> None:
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

        def _spa(self) -> None:
            idx = UI_DIST / "index.html"
            if idx.exists():
                self._file(idx)
            else:
                self._json(503, {"error": "UI not built. Run: cd tools/doc-qa-ui && npm install && npm run build"})

        def do_GET(self) -> None:  # noqa: N802
            parsed = urlparse(self.path)
            path = parsed.path.rstrip("/") or "/"
            if path == "/doc-qa/api/status":
                self._json(200, {
                    "model": config["model"],
                    "models": [m.strip() for m in config["models"].split(",") if m.strip()],
                    "file_count": len(files),
                    "files": file_list,
                })
            elif path == "/doc-qa/api/file":
                # 查看单个文档：?path=README.md（仅允许索引内的文件，防止路径穿越）
                from urllib.parse import parse_qs
                rel = parse_qs(parsed.query).get("path", [""])[0]
                if rel not in file_list:
                    self._json(404, {"error": f"文档不在索引中: {rel}"})
                    return
                content = (PROJECT_ROOT / rel).read_text(encoding="utf-8", errors="replace")
                self._json(200, {"path": rel, "content": content})
            elif path.startswith("/doc-qa/api/"):
                # 未知 API 路径必须返回 JSON 404，绝不落入 SPA fallback（否则前端收到 HTML 报 JSON 解析错误）
                self._json(404, {"error": f"Not found: {path}"})
            elif path.startswith("/doc-qa/"):
                asset = UI_DIST / path[len("/doc-qa/"):]
                if asset.is_file():
                    self._file(asset)
                else:
                    self._spa()
            elif path in ("/doc-qa", "/"):
                self._spa()
            else:
                self._json(404, {"error": "Not found"})

        def do_POST(self) -> None:  # noqa: N802
            path = urlparse(self.path).path.rstrip("/")
            length = int(self.headers.get("Content-Length", 0))
            try:
                body = json.loads(self.rfile.read(length)) if length else {}
            except json.JSONDecodeError:
                self._json(400, {"error": "Invalid JSON"})
                return

            if path == "/doc-qa/api/ask":
                question = (body.get("question") or "").strip()
                session = body.get("session") or "default"
                if not question:
                    self._json(400, {"error": "question is required"})
                    return
                with lock:
                    history = sessions.setdefault(session, [])
                try:
                    answer, selected = ask(question, index, files, config, history)
                    self._json(200, {"answer": answer, "files": selected})
                except RuntimeError as exc:
                    self._json(502, {"error": str(exc)})

            elif path == "/doc-qa/api/clear":
                session = body.get("session") or "default"
                with lock:
                    sessions.pop(session, None)
                self._json(200, {"ok": True})

            elif path == "/doc-qa/api/model":
                model = str(body.get("model", "")).strip()
                allowed = [m.strip() for m in config["models"].split(",") if m.strip()]
                if model not in allowed:
                    self._json(400, {"error": f"未知模型: {model}，可选: {allowed}"})
                    return
                (PROJECT_ROOT / ".doc-qa-model").write_text(model, encoding="utf-8")
                config["model"] = model  # 热切换，后续请求即刻生效
                self._json(200, {"active": model})

            else:
                self._json(404, {"error": "Not found"})

    server = ThreadingHTTPServer((host, port), Handler)
    if not (UI_DIST / "index.html").exists():
        print("warning: UI not built — run: cd tools/doc-qa-ui && npm install && npm run build", flush=True)
    print(f"doc-qa  →  http://{host}:{port}/doc-qa/", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


# ── 入口 ──────────────────────────────────────────────────────────────────────

def run(question: str | None = None) -> None:
    config = _load_config()
    if not config["api_key"]:
        print("错误：未找到 API key。请在 .env 中设置 LOG_AI_LLM_API_KEY 或 DEEPSEEK_API_KEY。", file=sys.stderr)
        sys.exit(1)

    print("正在构建文档索引...", end=" ", flush=True)
    files = _collect_files()
    index = build_index(files)
    print(f"完成（{len(files)} 个文件，模型：{config['model']}）\n")

    history: list[dict] = []

    def ask_cli(q: str) -> str:
        answer, selected = ask(q, index, files, config, history)
        print(f"  [加载文件: {', '.join(selected)}]", flush=True)
        return answer

    if question:
        print(ask_cli(question))
        return

    print("HyperTicket 文档问答（exit 退出，clear 清除历史）\n")
    while True:
        try:
            q = input("问题> ").strip()
        except (EOFError, KeyboardInterrupt):
            print(); break
        if not q:
            continue
        if q.lower() in ("exit", "quit", "q", "退出"):
            break
        if q.lower() in ("clear", "清除"):
            history.clear(); print("历史已清除。\n"); continue
        try:
            print(f"\n{ask_cli(q)}\n")
        except RuntimeError as exc:
            print(f"错误：{exc}\n", file=sys.stderr)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="HyperTicket 文档问答机器人")
    parser.add_argument("--question", "-q", help="直接提问（非交互模式）")
    parser.add_argument("--serve", action="store_true", help="启动 Web UI 服务")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=7171)
    args = parser.parse_args()

    if args.serve:
        cfg = _load_config()
        if not cfg["api_key"]:
            print("错误：未找到 API key。", file=sys.stderr)
            sys.exit(1)
        serve(args.host, args.port, cfg)
    else:
        run(args.question)
