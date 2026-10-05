"""Tiny .env loader (no extra dependency). Looks in the repo root and server/."""
from __future__ import annotations

import os
from pathlib import Path


def load_env() -> None:
    root = Path(__file__).resolve().parents[2]
    for candidate in (root / ".env", root / "server" / ".env"):
        if not candidate.exists():
            continue
        for raw in candidate.read_text().splitlines():
            line = raw.strip()
            if not line or line.startswith("#") or "=" not in line:
                continue
            key, _, value = line.partition("=")
            key = key.strip().removeprefix("export ").strip()
            value = value.strip().strip('"').strip("'")
            if key and key not in os.environ:
                os.environ[key] = value
