"""The Stellar Forge agent (OpenAI Agents SDK).

The agent gets a toolbox that mutates a SystemDraft. Tools validate physics and
canon, return actionable errors, and stream telemetry lines to the client
while the pilot sits in the hyperspace tunnel.
"""
from __future__ import annotations

import asyncio
import json
import logging
import os
from dataclasses import dataclass
from typing import Any, Literal, Optional

from agents import Agent, ModelSettings, RunContextWrapper, Runner, function_tool
from agents.agent import ToolsToFinalOutputResult
from agents.models.interface import Model
from agents.tool import FunctionToolResult

from .bible import directive
from .draft import SystemDraft

log = logging.getLogger("omnispace.forge")

PlanetType = Literal["rocky", "barren", "desert", "lava", "ice", "ocean", "earthlike", "jungle", "toxic", "gas_giant", "ice_giant"]
MoonType = Literal["rocky", "barren", "desert", "lava", "ice", "ocean", "earthlike", "jungle", "toxic"]
StationType = Literal["spire", "ring", "outpost"]
PoiKind = Literal["derelict", "signal", "anomaly", "beacon", "wreck"]
Security = Literal["none", "low", "medium", "high"]
Economy = Literal["Industrial", "Extraction", "Refinery", "Agriculture", "High Tech", "Tourism", "Military", "Research", "Colony", "None"]


@dataclass
class ForgeContext:
    draft: SystemDraft


def _out(d: dict[str, Any]) -> str:
    return json.dumps(d)


@function_tool
def survey_star(ctx: RunContextWrapper[ForgeContext]) -> str:
    """Survey the star you are building around. Returns the fixed stellar properties,
    habitable zone and frost line (AU), settlement guidance for the region, the canon
    factions and any already-charted neighbouring systems. Always call this first."""
    return _out(ctx.context.draft.survey())


@function_tool
def set_system_profile(
    ctx: RunContextWrapper[ForgeContext],
    description: str,
    lore: str,
    faction: str,
    government: str,
    economy: Economy,
    security: Security,
    population: int,
    danger: int,
    star_flavor: str,
) -> str:
    """Set the system-wide profile shown on the pilot's system map.

    Args:
        description: One vivid sentence summarising the system.
        lore: 2-4 sentences of history and current events. Reference neighbours when it fits.
        faction: Controlling faction (prefer a canon faction; 'Unclaimed' if uninhabited).
        government: e.g. Corporate, Democracy, Theocracy, Cooperative, Anarchy, Directorate, Patronage, None.
        economy: Primary economy, or 'None' if uninhabited.
        security: Patrol presence.
        population: Total population (0 if uninhabited).
        danger: 0 (safe) to 5 (lethal).
        star_flavor: Short evocative label for the star, e.g. 'Ancient orange dwarf, heavy flare activity'.
    """
    return _out(ctx.context.draft.set_profile(description, lore, faction, government, economy, security, population, danger, star_flavor))


@function_tool
def add_planet(
    ctx: RunContextWrapper[ForgeContext],
    name: str,
    planet_type: PlanetType,
    orbit_au: float,
    radius_km: float,
    palette: list[str],
    description: str,
    atmosphere_color: Optional[str],
    cloud_cover: float,
    band_intensity: float,
    axial_tilt_deg: float,
) -> str:
    """Add a planet orbiting the star. Returns its id (needed for moons, rings, stations).

    Args:
        name: Planet name.
        planet_type: Surface/composition class. Habitable types need the habitable zone.
        orbit_au: Orbital radius in AU (see survey zones).
        radius_km: Planet radius in km (earthlike ~6000, ice giant ~25000, gas giant ~70000).
        palette: Exactly three hex colours [dark base, mid tone, highlight] for the shader.
        description: 1-2 vivid sentences.
        atmosphere_color: Hex colour of the atmospheric rim glow, or null if airless.
        cloud_cover: 0..1 cloud layer coverage (terrestrial worlds).
        band_intensity: 0..1 strength of cloud bands (giants), 0 for solid worlds.
        axial_tilt_deg: Axial tilt in degrees.
    """
    return _out(ctx.context.draft.add_planet(name, planet_type, orbit_au, radius_km, palette, description, atmosphere_color, cloud_cover, band_intensity, axial_tilt_deg))


