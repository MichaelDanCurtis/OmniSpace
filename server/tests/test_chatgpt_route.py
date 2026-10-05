"""The ChatGPT-plan route: stream=true, store=false, instructions (no system
message), namespaced tools. Uses an in-process mock of the Responses API."""
import asyncio
import json

import httpx
from agents import OpenAIResponsesModel, set_tracing_disabled
from openai import AsyncOpenAI

from omnispace_server.draft import SystemDraft
from omnispace_server.forge import ChatGPTPlanModel, ForgeRuntime, forge_system
from tests.fixtures import star

set_tracing_disabled(True)


class MockResponses:
    def __init__(self, reject_include=False):
        self.requests = []
        self.turn = 0
        self.reject_include = reject_include

    def _fc(self, name, args, i):
        return {"type": "function_call", "id": f"fc_{self.turn}_{i}", "call_id": f"call_{self.turn}_{i}", "name": name,
                "namespace": "stellar_forge", "arguments": json.dumps(args), "status": "completed"}

    def _plan(self):
        if self.turn == 0:
            return [{"type": "reasoning", "id": "rs_1", "summary": [], "encrypted_content": "ENC"}, self._fc("survey_star", {}, 0)]
        if self.turn == 1:
            return [
                self._fc("set_system_profile", {"description": "d", "lore": "l", "faction": "Free Worlds Accord", "government": "Democracy",
                                                "economy": "Industrial", "security": "high", "population": 1000, "danger": 1, "star_flavor": "x"}, 0),
                self._fc("add_planet", {"name": "Alpha", "planet_type": "ocean", "orbit_au": 1.0, "radius_km": 6000, "palette": [],
                                        "description": "wet", "atmosphere_color": "#88aaff", "cloud_cover": 0.5, "band_intensity": 0,
                                        "axial_tilt_deg": 10}, 1),
            ]
        if self.turn == 2:
            return [self._fc("add_station", {"name": "Hub", "host_id": "alpha", "station_type": "spire", "orbit_km": 20000,
                                             "faction": "Free Worlds Accord", "economy": "Industrial", "services": ["market"],
                                             "tagline": "t", "description": "d"}, 0)]
        return [self._fc("finalize_system", {}, 0)]

    def handler(self, request: httpx.Request):
        body = json.loads(request.content)
        self.requests.append(body)
        if self.reject_include and body.get("include"):
            return httpx.Response(400, json={"error": {"message": "Unsupported parameter: include", "type": "invalid_request_error"}})
        out = self._plan()
        rid = f"resp_{self.turn}"
        self.turn += 1
        resp = {"id": rid, "object": "response", "created_at": 1, "model": "gpt-test", "status": "completed", "output": out,
                "parallel_tool_calls": True, "tool_choice": "auto", "tools": [],
                "usage": {"input_tokens": 1, "output_tokens": 1, "total_tokens": 2, "input_tokens_details": {"cached_tokens": 0},
                          "output_tokens_details": {"reasoning_tokens": 0}}}
        events = [{"type": "response.completed", "sequence_number": 0, "response": resp}]
        sse = "".join(f"event: {e['type']}\ndata: {json.dumps(e)}\n\n" for e in events)
        return httpx.Response(200, headers={"content-type": "text/event-stream"}, content=sse.encode())


def _runtime(mock):
    client = AsyncOpenAI(api_key="token", base_url="https://api.openai.com/v1", max_retries=0,
                         http_client=httpx.AsyncClient(transport=httpx.MockTransport(mock.handler)))
    return ForgeRuntime(model=ChatGPTPlanModel(model="gpt-test", openai_client=client), label="mock", streamed=True, stateless=True, namespace=True)


def test_chatgpt_plan_request_shape():
    mock = MockResponses()
    d = SystemDraft(star=star(settlement=0.7), commander="M")
    asyncio.run(forge_system(d, runtime=_runtime(mock), timeout=30))
    assert d.finalized and d.stations
    first = mock.requests[0]
    assert first["stream"] is True and first["store"] is False
    assert "instructions" in first and all(i.get("role") != "system" for i in first["input"])
    assert first["tools"][0]["type"] == "namespace"
    for banned in ("temperature", "top_p", "max_output_tokens", "metadata", "background", "previous_response_id"):
        assert banned not in first


def test_chatgpt_plan_falls_back_to_safe_mode():
    mock = MockResponses(reject_include=True)
    d = SystemDraft(star=star(settlement=0.7), commander="M")
    asyncio.run(forge_system(d, runtime=_runtime(mock), timeout=30))
    assert d.finalized
    later = mock.requests[-1]
    assert not later.get("include")
    assert "prompt_cache_key" not in later
    assert all(i.get("type") != "reasoning" and "id" not in i for i in later["input"] if isinstance(i, dict))
