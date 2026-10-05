"""The Stellar Forge dressing pass: meaning on top of the survey skeleton.

The onboard survey (survey.py) has already decided what orbits the star. This agent
gets that skeleton and writes the layer procedural code cannot: names that read as a
culture, history that explains the economy, factions that follow from the lore,
descriptions with a hook, and continuity with the charted neighbours. One tool call,
strictly validated, applied in place. If anything fails the skeleton stands.
"""
from __future__ import annotations

import asyncio
import json
import logging
from dataclasses import dataclass
from typing import Any, Literal, Optional

from agents import Agent, ModelSettings, RunContextWrapper, function_tool, tool_namespace
from pydantic import BaseModel, Field

from .bible import CANON_FACTIONS, ECONOMIES, GOVERNMENTS, SERVICES
from .draft import AU, KM, SystemDraft
from .forge import ForgeRuntime, _run, model_name
from .schema import SystemProfile

log = logging.getLogger("omnispace.dressing")

Security = Literal["none", "low", "medium", "high"]


class BodyDressing(BaseModel):
    id: str = Field(description="Body id from the skeleton (unchanged).")
    name: str = Field(description="New name, 2-24 characters. Roman-numeral style ('Tanara VI') or a proper settler name.")
    description: str = Field(description="1-2 vivid sentences. Mention anything that makes it memorable.")


class StationDressing(BaseModel):
    id: str
    name: str = Field(description="Evocative station name, e.g. 'Calloway Exchange', 'Brightwater Spire'.")
    tagline: str = Field(description="A short motto or slogan.")
    description: str = Field(description="1-2 sentences about who runs it and what it is for.")
    faction: str = Field(description="Controlling faction (prefer canon).")
    economy: str = Field(description=f"One of {ECONOMIES}.")


class PoiDressing(BaseModel):
    id: str
    name: str
    description: str = Field(description="1-2 sentences. This is the system's hook; make it intriguing.")


class ProfileDressing(BaseModel):
    description: str = Field(description="One vivid sentence summarising the system.")
    lore: str = Field(description="2-4 sentences of history and current events. Reference charted neighbours when it fits.")
    faction: str
    government: str = Field(description=f"One of {GOVERNMENTS}.")
    economy: str = Field(description=f"One of {ECONOMIES} or 'None' if uninhabited.")
    security: Security
    population: int
    danger: int = Field(description="0 (safe) to 5 (lethal).")
    star_flavor: str = Field(description="Short evocative label for the star.")


@dataclass
class DressingContext:
    draft: SystemDraft
    applied: bool = False
    errors: list[str] | None = None


def apply_dressing(draft: SystemDraft, profile: ProfileDressing, bodies: list[BodyDressing], stations: list[StationDressing],
                   pois: list[PoiDressing]) -> list[str]:
    """Apply validated dressing to the draft in place; returns a list of (non-fatal) problems."""
    problems: list[str] = []
    econ = profile.economy if profile.economy in ECONOMIES or profile.economy == "None" else draft.profile.economy if draft.profile else "None"
    if econ != profile.economy:
        problems.append(f"economy '{profile.economy}' is not in the list; kept '{econ}'")
    sec = profile.security if profile.security in ("none", "low", "medium", "high") else "low"
    pop = max(0, int(profile.population))
    if draft.star.settlement <= 0.05:
        pop, econ, faction = 0, "None", "Unclaimed"
        if profile.faction != "Unclaimed":
            problems.append("deep space systems are Unclaimed; faction overridden")
    else:
        faction = profile.faction.strip()[:60] or "Independent"
    draft.profile = SystemProfile(
        description=profile.description.strip()[:400], lore=profile.lore.strip()[:900], faction=faction,
        government=(profile.government.strip()[:40] or "None"), economy=econ, security=sec,  # type: ignore[arg-type]
        population=pop, danger=max(0, min(5, int(profile.danger))),
    )
    draft.star_flavor = profile.star_flavor.strip()[:120] or draft.star_flavor
    used: set[str] = set()
    by_id = {b.id: b for b in draft.bodies}
    for bd in bodies:
        b = by_id.get(bd.id)
        if b is None:
            problems.append(f"unknown body id {bd.id}")
            continue
        name = bd.name.strip()[:28]
        if len(name) < 2 or name.lower() in used:
            problems.append(f"bad or duplicate body name '{bd.name}'")
            continue
        used.add(name.lower())
        b.name = name
        if bd.description.strip():
            b.description = bd.description.strip()[:400]
    st_by_id = {s.id: s for s in draft.stations}
    for sd in stations:
        s = st_by_id.get(sd.id)
        if s is None:
            problems.append(f"unknown station id {sd.id}")
            continue
        name = sd.name.strip()[:40]
        if len(name) >= 3 and name.lower() not in used:
            used.add(name.lower())
            s.name = name
        s.tagline = sd.tagline.strip()[:80] or s.tagline
        s.description = sd.description.strip()[:400] or s.description
        s.faction = sd.faction.strip()[:60] or s.faction
        if sd.economy in ECONOMIES:
            s.economy = sd.economy
    poi_by_id = {p.id: p for p in draft.pois}
    for pd in pois:
        p = poi_by_id.get(pd.id)
        if p is None:
            problems.append(f"unknown poi id {pd.id}")
            continue
        p.name = pd.name.strip()[:48] or p.name
        p.description = pd.description.strip()[:400] or p.description
    draft.log.append(f"Stellar Forge: {draft.star.name} named and chronicled")
    return problems


