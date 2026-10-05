"""FastAPI app: the Stellar Forge API + static host for the built game client."""
from __future__ import annotations

import logging
import os
from contextlib import asynccontextmanager
from pathlib import Path
from typing import Any

from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles

from .config import load_env
from .jobs import JobManager
from .schema import GenerateRequest, StarSystem
from .store import Store

load_env()
logging.basicConfig(level=os.environ.get("OMNISPACE_LOG", "INFO"))
log = logging.getLogger("omnispace")

ROOT = Path(__file__).resolve().parents[2]
DIST = Path(os.environ.get("OMNISPACE_DIST", ROOT / "client" / "dist"))
DB_PATH = os.environ.get("OMNISPACE_DB", str(ROOT / "data" / "galaxy.sqlite3"))


def ai_status() -> dict[str, Any]:
    mode = os.environ.get("OMNISPACE_AI", "openai").lower()
    if mode == "off":
        return {"enabled": False, "reason": "AI disabled (OMNISPACE_AI=off)"}
    if mode == "scripted":
        return {"enabled": True, "model": "scripted-demo", "reason": "Scripted demo forge (no OpenAI calls)"}
    if not os.environ.get("OPENAI_API_KEY"):
        return {"enabled": False, "reason": "OPENAI_API_KEY not set — using the onboard procedural generator"}
    return {"enabled": True, "model": os.environ.get("OMNISPACE_MODEL") or "OpenAI Agents SDK default"}


def make_model_override() -> Any:
    if os.environ.get("OMNISPACE_AI", "").lower() == "scripted":
        from agents import set_tracing_disabled

        from .scripted import ScriptedForgeModel

        set_tracing_disabled(True)

        return ScriptedForgeModel()
    return None


@asynccontextmanager
async def lifespan(app: FastAPI):
    app.state.store = Store(DB_PATH)
    app.state.jobs = JobManager(app.state.store, model_override=make_model_override())
    log.info("OmniSpace Stellar Forge ready. AI: %s. DB: %s", ai_status(), DB_PATH)
    yield


app = FastAPI(title="OmniSpace Stellar Forge", lifespan=lifespan)
app.add_middleware(CORSMiddleware, allow_origins=["*"], allow_methods=["*"], allow_headers=["*"])


@app.get("/api/health")
def health() -> dict[str, Any]:
    return {"ok": True, "ai": ai_status()}


@app.get("/api/systems")
def list_systems() -> list[dict[str, Any]]:
    return app.state.store.summaries()


@app.get("/api/systems/{system_id}")
def get_system(system_id: str) -> dict[str, Any]:
    sys = app.state.store.get(system_id)
    if sys:
        return {"status": "ready", "system": sys}
    job = app.state.jobs.jobs.get(system_id)
    if job:
        return {**job.public(), "system": None}
    raise HTTPException(404, "uncharted")


@app.post("/api/systems/{system_id}/generate")
async def generate(system_id: str, req: GenerateRequest) -> dict[str, Any]:
    if req.star.id != system_id:
        raise HTTPException(400, "star id mismatch")
    existing = app.state.store.get(system_id)
    if existing:
        job = app.state.jobs.jobs.get(system_id)
        return {"status": "ready", "system": existing, "log": job.public()["log"] if job else []}
    status = ai_status()
    if not status["enabled"]:
        return {"status": "unavailable", "reason": status["reason"], "log": []}
    job = app.state.jobs.start(req)
    return {**job.public(), "system": None}


@app.get("/api/systems/{system_id}/job")
def job_status(system_id: str) -> dict[str, Any]:
    sys = app.state.store.get(system_id)
    job = app.state.jobs.jobs.get(system_id)
    if sys:
        return {"status": "ready", "system": sys, "log": job.public()["log"] if job else []}
    if not job:
        raise HTTPException(404, "no job")
    return {**job.public(), "system": None}


@app.put("/api/systems/{system_id}")
def lock_in(system_id: str, system: StarSystem) -> dict[str, Any]:
    """Persist a client-generated (procedural) system so later visits stay consistent."""
    if system.id != system_id:
        raise HTTPException(400, "id mismatch")
    job = app.state.jobs.jobs.get(system_id)
    if job and job.status == "generating":
        return {"stored": False, "reason": "AI generation in progress"}
    stored = app.state.store.put(system)
    return {"stored": stored}


@app.delete("/api/systems/{system_id}")
def forget(system_id: str) -> dict[str, Any]:
    """Dev helper: forget a system so it is regenerated on the next visit."""
    app.state.store.delete(system_id)
    app.state.jobs.jobs.pop(system_id, None)
    return {"ok": True}


if DIST.exists():
    app.mount("/assets", StaticFiles(directory=DIST / "assets"), name="assets")

    @app.get("/{path:path}")
    def spa(path: str) -> FileResponse:
        f = DIST / path
        if path and f.is_file():
            return FileResponse(f)
        return FileResponse(DIST / "index.html")
