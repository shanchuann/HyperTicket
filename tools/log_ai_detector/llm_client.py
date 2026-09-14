"""Provider-neutral OpenAI-compatible LLM client."""

from __future__ import annotations

import json
import urllib.error
import urllib.request
from dataclasses import dataclass

from .config import Settings


@dataclass(frozen=True)
class LlmResult:
    content: str
    raw: dict


class LlmClient:
    def __init__(self, settings: Settings):
        self.settings = settings

    def chat(self, system_prompt: str, user_prompt: str) -> LlmResult:
        if self.settings.llm_api_style != "openai-compatible":
            raise ValueError(f"Unsupported LLM API style: {self.settings.llm_api_style}")
        if not self.settings.llm_base_url or not self.settings.llm_api_key or not self.settings.llm_model:
            raise ValueError("LLM configuration is incomplete; set LOG_AI_LLM_BASE_URL/API_KEY/MODEL")

        url = self.settings.llm_base_url.rstrip("/") + "/chat/completions"
        body = {
            "model": self.settings.llm_model,
            "messages": [
                {"role": "system", "content": system_prompt},
                {"role": "user", "content": user_prompt},
            ],
            "temperature": 0.2,
        }
        request = urllib.request.Request(
            url,
            data=json.dumps(body).encode("utf-8"),
            headers={
                "Authorization": f"Bearer {self.settings.llm_api_key}",
                "Content-Type": "application/json",
            },
            method="POST",
        )
        try:
            with urllib.request.urlopen(request, timeout=self.settings.llm_timeout_seconds) as response:
                payload = json.loads(response.read().decode("utf-8"))
        except urllib.error.HTTPError as exc:
            error_body = exc.read().decode("utf-8", errors="replace")
            raise RuntimeError(f"LLM HTTP {exc.code}: {error_body[:500]}") from exc
        except urllib.error.URLError as exc:
            raise RuntimeError(f"LLM request failed: {exc}") from exc

        try:
            content = payload["choices"][0]["message"]["content"]
        except (KeyError, IndexError, TypeError) as exc:
            raise RuntimeError(f"Unexpected LLM response schema: {payload}") from exc
        return LlmResult(content=content, raw=payload)
