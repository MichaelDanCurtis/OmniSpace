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
