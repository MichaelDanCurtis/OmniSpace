"""Sign in with ChatGPT: run the Stellar Forge on the player's ChatGPT plan.

OpenAI's "Sign in with ChatGPT" lets locally-run open-source / personal apps
use an eligible ChatGPT plan (Plus / Pro) instead of an API key:

  1. OAuth 2.0 authorization-code + PKCE against https://auth.openai.com with
     dynamic client registration (client_id=dynamic_agent_client) and a
     loopback redirect to this server (http://127.0.0.1:<port>/auth/callback).
  2. The resulting access token is a Bearer token for the public Responses
     API (https://api.openai.com/v1/responses) with store=false, stream=true.

This is an independent Python implementation of the documented protocol
(https://developers.openai.com/siwc). Credentials are stored locally in
data/chatgpt_auth.json (mode 600) and refreshed automatically.
"""
from __future__ import annotations

import asyncio
import base64
import hashlib
import json
import logging
import os
import secrets
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Optional
from urllib.parse import urlencode

import httpx

log = logging.getLogger("omnispace.chatgpt")

ISSUER = "https://auth.openai.com"
RESOURCE = "https://api.openai.com/v1"
SCOPES = "openid profile email offline_access resource.invoke chatgpt.tokens.use.direct"
SHARING_SCOPE = "chatgpt.tokens.use.direct"
APP_NAME = "OmniSpace"


def _b64url(raw: bytes) -> str:
    return base64.urlsafe_b64encode(raw).rstrip(b"=").decode()


def _jwt_claims(token: str) -> dict[str, Any]:
    """Decode (without verifying) the ID token payload for display purposes only.
    The token came straight from the TLS token endpoint, and it is never used
    for authorization decisions here."""
    try:
        payload = token.split(".")[1]
        payload += "=" * (-len(payload) % 4)
        return json.loads(base64.urlsafe_b64decode(payload))
    except Exception:  # noqa: BLE001
        return {}


@dataclass
class Pending:
    verifier: str
    nonce: str
    redirect_uri: str
    created: float = field(default_factory=time.time)


