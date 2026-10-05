"""FastAPI app: the Stellar Forge API + static host for the built game client."""
from __future__ import annotations

import logging
import os
from contextlib import asynccontextmanager
from pathlib import Path
from typing import Any

from fastapi import FastAPI, HTTPException, Request
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import FileResponse, HTMLResponse
from fastapi.staticfiles import StaticFiles

from .chatgpt_auth import RESOURCE, ChatGPTAuth
from .config import load_env
from .forge import ForgeRuntime, model_name
from .jobs import JobManager
from .schema import GenerateRequest, StarSystem
from .store import Store

load_env()
logging.basicConfig(level=os.environ.get("OMNISPACE_LOG", "INFO"))
log = logging.getLogger("omnispace")

ROOT = Path(__file__).resolve().parents[2]
DIST = Path(os.environ.get("OMNISPACE_DIST", ROOT / "client" / "dist"))
DB_PATH = os.environ.get("OMNISPACE_DB", str(ROOT / "data" / "galaxy.sqlite3"))
AUTH_PATH = os.environ.get("OMNISPACE_AUTH_FILE", str(ROOT / "data" / "chatgpt_auth.json"))
auth = ChatGPTAuth(AUTH_PATH)


def auth_mode() -> str:
    """'chatgpt' or 'api_key' — which credentials the forge should use."""
    pref = os.environ.get("OMNISPACE_AUTH", "auto").lower()
    if pref == "chatgpt":
        return "chatgpt"
    if pref in ("api_key", "apikey", "key"):
        return "api_key"
    if os.environ.get("OPENAI_API_KEY"):
        return "api_key"
    return "chatgpt"


def ai_status() -> dict[str, Any]:
    mode = os.environ.get("OMNISPACE_AI", "openai").lower()
    base: dict[str, Any] = {"signed_in": auth.signed_in, "account": auth.account(), "can_sign_in": True}
    if mode == "off":
        return {**base, "enabled": False, "reason": "AI disabled (OMNISPACE_AI=off)"}
    if mode == "scripted":
        return {**base, "enabled": True, "auth": "scripted", "model": "scripted-demo", "reason": "Scripted demo forge (no OpenAI calls)"}
    if auth_mode() == "chatgpt":
        if auth.signed_in:
            return {**base, "enabled": True, "auth": "chatgpt", "model": auth.preferred_model() or "auto", "sharing": auth.sharing}
        return {**base, "enabled": False, "reason": "Sign in with ChatGPT (or set OPENAI_API_KEY) to let the AI forge new systems"}
    return {**base, "enabled": True, "auth": "api_key", "model": model_name() or "OpenAI Agents SDK default"}


async def runtime_factory() -> ForgeRuntime:
    if auth_mode() == "chatgpt":
        from agents import set_tracing_disabled
        from openai import AsyncOpenAI

        from .forge import ChatGPTPlanModel

        set_tracing_disabled(not os.environ.get("OPENAI_API_KEY"))
        token = await auth.access_token()
        slug = await auth.pick_model()
        client = AsyncOpenAI(api_key=token, base_url=RESOURCE)
        return ForgeRuntime(model=ChatGPTPlanModel(model=slug, openai_client=client), label=f"{slug} · ChatGPT plan",
                            streamed=True, stateless=True, namespace=True)
    return ForgeRuntime(model=model_name(), label=model_name() or "sdk-default")


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
    override = make_model_override()
    app.state.jobs = JobManager(app.state.store, model_override=override, runtime_factory=None if override is not None else runtime_factory)
    log.info("OmniSpace Stellar Forge ready. AI: %s. DB: %s", ai_status(), DB_PATH)
    yield


app = FastAPI(title="OmniSpace Stellar Forge", lifespan=lifespan)
# Only the Vite dev server needs cross-origin access; the game itself is same-origin.
app.add_middleware(CORSMiddleware, allow_origins=["http://localhost:5173", "http://127.0.0.1:5173"], allow_methods=["*"], allow_headers=["*"])


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
    # No AI (not signed in / no key): the server still charts the system with the onboard
    # survey so every client, including the Unreal build, always gets a system back.
    job = app.state.jobs.start(req, survey=not status["enabled"])
    return {**job.public(), "system": None, "reason": None if status["enabled"] else status["reason"]}


