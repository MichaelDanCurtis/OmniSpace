"""SQLite persistence for charted systems (the shared galactic record)."""
from __future__ import annotations

import json
import sqlite3
import threading
from pathlib import Path
from typing import Any, Optional

from .schema import StarSystem


class Store:
    def __init__(self, path: str | Path):
        self.path = str(path)
        Path(self.path).parent.mkdir(parents=True, exist_ok=True)
        self._lock = threading.Lock()
        self._conn = sqlite3.connect(self.path, check_same_thread=False)
        self._conn.execute(
            """CREATE TABLE IF NOT EXISTS systems (
                id TEXT PRIMARY KEY,
                name TEXT NOT NULL,
                source TEXT NOT NULL,
                model TEXT,
                discovered_by TEXT,
                created_at TEXT,
                faction TEXT,
                economy TEXT,
                stations INTEGER,
                body_count INTEGER,
                json TEXT NOT NULL
            )"""
        )
        self._conn.commit()

    def get(self, system_id: str) -> Optional[dict[str, Any]]:
        with self._lock:
            row = self._conn.execute("SELECT json FROM systems WHERE id=?", (system_id,)).fetchone()
        return json.loads(row[0]) if row else None

    def put(self, system: StarSystem, overwrite: bool = False) -> bool:
        data = system.model_dump(mode="json")
        verb = "INSERT OR REPLACE" if overwrite else "INSERT OR IGNORE"
        with self._lock:
            cur = self._conn.execute(
                f"""{verb} INTO systems (id, name, source, model, discovered_by, created_at, faction, economy, stations, body_count, json)
                    VALUES (?,?,?,?,?,?,?,?,?,?,?)""",
                (
                    system.id, system.name, system.source, system.model, system.discoveredBy, system.createdAt,
                    system.profile.faction, system.profile.economy, len(system.stations), len(system.bodies), json.dumps(data),
                ),
            )
            self._conn.commit()
            return cur.rowcount > 0

    def delete(self, system_id: str) -> None:
        with self._lock:
            self._conn.execute("DELETE FROM systems WHERE id=?", (system_id,))
            self._conn.commit()

    def summaries(self, ids: Optional[list[str]] = None) -> list[dict[str, Any]]:
        q = "SELECT id, name, source, model, discovered_by, created_at, faction, economy, stations, body_count FROM systems"
        args: tuple[Any, ...] = ()
        if ids:
            q += f" WHERE id IN ({','.join('?' * len(ids))})"
            args = tuple(ids)
        with self._lock:
            rows = self._conn.execute(q, args).fetchall()
        keys = ["id", "name", "source", "model", "discoveredBy", "createdAt", "faction", "economy", "stations", "bodies"]
        return [dict(zip(keys, r)) for r in rows]