@function_tool
def dress_system(
    ctx: RunContextWrapper[DressingContext],
    profile: ProfileDressing,
    bodies: list[BodyDressing],
    stations: list[StationDressing],
    pois: list[PoiDressing],
) -> str:
    """Commit the names, descriptions, lore and profile for every body, station and point
    of interest in the skeleton. Call it exactly once with every id covered.

    Args:
        profile: System-wide profile and lore.
        bodies: One entry per planet and moon id in the skeleton.
        stations: One entry per station id.
        pois: One entry per point-of-interest id.
    """
    c = ctx.context
    problems = apply_dressing(c.draft, profile, bodies, stations, pois)
    c.applied = True
    c.errors = problems
    return json.dumps({"ok": True, "problems": problems})


DIRECTIVE = """
You are the STELLAR FORGE, the world-building intelligence of OmniSpace, a cinematic
space flight game in the spirit of Elite Dangerous. The onboard survey has already
charted the physical skeleton of the system a pilot is jumping into: the star, every
planet and moon with its type and orbit, rings, belts, stations and points of interest.
THAT SKELETON IS FIXED. Your job is the layer only a storyteller can add:

- NAMES that read as one culture: planets may follow star-name + roman numeral
  ("Tanara VI") or carry proper settler names; stations get evocative names
  ("Calloway Exchange", "Brightwater Spire"). Never reuse a name. Keep the skeleton ids.
- A PROFILE whose faction, government and economy follow from the lore and the
  settlement guidance (core systems: canon factions, busy; frontier: sparse, scrappy;
  deep space: Unclaimed, population 0, security none).
- LORE: 2-4 sentences of history and current events. Reference charted neighbours
  (trade lanes, rivalries, refugees, shared history) when it fits.
- DESCRIPTIONS: 1-2 vivid sentences per body, station and point of interest. Give the
  system ONE memorable hook and express it through the point of interest and one or
  two descriptions. Physical facts in the skeleton (type, temperature, rings) must not
  be contradicted. No markdown.

Call dress_system exactly once with an entry for EVERY body, station and point of
interest id listed in the skeleton, then stop.

CANON FACTIONS
{factions}
""".strip()


def skeleton_summary(draft: SystemDraft) -> dict[str, Any]:
    return {
        "star": {"id": draft.star.id, "name": draft.star.name, "class": draft.star.cls, "label": draft.star_flavor,
                 "temperature_k": round(draft.star.temperature), "age_gyr": draft.star.ageGyr, "region": draft.star.region},
        "settlement_index": round(draft.star.settlement, 2),
        "settlement_guidance": draft.settlement_guidance(),
        "neighbors": draft.neighbor_summaries or [{"name": n.name, "distance_ly": round(n.distanceLy, 1), "charted": False} for n in draft.neighbors[:8]],
        "bodies": [{"id": b.id, "kind": b.kind, "type": b.type, "parent": b.parent, "orbit_au": round(b.orbitM / AU, 3) if b.kind == "planet" else None,
                    "orbit_km": round(b.orbitM / KM) if b.kind == "moon" else None, "radius_km": round(b.radiusM / KM),
                    "temperature_k": round(b.temperatureK) if b.temperatureK else None, "rings": bool(b.rings), "survey_name": b.name}
                   for b in draft.bodies],
        "stations": [{"id": s.id, "kind": s.kind, "host": s.parent, "survey_name": s.name, "services": s.services} for s in draft.stations],
        "belts": [{"id": b.id, "name": b.name, "composition": b.composition} for b in draft.belts],
        "pois": [{"id": p.id, "kind": p.kind, "host": p.parent, "survey_name": p.name} for p in draft.pois],
        "economies": ECONOMIES, "governments": GOVERNMENTS, "station_services": SERVICES,
    }


def build_dressing_agent(rt: ForgeRuntime) -> Agent[DressingContext]:
    m = rt.model if rt.model is not None else model_name()
    tools: list[Any] = [dress_system]
    if rt.namespace:
        tools = tool_namespace(name="stellar_forge", description="Commit the names and lore of the system.", tools=[dress_system])
    facts = "\n".join(f"- {k}: {v}" for k, v in CANON_FACTIONS.items())
    agent: Agent[DressingContext] = Agent(
        name="Stellar Forge",
        instructions=DIRECTIVE.format(factions=facts),
        tools=tools,
        tool_use_behavior="stop_on_first_tool",
        **({"model": m} if m is not None else {}),
    )
    if rt.stateless:
        agent.model_settings = agent.model_settings.resolve(ModelSettings(store=False, response_include=["reasoning.encrypted_content"] if rt.include_reasoning else None))
    return agent


async def dress_draft(draft: SystemDraft, runtime: ForgeRuntime, timeout: float = 120.0) -> list[str]:
    """Run the dressing agent on a surveyed draft. Returns problems; raises on failure."""
    ctx = DressingContext(draft=draft)
    prompt = ("Here is the charted skeleton. Name and chronicle it for Commander "
              f"{draft.commander}, who is arriving now.\n\n{json.dumps(skeleton_summary(draft))}")
    try:
        await asyncio.wait_for(_run(build_dressing_agent(runtime), prompt, ctx, runtime, max_turns=4), timeout=timeout)
    except Exception as e:  # noqa: BLE001
        if runtime.stateless and not runtime.safe_mode and not ctx.applied and "400" in str(e):
            log.warning("retrying dressing in safe stateless mode: %s", e)
            runtime.include_reasoning = False
            runtime.safe_mode = True
            if hasattr(runtime.model, "safe"):
                runtime.model.safe = True  # type: ignore[union-attr]
            await asyncio.wait_for(_run(build_dressing_agent(runtime), prompt, ctx, runtime, max_turns=4), timeout=timeout)
        else:
            raise
    if not ctx.applied:
        raise RuntimeError("The Forge finished without calling dress_system")
    return ctx.errors or []
