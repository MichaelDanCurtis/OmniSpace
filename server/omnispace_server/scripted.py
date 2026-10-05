"""A deterministic stand-in model for the Agents SDK.

Set OMNISPACE_AI=scripted to exercise the full agent/tool pipeline (and see the
hyperspace telemetry) without an OpenAI key. It is also used by the test-suite.
It plays the role of the LLM: reads tool outputs, then issues tool calls.
"""
from __future__ import annotations

import asyncio
import hashlib
import json
import random
from typing import Any, AsyncIterator

from agents.items import ModelResponse
from agents.models.interface import Model
from agents.usage import Usage
from openai.types.responses import ResponseFunctionToolCall, ResponseOutputMessage, ResponseOutputText

SYLL = ["ka", "ri", "on", "the", "va", "lis", "mor", "ae", "dun", "sel", "ix", "or", "tha", "ny", "bel", "cor"]


def _name(rng: random.Random) -> str:
    return "".join(rng.choice(SYLL) for _ in range(rng.randint(2, 3))).capitalize()


class ScriptedForgeModel(Model):
    label = "scripted-demo"

    def __init__(self, delay: float = 0.6):
        self.delay = delay

    async def get_response(self, system_instructions, input, model_settings, tools, output_schema, handoffs, tracing, *,
                           previous_response_id=None, conversation_id=None, prompt=None, **kwargs: Any) -> ModelResponse:
        await asyncio.sleep(self.delay)
        items = input if isinstance(input, list) else [{"role": "user", "content": input}]
        tool_names = [getattr(t, "name", "") for t in (tools or [])]
        if any(n.endswith("dress_system") for n in tool_names):
            return self._dress(items, next(n for n in tool_names if n.endswith("dress_system")))
        calls: dict[str, str] = {}
        outputs: list[tuple[str, dict[str, Any]]] = []
        for it in items:
            d = it if isinstance(it, dict) else getattr(it, "model_dump", lambda: {})()
            if d.get("type") == "function_call":
                calls[d["call_id"]] = d["name"]
            elif d.get("type") == "function_call_output":
                try:
                    out = json.loads(d.get("output") or "{}")
                except Exception:  # noqa: BLE001
                    out = {}
                outputs.append((calls.get(d["call_id"], "?"), out))
        names = [n for n, _ in outputs]
        rng = random.Random(len(items))
        survey = next((o for n, o in outputs if n == "survey_star"), None)
        if survey:
            rng = random.Random(int(hashlib.md5(survey["star"]["name"].encode()).hexdigest()[:8], 16))

        if "survey_star" not in names:
            return self._calls([("survey_star", {})])
        if "add_planet" not in names:
            return self._calls(self._design(survey, rng))
        if "add_station" not in names and "add_moon" not in names and "finalize_system" not in names:
            planets = [o for n, o in outputs if n == "add_planet" and o.get("ok")]
            return self._calls(self._details(survey, planets, rng) or [("finalize_system", {})])
        if "finalize_system" not in names:
            return self._calls([("finalize_system", {})])
        return ModelResponse(output=[ResponseOutputMessage(id="msg", type="message", role="assistant", status="completed",
                                                           content=[ResponseOutputText(type="output_text", text="done", annotations=[])])],
                             usage=Usage(), response_id=None)

    def stream_response(self, *args: Any, **kwargs: Any) -> AsyncIterator[Any]:  # pragma: no cover - not used
        raise NotImplementedError

    # ---------------------------------------------------------------- dressing pass
    def _dress(self, items: list[Any], tool_name: str) -> ModelResponse:
        """Rename everything from the skeleton in the prompt and write stock lore (test stand-in)."""
        text = ""
        for it in items:
            d = it if isinstance(it, dict) else getattr(it, "model_dump", lambda: {})()
            if d.get("role") == "user":
                c = d.get("content")
                text = c if isinstance(c, str) else "".join(x.get("text", "") for x in c if isinstance(x, dict))
        try:
            skel = json.loads(text[text.index("{"):])
        except Exception:  # noqa: BLE001
            skel = {"bodies": [], "stations": [], "pois": [], "star": {"name": "Unknown"}}
        star = skel["star"]["name"]
        rng = random.Random(star)
        inhabited = skel.get("settlement_index", 0) > 0.05
        args = {
            "profile": {
                "description": f"{star}: a {skel['star'].get('label', 'star')} system the Forge has named.",
                "lore": f"Settlers reached {star} two generations ago and named its worlds after the ships that carried them. "
                        + ("Trade still follows the old arrival lanes." if inhabited else "They did not stay."),
                "faction": "Free Worlds Accord" if inhabited else "Unclaimed", "government": "Democracy" if inhabited else "None",
                "economy": "Extraction" if inhabited else "None", "security": "low" if inhabited else "none",
                "population": 120000 if inhabited else 0, "danger": 1 if inhabited else 3, "star_flavor": skel["star"].get("label", "star"),
            },
            "bodies": [{"id": b["id"], "name": f"{star} {_name(rng)}", "description": f"A {b['type'].replace('_', ' ')} world named by the Forge."} for b in skel["bodies"]],
            "stations": [{"id": s_["id"], "name": f"{_name(rng)} Exchange", "tagline": "Further, together", "description": "A Forge-named station.",
                          "faction": "Free Worlds Accord", "economy": "Extraction"} for s_ in skel["stations"]],
            "pois": [{"id": p["id"], "name": "The Quiet Signal", "description": "A hook the Forge wrote."} for p in skel["pois"]],
        }
        return self._calls([(tool_name, args)])

    # ---------------------------------------------------------------- script
    def _calls(self, calls: list[tuple[str, dict[str, Any]]]) -> ModelResponse:
        out = [ResponseFunctionToolCall(id=f"fc_{i}", call_id=f"call_{random.getrandbits(40):x}", name=n, arguments=json.dumps(a),
                                        type="function_call", status="completed") for i, (n, a) in enumerate(calls)]
        return ModelResponse(output=out, usage=Usage(), response_id=None)

    def _design(self, survey: dict[str, Any], rng: random.Random) -> list[tuple[str, dict[str, Any]]]:
        z = survey["zones_au"]
        settle = survey["settlement_index"]
        star = survey["star"]["name"]
        calls: list[tuple[str, dict[str, Any]]] = []
        inhabited = settle > 0.05
        calls.append(("set_system_profile", {
            "description": f"The {star} system, charted live by the Stellar Forge.",
            "lore": f"Survey drones reached {star} only recently. " + ("Traders followed within weeks." if inhabited else "No one has followed."),
            "faction": "Free Worlds Accord" if inhabited else "Unclaimed", "government": "Democracy" if inhabited else "None",
            "economy": "Extraction" if inhabited else "None", "security": "medium" if settle > 0.3 else ("low" if inhabited else "none"),
            "population": int(10 ** (4 + settle * 5)) if inhabited else 0, "danger": 1 if inhabited else 3,
            "star_flavor": survey["star"]["label"],
        }))
        a = max(z["min_safe_orbit"] * 1.6, 0.05)
        hz = z["habitable_zone"]
        frost = z["frost_line"]
        for i in range(rng.randint(3, 6)):
            if a > z["practical_outer_limit"] * 1.5:
                break
            if hz[0] <= a <= hz[1]:
                ptype, r = "ocean", 6800
            elif a < hz[0]:
                ptype, r = ("lava", 3000) if a < hz[0] * 0.5 else ("desert", 4200)
            elif a < frost:
                ptype, r = "rocky", 3500
            else:
                ptype, r = rng.choice([("gas_giant", 60000), ("ice_giant", 24000), ("ice", 2500)])
            calls.append(("add_planet", {
                "name": f"{_name(rng)}", "planet_type": ptype, "orbit_au": round(a, 4), "radius_km": r,
                "palette": [], "description": f"A {ptype.replace('_', ' ')} world.", "atmosphere_color": "#9cc8ff" if ptype in ("ocean", "ice_giant", "gas_giant") else None,
                "cloud_cover": 0.5 if ptype == "ocean" else 0, "band_intensity": 0.8 if "giant" in ptype else 0, "axial_tilt_deg": rng.uniform(-25, 25),
            }))
            a *= rng.uniform(1.6, 2.2)
        return calls

    def _details(self, survey: dict[str, Any], planets: list[dict[str, Any]], rng: random.Random) -> list[tuple[str, dict[str, Any]]]:
        calls: list[tuple[str, dict[str, Any]]] = []
        if not planets:
            return calls
        host = planets[min(1, len(planets) - 1)]["id"]
        calls.append(("add_moon", {"parent_id": host, "name": f"{_name(rng)}", "moon_type": "barren", "orbit_km": 400000, "radius_km": 900,
                                   "palette": [], "description": "A small cratered moon.", "atmosphere_color": None}))
        if survey["settlement_index"] > 0.05:
            calls.append(("add_station", {"name": f"{_name(rng)} Exchange", "host_id": host, "station_type": "spire" if survey["settlement_index"] > 0.4 else "outpost",
                                          "orbit_km": 30000, "faction": "Free Worlds Accord", "economy": "Extraction",
                                          "services": ["refuel", "repair", "market", "cartographics"], "tagline": "Further, together",
                                          "description": "A freshly built trade platform."}))
        calls.append(("add_point_of_interest", {"name": "Unidentified Signal Source", "kind": "signal", "host_id": planets[-1]["id"], "distance_km": 90000,
                                                "description": "A looping distress call in an obsolete cipher.", "reward_credits": 3000}))
        return calls
