"""Background generation jobs. One job per system id; clients poll for progress."""
from __future__ import annotations

import asyncio
import logging
import os
import time
from dataclasses import dataclass, field
from typing import Any, Optional

from .draft import SystemDraft
from .forge import forge_system, model_name
from .schema import GenerateRequest
from .store import Store

log = logging.getLogger("omnispace.jobs")


@dataclass
class Job:
    system_id: str
    status: str = "generating"  # generating | ready | failed
    draft: Optional[SystemDraft] = None
    error: Optional[str] = None
    started: float = field(default_factory=time.time)
    finished: Optional[float] = None
    task: Optional[asyncio.Task[Any]] = None

    def public(self) -> dict[str, Any]:
        return {
            "status": self.status,
            "log": list(self.draft.log) if self.draft else [],
            "error": self.error,
            "elapsed": round((self.finished or time.time()) - self.started, 1),
        }


class JobManager:
    def __init__(self, store: Store, model_override: Any = None, max_concurrent: int = 3):
        self.store = store
        self.jobs: dict[str, Job] = {}
        self.model_override = model_override
        self.sem = asyncio.Semaphore(max_concurrent)
        self.timeout = float(os.environ.get("OMNISPACE_TIMEOUT", "240"))

    def model_label(self) -> str:
        if self.model_override is not None:
            return getattr(self.model_override, "label", type(self.model_override).__name__)
        return model_name() or "sdk-default"

    def start(self, req: GenerateRequest) -> Job:
        sid = req.star.id
        job = self.jobs.get(sid)
        if job and job.status in ("generating", "ready"):
            return job
        draft = SystemDraft(star=req.star, neighbors=req.neighbors, commander=req.commander)
        draft.neighbor_summaries = self._neighbor_context(req)
        draft.log.append(f"Stellar Forge engaged for {req.star.name}")
        job = Job(system_id=sid, draft=draft)
        self.jobs[sid] = job
        job.task = asyncio.create_task(self._run(job))
        return job

    def _neighbor_context(self, req: GenerateRequest) -> list[dict[str, Any]]:
        ids = [n.id for n in req.neighbors[:12]]
        charted = {s["id"]: s for s in self.store.summaries(ids)} if ids else {}
        out = []
        for n in req.neighbors[:12]:
            entry: dict[str, Any] = {"name": n.name, "distance_ly": round(n.distanceLy, 1), "star_class": n.cls, "charted": n.id in charted}
            if n.id in charted:
                full = self.store.get(n.id) or {}
                prof = full.get("profile", {})
                entry.update({
                    "faction": prof.get("faction"), "economy": prof.get("economy"),
                    "summary": prof.get("description"), "lore": (prof.get("lore") or "")[:300],
                    "stations": [s.get("name") for s in full.get("stations", [])][:4],
                })
            out.append(entry)
        return out

    async def _run(self, job: Job) -> None:
        assert job.draft is not None
        try:
            async with self.sem:
                job.draft.log.append("Hyperspace cartography uplink established")
                await forge_system(job.draft, model=self.model_override, timeout=self.timeout)
            system = job.draft.build(self.model_label())
            self.store.put(system)
            job.status = "ready"
        except Exception as e:  # noqa: BLE001 - report every failure to the client
            log.exception("forge failed for %s", job.system_id)
            job.status = "failed"
            job.error = f"{type(e).__name__}: {e}"[:500]
            job.draft.log.append("Forge link lost — falling back to onboard survey computer")
        finally:
            job.finished = time.time()