@function_tool
def add_moon(
    ctx: RunContextWrapper[ForgeContext],
    parent_id: str,
    name: str,
    moon_type: MoonType,
    orbit_km: float,
    radius_km: float,
    palette: list[str],
    description: str,
    atmosphere_color: Optional[str],
) -> str:
    """Add a moon around a planet.

    Args:
        parent_id: Planet id returned by add_planet.
        name: Moon name.
        moon_type: Surface class.
        orbit_km: Distance from planet centre in km (must clear the planet and its rings).
        radius_km: Moon radius in km (< 45% of parent).
        palette: Three hex colours.
        description: 1-2 sentences.
        atmosphere_color: Hex rim colour or null.
    """
    return _out(ctx.context.draft.add_moon(parent_id, name, moon_type, orbit_km, radius_km, palette, description, atmosphere_color))


@function_tool
def add_rings(ctx: RunContextWrapper[ForgeContext], planet_id: str, inner_km: float, outer_km: float, color: str, opacity: float) -> str:
    """Give a planet a ring system. Add rings BEFORE adding moons/stations around that planet.

    Args:
        planet_id: Planet id.
        inner_km: Inner ring radius in km (>= 1.15x planet radius).
        outer_km: Outer ring radius in km (<= 4.5x planet radius).
        color: Hex colour of the ring material.
        opacity: 0.15..1 ring density.
    """
    return _out(ctx.context.draft.add_rings(planet_id, inner_km, outer_km, color, opacity))


@function_tool
def add_asteroid_belt(ctx: RunContextWrapper[ForgeContext], name: str, orbit_au: float, width_au: float, composition: str) -> str:
    """Add a stellar asteroid belt between planetary orbits.

    Args:
        name: Belt name.
        orbit_au: Belt centre radius in AU.
        width_au: Belt width in AU.
        composition: e.g. metallic, rocky, icy, carbonaceous.
    """
    return _out(ctx.context.draft.add_belt(name, orbit_au, width_au, composition))


@function_tool
def add_station(
    ctx: RunContextWrapper[ForgeContext],
    name: str,
    host_id: str,
    station_type: StationType,
    orbit_km: float,
    faction: str,
    economy: Literal["Industrial", "Extraction", "Refinery", "Agriculture", "High Tech", "Tourism", "Military", "Research", "Colony"],
    services: list[str],
    tagline: str,
    description: str,
) -> str:
    """Add a dockable space station in orbit around a planet or moon.

    Args:
        name: Station name.
        host_id: Body id it orbits.
        station_type: 'spire' (towering trade spire), 'ring' (orbital ring habitat) or 'outpost' (small frontier truss).
        orbit_km: Orbit radius from host centre in km.
        faction: Controlling faction.
        economy: Station economy (drives its market).
        services: Subset of refuel, repair, market, cartographics, shipyard, outfitting, missions.
        tagline: Short slogan shown on the station's holo-sign.
        description: 1-2 sentences.
    """
    return _out(ctx.context.draft.add_station(name, host_id, station_type, orbit_km, faction, economy, services, tagline, description))


@function_tool
def add_point_of_interest(
    ctx: RunContextWrapper[ForgeContext],
    name: str,
    kind: PoiKind,
    host_id: str,
    distance_km: float,
    description: str,
    reward_credits: int,
) -> str:
    """Add a discoverable point of interest (derelict, signal, anomaly, beacon or wreck) near a body.

    Args:
        name: POI name.
        kind: Type of contact.
        host_id: Body id it is near.
        distance_km: Distance from the body centre in km.
        description: The mystery, in 1-2 sentences.
        reward_credits: Data value awarded when a pilot investigates it (500-15000).
    """
    return _out(ctx.context.draft.add_poi(name, kind, host_id, distance_km, description, reward_credits))