@app.get("/api/systems/{system_id}/job")
def job_status(system_id: str) -> dict[str, Any]:
    sys = app.state.store.get(system_id)
    job = app.state.jobs.jobs.get(system_id)
    if sys:
        return {"status": "ready", "system": sys, "log": job.public()["log"] if job else [], "error": job.error if job else None}
    if not job:
        raise HTTPException(404, "no job")
    return {**job.public(), "system": None}


@app.put("/api/systems/{system_id}")
def lock_in(system_id: str, system: StarSystem) -> dict[str, Any]:
    """Persist a client-generated (procedural) system so later visits stay consistent."""
    if system.id != system_id:
        raise HTTPException(400, "id mismatch")
    # The client gave up waiting and committed its procedural version: that version wins,
    # so cancel any generation still running for this system.
    app.state.jobs.cancel(system_id)
    stored = app.state.store.put(system)
    return {"stored": stored}


@app.delete("/api/systems/{system_id}")
def forget(system_id: str) -> dict[str, Any]:
    """Dev helper: forget a system so it is regenerated on the next visit."""
    app.state.store.delete(system_id)
    app.state.jobs.jobs.pop(system_id, None)
    return {"ok": True}


# ------------------------------------------------------------ Sign in with ChatGPT
@app.get("/api/auth/chatgpt/start")
async def chatgpt_start(request: Request) -> dict[str, Any]:
    server = request.scope.get("server") or ("127.0.0.1", 8000)
    port = int(os.environ.get("OMNISPACE_PORT") or server[1] or 8000)
    try:
        url = await auth.start(f"http://127.0.0.1:{port}/auth/callback")
    except Exception as e:  # noqa: BLE001
        raise HTTPException(502, f"Could not reach ChatGPT sign-in: {e}") from e
    return {"url": url}


@app.get("/auth/callback", response_class=HTMLResponse)
async def chatgpt_callback(state: str = "", code: str | None = None, client_id: str | None = None, error: str | None = None) -> HTMLResponse:
    try:
        await auth.finish(state, code, client_id, error)
        msg = f"Signed in{' as ' + auth.account() if auth.account() else ''}. The Stellar Forge will now use your ChatGPT plan."
        if not auth.sharing:
            msg += " (Plan usage was not granted — enable it in ChatGPT settings, then sign in again.)"
        ok = True
    except Exception as e:  # noqa: BLE001
        msg, ok = str(e), False
    color = "#6cffb0" if ok else "#ff8a7a"
    return HTMLResponse(
        f"""<!doctype html><meta charset="utf-8"><title>OmniSpace</title>
<body style="background:#03070d;color:#dceaf8;font:17px system-ui;display:grid;place-items:center;height:100vh;margin:0">
<div style="max-width:32rem;text-align:center"><h1 style="letter-spacing:.3em;font-weight:500">OMNISPACE</h1>
<p style="color:{color}">{msg}</p><p style="color:#8fa6bd">You can close this tab and return to the game.</p></div>""",
        headers={"Cache-Control": "no-store", "Referrer-Policy": "no-referrer"},
    )


@app.post("/api/auth/chatgpt/logout")
def chatgpt_logout() -> dict[str, Any]:
    auth.logout()
    return {"ok": True}


@app.get("/api/auth/chatgpt/models")
async def chatgpt_models() -> dict[str, Any]:
    try:
        return {"models": await auth.models(), "selected": auth.preferred_model()}
    except Exception as e:  # noqa: BLE001
        raise HTTPException(502, str(e)) from e


@app.post("/api/auth/chatgpt/model")
def chatgpt_set_model(body: dict[str, str]) -> dict[str, Any]:
    slug = (body.get("slug") or "").strip()
    auth.set_model(slug or None)
    return {"ok": True, "model": slug or None}


if DIST.exists():
    app.mount("/assets", StaticFiles(directory=DIST / "assets"), name="assets")

    DIST_ROOT = DIST.resolve()

    @app.get("/{path:path}")
    def spa(path: str) -> FileResponse:
        f = (DIST / path).resolve()
        if path and f.is_file() and f.is_relative_to(DIST_ROOT):
            return FileResponse(f)
        return FileResponse(DIST_ROOT / "index.html")
