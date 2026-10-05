"""Background generation jobs. One job per system id; clients poll for progress."""
from __future__ import annotations

import asyncio
import logging
import os
import time
from dataclasses import dataclass, field
from typing import Any, Awaitable, Callable, Optional

from .draft import SystemDraft
from .forge import ForgeRuntime, forge_system, model_name
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
    prefetch: bool = False
    survey: bool = False  # no AI available: chart with the onboard survey (scripted forge) directly

    def public(self) -> dict[str, Any]:
        return {
            "status": self.status,
            "log": list(self.draft.log) if self.draft else [],
            "error": self.error,
            "elapsed": round((self.finished or time.time()) - self.started, 1),
        }


class JobManager:
    def __init__(self, store: Store, model_override: Any = None, max_concurrent: int = 3,
                 runtime_factory: Optional[Callable[[], Awaitable[ForgeRuntime]]] = None):
        self.store = store
        self.jobs: dict[str, Job] = {}
        self.model_override = model_override
        self.runtime_factory = runtime_factory
        self.sem = asyncio.Semaphore(max_concurrent)
        # prefetches (route plotted, not jumping yet) share a single lane so they never
        # starve the jump the pilot is actually waiting on
        self.prefetch_sem = asyncio.Semaphore(1)
        self.timeout = float(os.environ.get("OMNISPACE_TIMEOUT", "240"))

    def model_label(self) -> str:
        if self.model_override is not None:
            return getattr(self.model_override, "label", type(self.model_override).__name__)
        return model_name() or "sdk-default"

    def start(self, req: GenerateRequest, survey: bool = False) -> Job:
        sid = req.star.id
        job = self.jobs.get(sid)
        if job and job.status in ("generating", "ready"):
            if not req.prefetch:
                job.prefetch = False  # the pilot is now jumping: promote it
            return job
        draft = SystemDraft(star=req.star, neighbors=req.neighbors, commander=req.commander)
        draft.neighbor_summaries = self._neighbor_context(req)
        draft.log.append(f"Stellar Forge engaged for {req.star.name}")
        job = Job(system_id=sid, draft=draft)
        job.prefetch = req.prefetch
        job.survey = survey
        self.jobs[sid] = job
        job.task = asyncio.create_task(self._run(job))
        return job

    def cancel(self, system_id: str) -> None:
        job = self.jobs.get(system_id)
        if job and job.status == "generating" and job.task:
            job.task.cancel()
            job.status = "cancelled"
            job.finished = time.time()

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
        holding_lane = False
        try:
            # at most one prefetch runs at a time; a prefetch that gets promoted (the pilot
            # actually jumps) stops waiting for the lane immediately
            while job.prefetch:
                if not self.prefetch_sem.locked():
                    await self.prefetch_sem.acquire()
                    holding_lane = True
                    break
                await asyncio.sleep(0.3)
            async with self.sem:
                if job.survey:
                    raise RuntimeError("AI forge unavailable; charting with the onboard survey")
                if self.runtime_factory is not None:
                    runtime = await self.runtime_factory()
                else:
                    runtime = ForgeRuntime(model=self.model_override, label=self.model_label())
                job.draft.log.append(f"Hyperspace cartography uplink established · {runtime.label}")
                await forge_system(job.draft, timeout=self.timeout, runtime=runtime)
            system = job.draft.build(runtime.label)
            self.store.put(system)
            job.status = "ready"
        except Exception as e:  # noqa: BLE001 - report every failure to the client
            log.exception("forge failed for %s", job.system_id)
            job.error = f"{type(e).__name__}: {e}"[:500]
            job.draft.log.append("Forge link lost — falling back to onboard survey computer")
            # The game must stay playable without the AI: chart the system with the scripted
            # stand-in forge (deterministic, instant) and keep the error for the status line.
            try:
                from .scripted import ScriptedForgeModel

                survey = ForgeRuntime(model=ScriptedForgeModel(delay=0.05), label="onboard survey")
                old = job.draft
                fresh = SystemDraft(star=old.star, neighbors=old.neighbors, commander=old.commander)
                fresh.log.extend(old.log)  # keep the cartography log the pilot has already seen
                job.draft = fresh
                await forge_system(fresh, timeout=min(self.timeout, 60), runtime=survey)
                system = fresh.build("onboard survey", source="procedural")
                self.store.put(system)
                job.status = "ready"
                log.warning("charted %s with the onboard survey after the forge failed", job.system_id)
            except Exception as e2:  # noqa: BLE001
                log.exception("survey fallback failed for %s", job.system_id)
                job.status = "failed"
                job.error = f"{job.error} / survey: {type(e2).__name__}: {e2}"[:500]
        finally:
            if holding_lane:
                self.prefetch_sem.release()
            job.finished = job.finished or time.time()
