"""Configuration loading for the log AI detector.

The detector reads the project .env file directly so secrets can stay outside git.
Only values prefixed with LOG_AI_ are used by this tool.
"""

from __future__ import annotations

import os
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable


DEFAULT_PROJECT_ROOT = Path(__file__).resolve().parents[2]
SECRET_KEYS = ("KEY", "PASSWORD", "TOKEN", "SECRET", "WEBHOOK_URL")


def _parse_env_file(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    if not path.exists():
        return values

    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        key = key.strip()
        value = value.strip().strip('"').strip("'")
        values[key] = value
    return values


def _get_bool(value: str | None, default: bool = False) -> bool:
    if value is None or value == "":
        return default
    return value.strip().lower() in {"1", "true", "yes", "on"}


def _split_csv(value: str | None, default: Iterable[str]) -> list[str]:
    if not value:
        return list(default)
    return [item.strip() for item in value.split(",") if item.strip()]


def _selected_model(path: Path, fallback: str) -> str:
    try:
        saved = path.read_text(encoding="utf-8").strip()
    except OSError:
        return fallback
    return saved or fallback


@dataclass(frozen=True)
class Settings:
    project_root: Path = DEFAULT_PROJECT_ROOT
    log_dir: Path = DEFAULT_PROJECT_ROOT / "logs"
    report_dir: Path = DEFAULT_PROJECT_ROOT / "reports" / "log-ai-detector"
    state_file: Path = DEFAULT_PROJECT_ROOT / ".log-ai-detector-state.json"

    llm_base_url: str = "https://api.deepseek.com"
    llm_api_key: str = ""
    llm_model: str = "deepseek-v4-flash"
    llm_api_style: str = "openai-compatible"
    available_models: tuple[str, ...] = ("deepseek-v4-flash", "deepseek-v4-pro")
    selected_model_file: Path = DEFAULT_PROJECT_ROOT / ".log-ai-detector-model"
    llm_timeout_seconds: int = 60
    llm_max_input_chars: int = 120_000

    levels: list[str] = field(default_factory=lambda: ["ERROR", "FATAL"])
    context_before_lines: int = 80
    context_after_lines: int = 40
    debounce_seconds: int = 30
    max_log_bytes: int = 1_048_576
    poll_interval_seconds: float = 1.5

    codegraph_bin: str = "codegraph"
    codegraph_project: Path = DEFAULT_PROJECT_ROOT
    codegraph_index_dir: Path = DEFAULT_PROJECT_ROOT / ".codegraph"
    codegraph_index_cmd: str = ""
    codegraph_search_cmd: str = ""
    codegraph_update_cmd: str = ""
    code_search_max_hits: int = 8

    notify_channels: list[str] = field(default_factory=list)
    webhook_url: str = ""
    webhook_timeout_seconds: int = 10

    smtp_host: str = ""
    smtp_port: int = 587
    smtp_username: str = ""
    smtp_password: str = ""
    smtp_from: str = ""
    smtp_to: str = ""
    smtp_use_tls: bool = True

    daemon_log_file: Path = DEFAULT_PROJECT_ROOT / "reports" / "log-ai-detector" / "daemon.log"
    pid_file: Path = DEFAULT_PROJECT_ROOT / ".log-ai-detector.pid"
    api_token: str = ""

    @classmethod
    def load(cls, project_root: Path | None = None) -> "Settings":
        root = (project_root or DEFAULT_PROJECT_ROOT).resolve()
        env_values = _parse_env_file(root / ".env")

        def get(name: str, default: str = "") -> str:
            return os.environ.get(name, env_values.get(name, default))

        def get_int(name: str, default: int) -> int:
            raw = get(name, "")
            if raw == "":
                return default
            return int(raw)

        def get_float(name: str, default: float) -> float:
            raw = get(name, "")
            if raw == "":
                return default
            return float(raw)

        configured_root = Path(get("LOG_AI_PROJECT_ROOT", str(root))).expanduser().resolve()
        return cls(
            project_root=configured_root,
            log_dir=Path(get("LOG_AI_LOG_DIR", str(configured_root / "logs"))).expanduser().resolve(),
            report_dir=Path(get("LOG_AI_REPORT_DIR", str(configured_root / "reports" / "log-ai-detector"))).expanduser().resolve(),
            state_file=Path(get("LOG_AI_STATE_FILE", str(configured_root / ".log-ai-detector-state.json"))).expanduser().resolve(),
            llm_base_url=get("LOG_AI_LLM_BASE_URL", get("DEEPSEEK_BASE_URL", "https://api.deepseek.com")),
            llm_api_key=get("LOG_AI_LLM_API_KEY", get("DEEPSEEK_API_KEY")),
            llm_model=_selected_model(
                Path(get("LOG_AI_SELECTED_MODEL_FILE", str(configured_root / ".log-ai-detector-model"))).expanduser().resolve(),
                get("LOG_AI_LLM_MODEL", get("DEEPSEEK_MODEL", "deepseek-v4-flash")),
            ),
            llm_api_style=get("LOG_AI_LLM_API_STYLE", "openai-compatible"),
            available_models=tuple(_split_csv(get("LOG_AI_AVAILABLE_MODELS", "deepseek-v4-flash,deepseek-v4-pro"), ["deepseek-v4-flash", "deepseek-v4-pro"])),
            selected_model_file=Path(get("LOG_AI_SELECTED_MODEL_FILE", str(configured_root / ".log-ai-detector-model"))).expanduser().resolve(),
            llm_timeout_seconds=get_int("LOG_AI_LLM_TIMEOUT_SECONDS", 60),
            llm_max_input_chars=get_int("LOG_AI_LLM_MAX_INPUT_CHARS", 120_000),
            levels=_split_csv(get("LOG_AI_LEVELS", "ERROR,FATAL"), ["ERROR", "FATAL"]),
            context_before_lines=get_int("LOG_AI_CONTEXT_BEFORE_LINES", 80),
            context_after_lines=get_int("LOG_AI_CONTEXT_AFTER_LINES", 40),
            debounce_seconds=get_int("LOG_AI_DEBOUNCE_SECONDS", 30),
            max_log_bytes=get_int("LOG_AI_MAX_LOG_BYTES", 1_048_576),
            poll_interval_seconds=get_float("LOG_AI_POLL_INTERVAL_SECONDS", 1.5),
            codegraph_bin=get("LOG_AI_CODEGRAPH_BIN", "codegraph"),
            codegraph_project=Path(get("LOG_AI_CODEGRAPH_PROJECT", str(configured_root))).expanduser().resolve(),
            codegraph_index_dir=Path(get("LOG_AI_CODEGRAPH_INDEX_DIR", str(configured_root / ".codegraph"))).expanduser().resolve(),
            codegraph_index_cmd=get("LOG_AI_CODEGRAPH_INDEX_CMD"),
            codegraph_search_cmd=get("LOG_AI_CODEGRAPH_SEARCH_CMD"),
            codegraph_update_cmd=get("LOG_AI_CODEGRAPH_UPDATE_CMD"),
            code_search_max_hits=get_int("LOG_AI_CODE_SEARCH_MAX_HITS", 8),
            notify_channels=_split_csv(get("LOG_AI_NOTIFY_CHANNELS", ""), []),
            webhook_url=get("LOG_AI_WEBHOOK_URL"),
            webhook_timeout_seconds=get_int("LOG_AI_WEBHOOK_TIMEOUT_SECONDS", 10),
            smtp_host=get("LOG_AI_SMTP_HOST"),
            smtp_port=get_int("LOG_AI_SMTP_PORT", 587),
            smtp_username=get("LOG_AI_SMTP_USERNAME"),
            smtp_password=get("LOG_AI_SMTP_PASSWORD"),
            smtp_from=get("LOG_AI_SMTP_FROM"),
            smtp_to=get("LOG_AI_SMTP_TO"),
            smtp_use_tls=_get_bool(get("LOG_AI_SMTP_USE_TLS", "true"), True),
            daemon_log_file=Path(get("LOG_AI_DAEMON_LOG_FILE", str(configured_root / "reports" / "log-ai-detector" / "daemon.log"))).expanduser().resolve(),
            pid_file=Path(get("LOG_AI_PID_FILE", str(configured_root / ".log-ai-detector.pid"))).expanduser().resolve(),
            api_token=get("LOG_AI_API_TOKEN"),
        )

    def validate(self, require_llm: bool = True) -> list[str]:
        errors: list[str] = []
        if not self.project_root.exists():
            errors.append(f"LOG_AI_PROJECT_ROOT does not exist: {self.project_root}")
        if not self.log_dir.exists():
            errors.append(f"LOG_AI_LOG_DIR does not exist: {self.log_dir}")
        if require_llm:
            if not self.llm_base_url:
                errors.append("LOG_AI_LLM_BASE_URL is required")
            if not self.llm_api_key:
                errors.append("LOG_AI_LLM_API_KEY is required")
            if not self.llm_model:
                errors.append("LOG_AI_LLM_MODEL is required")
        if self.llm_api_style != "openai-compatible":
            errors.append(f"Unsupported LOG_AI_LLM_API_STYLE: {self.llm_api_style}")
        return errors

    def sanitized_summary(self) -> dict[str, str]:
        data = {
            "project_root": str(self.project_root),
            "log_dir": str(self.log_dir),
            "report_dir": str(self.report_dir),
            "llm_base_url": self.llm_base_url,
            "llm_api_key": self.llm_api_key,
            "llm_model": self.llm_model,
            "levels": ",".join(self.levels),
            "codegraph_bin": self.codegraph_bin,
            "codegraph_index_dir": str(self.codegraph_index_dir),
            "notify_channels": ",".join(self.notify_channels),
            "webhook_url": self.webhook_url,
            "smtp_host": self.smtp_host,
            "smtp_username": self.smtp_username,
            "smtp_password": self.smtp_password,
        }
        return {key: mask_secret(key, value) for key, value in data.items()}


def mask_secret(key: str, value: str) -> str:
    if not value:
        return ""
    upper_key = key.upper()
    if any(token in upper_key for token in SECRET_KEYS):
        if len(value) <= 8:
            return "***"
        return value[:4] + "***" + value[-4:]
    return value
