"""Notification backends for generated analysis reports."""

from __future__ import annotations

import json
import smtplib
import urllib.error
import urllib.request
from email.message import EmailMessage

from .config import Settings, mask_secret
from .reporter import AnalysisReport


class Notifier:
    def send(self, report: AnalysisReport) -> None:  # pragma: no cover - interface
        raise NotImplementedError


class NoopNotifier(Notifier):
    def send(self, report: AnalysisReport) -> None:
        return None


class WebhookNotifier(Notifier):
    def __init__(self, settings: Settings):
        self.settings = settings

    def send(self, report: AnalysisReport) -> None:
        payload = {
            "project": "HyperTicket",
            "level": report.event.level,
            "summary": report.summary,
            "log_file": report.event.log_file,
            "report_file": str(report.path),
            "fingerprint": report.fingerprint,
            "created_at": report.created_at,
        }
        request = urllib.request.Request(
            self.settings.webhook_url,
            data=json.dumps(payload, ensure_ascii=False).encode("utf-8"),
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        try:
            with urllib.request.urlopen(request, timeout=self.settings.webhook_timeout_seconds) as response:
                response.read()
        except urllib.error.URLError as exc:
            safe_url = mask_secret("webhook_url", self.settings.webhook_url)
            raise RuntimeError(f"Webhook notification failed for {safe_url}: {exc}") from exc


class EmailNotifier(Notifier):
    def __init__(self, settings: Settings):
        self.settings = settings

    def send(self, report: AnalysisReport) -> None:
        if not (self.settings.smtp_host and self.settings.smtp_from and self.settings.smtp_to):
            raise RuntimeError("SMTP notification is enabled but SMTP host/from/to are incomplete")

        msg = EmailMessage()
        msg["Subject"] = f"[HyperTicket][{report.event.level}] 日志错误分析报告 - {report.summary}"
        msg["From"] = self.settings.smtp_from
        msg["To"] = self.settings.smtp_to
        msg.set_content(
            f"报告路径: {report.path}\n"
            f"日志文件: {report.event.log_file}\n"
            f"触发日志: {report.event.raw}\n"
            f"fingerprint: {report.fingerprint}\n\n"
            f"摘要: {report.summary}\n\n"
            f"在 Web 界面查看完整报告：\n"
            f"http://127.0.0.1:7070/log-ai-detector/\n"
        )
        # 465 端口是隐式 SSL（QQ/163 等），587 是 STARTTLS，二者协议不同不能混用。
        if self.settings.smtp_port == 465:
            with smtplib.SMTP_SSL(self.settings.smtp_host, self.settings.smtp_port, timeout=15) as smtp:
                if self.settings.smtp_username or self.settings.smtp_password:
                    smtp.login(self.settings.smtp_username, self.settings.smtp_password)
                smtp.send_message(msg)
        else:
            with smtplib.SMTP(self.settings.smtp_host, self.settings.smtp_port, timeout=15) as smtp:
                if self.settings.smtp_use_tls:
                    smtp.starttls()
                if self.settings.smtp_username or self.settings.smtp_password:
                    smtp.login(self.settings.smtp_username, self.settings.smtp_password)
                smtp.send_message(msg)


class CompositeNotifier(Notifier):
    def __init__(self, notifiers: list[Notifier]):
        self.notifiers = notifiers

    def send(self, report: AnalysisReport) -> None:
        errors: list[str] = []
        for notifier in self.notifiers:
            try:
                notifier.send(report)
            except Exception as exc:  # notification must not hide generated report
                errors.append(str(exc))
        if errors:
            raise RuntimeError("; ".join(errors))


def build_notifier(settings: Settings) -> Notifier:
    notifiers: list[Notifier] = []
    channels = {channel.lower() for channel in settings.notify_channels}
    if "webhook" in channels and settings.webhook_url:
        notifiers.append(WebhookNotifier(settings))
    if "email" in channels:
        notifiers.append(EmailNotifier(settings))
    if not notifiers:
        return NoopNotifier()
    return CompositeNotifier(notifiers)
