import asyncio
import base64
import json
import time
from urllib.parse import parse_qs, urlparse

import httpx

from omnispace_server.chatgpt_auth import ChatGPTAuth


def _jwt(claims):
    enc = lambda d: base64.urlsafe_b64encode(json.dumps(d).encode()).rstrip(b"=").decode()  # noqa: E731
    return f"{enc({'alg': 'RS256'})}.{enc(claims)}.sig"


class FakeOpenAI:
    def __init__(self):
        self.token_requests = []
        self.nonce = None

    def handler(self, request: httpx.Request):
        url = str(request.url)
        if url.endswith("/.well-known/openid-configuration"):
            return httpx.Response(200, json={"issuer": "https://auth.openai.com", "authorization_endpoint": "https://auth.openai.com/oauth/authorize",
                                             "token_endpoint": "https://auth.openai.com/oauth/token", "jwks_uri": "https://auth.openai.com/jwks"})
        if url.endswith("/oauth/token"):
            form = parse_qs(request.content.decode())
            self.token_requests.append(form)
            body = {"access_token": f"at{len(self.token_requests)}", "token_type": "Bearer", "expires_in": 3600,
                    "refresh_token": "rt", "scope": "openid email offline_access resource.invoke chatgpt.tokens.use.direct"}
            if form["grant_type"][0] == "authorization_code":
                body["id_token"] = _jwt({"sub": "u1", "email": "pilot@example.com", "nonce": self.nonce})
            return httpx.Response(200, json=body)
        if url.endswith("/v1/models"):
            assert request.headers["authorization"].startswith("Bearer at")
            return httpx.Response(200, json={"models": [{"slug": "gpt-big", "display_name": "Big", "visibility": "list"},
                                                        {"slug": "gpt-x-mini", "display_name": "Mini", "visibility": "list"},
                                                        {"slug": "hidden", "display_name": "H", "visibility": "hide"}]})
        return httpx.Response(404)


def test_sign_in_refresh_and_models(tmp_path):
    fake = FakeOpenAI()
    auth = ChatGPTAuth(tmp_path / "auth.json", transport=httpx.MockTransport(fake.handler))

    async def flow():
        url = await auth.start("http://127.0.0.1:8000/auth/callback")
        q = parse_qs(urlparse(url).query)
        assert q["client_id"] == ["dynamic_agent_client"]
        assert q["code_challenge_method"] == ["S256"] and q["resource"] == ["https://api.openai.com/v1"]
        assert "chatgpt.tokens.use.direct" in q["scope"][0]
        assert q["agent_name_hint"] == ["OmniSpace"]
        fake.nonce = q["nonce"][0]
        await auth.finish(q["state"][0], "CODE", "client_123", None)
        assert auth.signed_in and auth.sharing and auth.account() == "pilot@example.com"
        tok = fake.token_requests[0]
        assert tok["client_id"] == ["client_123"] and tok["code_verifier"][0]
        assert await auth.access_token() == "at1"
        auth.data["expires_at"] = time.time() - 1  # force refresh
        assert await auth.access_token() == "at2"
        assert fake.token_requests[1]["grant_type"] == ["refresh_token"]
        models = await auth.models()
        assert [m["slug"] for m in models] == ["gpt-big", "gpt-x-mini"]
        assert await auth.pick_model() == "gpt-x-mini"
        # state cannot be replayed
        try:
            await auth.finish(q["state"][0], "CODE", "client_123", None)
            raise AssertionError("replay accepted")
        except RuntimeError:
            pass

    asyncio.run(flow())
    saved = json.loads((tmp_path / "auth.json").read_text())
    assert saved["client_id"] == "client_123" and saved["refresh_token"] == "rt"
    assert (tmp_path / "auth.json").stat().st_mode & 0o777 == 0o600