@function_tool
def review_system(ctx: RunContextWrapper[ForgeContext]) -> str:
    """Review everything added so far plus any outstanding issues."""
    return _out(ctx.context.draft.review())


@function_tool
def finalize_system(ctx: RunContextWrapper[ForgeContext]) -> str:
    """Commit the system to the galactic record. Call exactly once when finished."""
    return _out(ctx.context.draft.finalize())


TOOLS = [survey_star, set_system_profile, add_planet, add_moon, add_rings, add_asteroid_belt, add_station,
         add_point_of_interest, review_system, finalize_system]


def _stop_when_finalized(ctx: RunContextWrapper[ForgeContext], results: list[FunctionToolResult]) -> ToolsToFinalOutputResult:
    if ctx.context.draft.finalized:
        return ToolsToFinalOutputResult(is_final_output=True, final_output="finalized")
    return ToolsToFinalOutputResult(is_final_output=False)


def model_name() -> Optional[str]:
    return os.environ.get("OMNISPACE_MODEL") or None


def configure_openai() -> None:
    """Optional: OMNISPACE_API=chat switches the SDK to Chat Completions, which lets
    you point OPENAI_BASE_URL at OpenAI-compatible servers (Ollama, LM Studio, vLLM...)."""
    if os.environ.get("OMNISPACE_API", "").lower() in ("chat", "chat_completions"):
        from agents import set_default_openai_api

        set_default_openai_api("chat_completions")


configure_openai()


def build_agent(model: Optional[str | Model] = None) -> Agent[ForgeContext]:
    m = model if model is not None else model_name()
    agent: Agent[ForgeContext] = Agent(
        name="Stellar Forge",
        instructions=directive(),
        tools=list(TOOLS),
        tool_use_behavior=_stop_when_finalized,
        **({"model": m} if m is not None else {}),
    )
    # Keep the SDK's per-model defaults (e.g. GPT-5 reasoning settings) and layer ours on top.
    agent.model_settings = agent.model_settings.resolve(ModelSettings(parallel_tool_calls=True))
    effort = os.environ.get("OMNISPACE_REASONING")
    if effort:
        from openai.types.shared.reasoning import Reasoning

        agent.model_settings = agent.model_settings.resolve(ModelSettings(reasoning=Reasoning(effort=effort)))  # type: ignore[arg-type]
    return agent


async def forge_system(draft: SystemDraft, model: Optional[str | Model] = None, timeout: float = 240.0, max_turns: int = 40) -> SystemDraft:
    """Run the agent to completion against the given draft. Raises on failure."""
    agent = build_agent(model)
    prompt = (
        f"Build the star system '{draft.star.name}' (star id {draft.star.id}, class {draft.star.cls}) "
        f"for Commander {draft.commander}, who is arriving now. Survey, design, then finalize."
    )
    ctx = ForgeContext(draft=draft)
    try:
        await asyncio.wait_for(Runner.run(agent, prompt, context=ctx, max_turns=max_turns), timeout=timeout)
    except Exception as e:  # noqa: BLE001
        # Keep a usable partial system (e.g. the model ran out of turns or time after
        # building most of it); otherwise surface the failure.
        if draft.bodies and not draft.issues():
            log.warning("forge for %s ended early (%s); keeping partial system", draft.star.id, type(e).__name__)
            draft.log.append("Forge link unstable — committing partial survey")
        else:
            raise
    if not draft.finalized:
        issues = draft.issues()
        if issues:
            raise RuntimeError("Agent stopped before finalizing: " + "; ".join(issues))
        draft.finalized = True
    return draft
