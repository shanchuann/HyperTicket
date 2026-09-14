"""Helpers for updating codegraph after feat commits."""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

from .code_index import CodeIndex
from .config import Settings

FEAT_COMMIT_RE = re.compile(r"^feat(\(.+\))?!?:\s+")


def _git(args: list[str], cwd: Path) -> str:
    completed = subprocess.run(
        ["git", *args], cwd=cwd, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False
    )
    if completed.returncode != 0:
        raise RuntimeError(completed.stderr.strip() or completed.stdout.strip())
    return completed.stdout


def is_feat_commit(subject: str) -> bool:
    return bool(FEAT_COMMIT_RE.match(subject.strip()))


def update_for_commit(settings: Settings, commit: str) -> int:
    subject = _git(["log", "-1", "--pretty=%s", commit], settings.project_root).strip()
    if not is_feat_commit(subject):
        print(f"skip codegraph update: latest commit is not feat: {subject}")
        return 0

    diff_range = f"{commit}~1..{commit}"
    diff_text = _git(["diff", diff_range], settings.project_root)
    changed_files = _git(["diff", "--name-only", diff_range], settings.project_root).splitlines()
    result = CodeIndex(settings).update_from_diff(diff_text, changed_files)
    if result.text:
        print(result.text)
    return 0 if result.available else 2


def install_post_commit_hook(project_root: Path) -> Path:
    hook_path = project_root / ".git" / "hooks" / "post-commit"
    hook_path.parent.mkdir(parents=True, exist_ok=True)
    marker = "# HyperTicket log-ai-detector codegraph hook"
    snippet = f"""{marker}
if [ -x \"$PWD/scripts/update-code-index-on-feat-commit\" ]; then
    \"$PWD/scripts/update-code-index-on-feat-commit\" --commit HEAD || \
        echo \"warning: log-ai-detector codegraph update failed\" >&2
fi
"""
    existing = hook_path.read_text(encoding="utf-8") if hook_path.exists() else "#!/bin/sh\n"
    if marker not in existing:
        if not existing.startswith("#!"):
            existing = "#!/bin/sh\n" + existing
        hook_path.write_text(existing.rstrip() + "\n\n" + snippet, encoding="utf-8")
    hook_path.chmod(0o755)
    return hook_path


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Update codegraph after feat commits")
    parser.add_argument("--commit", default="HEAD", help="commit to inspect, default HEAD")
    parser.add_argument("--full", action="store_true", help="force a full codegraph index/update")
    parser.add_argument("--install-hook", action="store_true", help="install .git/hooks/post-commit integration")
    parser.add_argument("--project-root", default=None)
    args = parser.parse_args(argv)

    settings = Settings.load(Path(args.project_root).resolve() if args.project_root else None)
    if args.install_hook:
        path = install_post_commit_hook(settings.project_root)
        print(f"installed post-commit hook: {path}")
        return 0
    if args.full:
        result = CodeIndex(settings).ensure_index()
        if result.text:
            print(result.text)
        return 0 if result.available else 2
    return update_for_commit(settings, args.commit)


if __name__ == "__main__":
    sys.exit(main())
