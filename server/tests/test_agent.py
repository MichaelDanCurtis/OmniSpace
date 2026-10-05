import asyncio
import json
from agents import Runner
from omnispace_server.draft import SystemDraft
from omnispace_server.forge import TOOLS, build_agent, forge_system
from omnispace_server.scripted import ScriptedForgeModel
from tests.fixtures import star


def test_tool_schemas_are_strict():
    names = {t.name for t in TOOLS}
    assert {"survey_star", "add_planet", "add_station", "finalize_system"} <= names
    for t in TOOLS:
        assert t.strict_json_schema
        assert t.params_json_schema["additionalProperties"] is False


def test_scripted_agent_builds_system():
    d = SystemDraft(star=star(settlement=0.7), commander="Michael")
    asyncio.run(forge_system(d, model=ScriptedForgeModel(delay=0), timeout=30))
    assert d.finalized
    sysm = d.build("scripted")
    assert sysm.bodies and sysm.stations and sysm.pois
    assert any("Planet charted" in l for l in d.log)
    json.dumps(sysm.model_dump(mode="json"))


class _NeverFinalizes(ScriptedForgeModel):
    """Builds planets then keeps calling review_system forever (runs out of turns)."""

    async def get_response(self, *a, **kw):
        r = await super().get_response(*a, **kw)
        names = [getattr(o, "name", None) for o in r.output]
        if "finalize_system" in names or not names or names == [None]:
            return self._calls([("review_system", {})])
        return r


def test_partial_system_kept_when_agent_runs_out_of_turns():
    d = SystemDraft(star=star(settlement=0.1), commander="Michael")
    asyncio.run(forge_system(d, model=_NeverFinalizes(delay=0), timeout=30, max_turns=6))
    assert d.finalized and d.bodies


def test_failure_without_bodies_raises():
    class _Mute(ScriptedForgeModel):
        async def get_response(self, *a, **kw):
            return self._calls([("review_system", {})])

    d = SystemDraft(star=star(), commander="Michael")
    try:
        asyncio.run(forge_system(d, model=_Mute(delay=0), timeout=30, max_turns=3))
    except Exception:
        return
    raise AssertionError("expected failure")