class ChatGPTAuth:
    def __init__(self, path: str | Path, transport: Optional[httpx.AsyncBaseTransport] = None):
        self.path = Path(path)
        self.transport = transport
        self.pending: dict[str, Pending] = {}
        self._discovery: Optional[dict[str, Any]] = None
        self._refresh_lock = asyncio.Lock()
        self.data: dict[str, Any] = self._load()

    # ------------------------------------------------------------ storage
    def _load(self) -> dict[str, Any]:
        try:
            return json.loads(self.path.read_text())
        except Exception:  # noqa: BLE001
            return {}

    def _save(self) -> None:
        self.path.parent.mkdir(parents=True, exist_ok=True)
        tmp = self.path.with_suffix(".tmp")
        tmp.write_text(json.dumps(self.data, indent=2))
        os.chmod(tmp, 0o600)
        tmp.replace(self.path)

    @property
    def signed_in(self) -> bool:
        return bool(self.data.get("refresh_token") or self.data.get("access_token"))

    @property
    def sharing(self) -> bool:
        return SHARING_SCOPE in (self.data.get("scopes") or [])

    def account(self) -> Optional[str]:
        return self.data.get("email") or self.data.get("name")

    def preferred_model(self) -> Optional[str]:
        return os.environ.get("OMNISPACE_MODEL") or self.data.get("model")

    def set_model(self, slug: Optional[str]) -> None:
        if slug:
            self.data["model"] = slug
        else:
            self.data.pop("model", None)
        self._save()

    def logout(self) -> None:
        self.data = {}
        try:
            self.path.unlink()
        except FileNotFoundError:
            pass

    # ------------------------------------------------------------ oauth
    async def discovery(self) -> dict[str, Any]:
        if self._discovery:
            return self._discovery
        async with httpx.AsyncClient(timeout=15, transport=self.transport) as c:
            r = await c.get(f"{ISSUER}/.well-known/openid-configuration")
            r.raise_for_status()
            d = r.json()
        if d.get("issuer") != ISSUER:
            raise RuntimeError("ChatGPT sign-in configuration could not be verified")
        for key in ("authorization_endpoint", "token_endpoint"):
            if not str(d.get(key, "")).startswith(ISSUER):
                raise RuntimeError("ChatGPT sign-in configuration could not be verified")
        self._discovery = d
        return d

    async def start(self, redirect_uri: str) -> str:
        d = await self.discovery()
        # drop stale attempts
        self.pending = {k: v for k, v in self.pending.items() if time.time() - v.created < 900}
        state = secrets.token_urlsafe(32)
        verifier = secrets.token_urlsafe(48)
        nonce = secrets.token_urlsafe(32)
        self.pending[state] = Pending(verifier=verifier, nonce=nonce, redirect_uri=redirect_uri)
        params = {
            "client_id": self.data.get("client_id") or "dynamic_agent_client",
            "response_type": "code",
            "redirect_uri": redirect_uri,
            "scope": SCOPES,
            "resource": RESOURCE,
            "state": state,
            "nonce": nonce,
            "code_challenge_method": "S256",
            "code_challenge": _b64url(hashlib.sha256(verifier.encode()).digest()),
        }
        if not self.data.get("client_id"):
            params["agent_name_hint"] = APP_NAME
        if self.data.get("email"):
            params["login_hint"] = self.data["email"]
        return f"{d['authorization_endpoint']}?{urlencode(params)}"

    async def finish(self, state: str, code: Optional[str], client_id: Optional[str], error: Optional[str]) -> None:
        pending = self.pending.pop(state, None)
        if pending is None:
            raise RuntimeError("Unknown or expired sign-in attempt. Start again from OmniSpace.")
        if error:
            raise RuntimeError(f"ChatGPT sign-in failed: {error}")
        cid = client_id or self.data.get("client_id")
        if not code or not cid or cid == "dynamic_agent_client":
            raise RuntimeError("ChatGPT did not complete app registration. Please try again.")
        data = await self._token({
            "grant_type": "authorization_code",
            "client_id": cid,
            "code": code,
            "code_verifier": pending.verifier,
            "redirect_uri": pending.redirect_uri,
            "resource": RESOURCE,
        })
        claims = _jwt_claims(data.get("id_token", ""))
        if claims.get("nonce") not in (None, pending.nonce):
            raise RuntimeError("Sign-in response did not match this request.")
        self.data.update({
            "client_id": cid,
            "email": claims.get("email"),
            "name": claims.get("name"),
            "subject": claims.get("sub"),
        })
        self._store_tokens(data)
        log.info("Signed in with ChatGPT as %s (sharing=%s)", self.account(), self.sharing)

    async def _token(self, form: dict[str, str]) -> dict[str, Any]:
        d = await self.discovery()
        async with httpx.AsyncClient(timeout=20, transport=self.transport) as c:
            r = await c.post(d["token_endpoint"], data=form, headers={"accept": "application/json"})
        try:
            body = r.json()
        except Exception:  # noqa: BLE001
            body = {"error": r.text[:200]}
        if r.status_code >= 400 or not isinstance(body, dict):
            raise RuntimeError(f"ChatGPT token request failed ({r.status_code}): {body.get('error_description') or body.get('error')}")
        return body

    def _store_tokens(self, data: dict[str, Any]) -> None:
        if data.get("access_token"):
            self.data["access_token"] = data["access_token"]
            self.data["expires_at"] = time.time() + float(data.get("expires_in", 3600))
        if data.get("refresh_token"):
            self.data["refresh_token"] = data["refresh_token"]
        if data.get("scope"):
            self.data["scopes"] = str(data["scope"]).split()
        self.data["saved_at"] = time.time()
        self._save()

    async def access_token(self) -> str:
        """A valid access token, refreshing it when it is about to expire."""
        if not self.signed_in:
            raise RuntimeError("Not signed in with ChatGPT")
        def fresh() -> bool:
            return bool(self.data.get("access_token")) and time.time() < float(self.data.get("expires_at", 0)) - 120

        if fresh():
            return self.data["access_token"]
        # One refresh at a time: refresh tokens may rotate, so concurrent jobs must not reuse one.
        async with self._refresh_lock:
            if fresh():
                return self.data["access_token"]
            if not self.data.get("refresh_token"):
                raise RuntimeError("ChatGPT session expired; sign in again")
            data = await self._token({
                "grant_type": "refresh_token",
                "client_id": self.data["client_id"],
                "refresh_token": self.data["refresh_token"],
                "resource": RESOURCE,
            })
            self._store_tokens(data)
            return self.data["access_token"]

    async def models(self) -> list[dict[str, str]]:
        token = await self.access_token()
        async with httpx.AsyncClient(timeout=20, transport=self.transport) as c:
            r = await c.get(f"{RESOURCE}/models", headers={"authorization": f"Bearer {token}", "accept": "application/json"})
        r.raise_for_status()
        body = r.json()
        out = []
        for m in body.get("models", []) if isinstance(body, dict) else []:
            if isinstance(m, dict) and m.get("visibility") == "list" and m.get("slug"):
                out.append({"slug": m["slug"], "name": m.get("display_name") or m["slug"]})
        if not out and isinstance(body, dict) and isinstance(body.get("data"), list):
            out = [{"slug": m["id"], "name": m["id"]} for m in body["data"] if isinstance(m, dict) and m.get("id")]
        return out

    async def pick_model(self) -> str:
        preferred = self.preferred_model()
        if preferred:
            return preferred
        models = await self.models()
        if not models:
            raise RuntimeError("Your ChatGPT plan did not list any models for this app")
        slugs = [m["slug"] for m in models]
        # favour fast models: generation runs while the pilot waits in hyperspace
        for s in slugs:
            if "mini" in s:
                return s
        return slugs[0]
