"""Onboard survey: the deterministic skeleton of a star system.

Builds a complete, physically plausible system in a few milliseconds using the same
validated SystemDraft the AI tools drive - orbits, planet types by zone, moons, rings,
belts, stations and points of interest, with workable names and descriptions. The
Stellar Forge (dressing.py) then rewrites the *meaning* on top: names, lore, factions,
economy, descriptions and hooks. If the AI is unavailable the survey result is the
system, so the game never waits on a model.
"""
from __future__ import annotations

import hashlib
import math
import random
from typing import Any

from .bible import ECONOMIES
from .draft import AU, GIANTS, KM, SystemDraft

ROMAN = ["I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X", "XI", "XII", "XIII", "XIV"]
SYLL_A = ["ka", "ri", "on", "tha", "va", "lis", "mor", "ae", "dun", "sel", "ix", "or", "ny", "bel", "cor", "hal", "ver", "tam", "ilu", "sha"]
SYLL_B = ["ra", "ne", "vo", "mi", "sa", "lo", "the", "ku", "ren", "da", "xi", "ur", "pe", "lan", "tor", "ga"]
STATION_SUFFIX = ["Station", "Exchange", "Gateway", "Dock", "Spire", "Anchorage", "Terminal", "Yards", "Platform", "Haven"]
TAGLINES = ["Further, together", "Trade makes a brighter tomorrow", "Hold the line", "What the dark gives, we keep",
            "Quiet hands, steady orbit", "Every cargo has a story", "Fuel, fire and fortune", "The Reach provides"]
FACTION_BY_SETTLE = [
    (0.6, ["Nexus Astra Combine", "Kelvar Directorate", "Nexus Astra Combine"]),
    (0.45, ["Free Worlds Accord", "Nexus Astra Combine", "Independent", "Meridian Survey Corps"]),
    (0.3, ["Free Worlds Accord", "Independent", "Veyl Syndicate", "Ashkar Covenant"]),
    (0.05, ["Independent", "Veyl Syndicate", "Meridian Survey Corps", "Ashkar Covenant", "Free Worlds Accord"]),
    (-1.0, ["Unclaimed"]),
]
GOV_BY_FACTION = {
    "Nexus Astra Combine": "Corporate", "Kelvar Directorate": "Directorate", "Free Worlds Accord": "Democracy",
    "Ashkar Covenant": "Theocracy", "Veyl Syndicate": "Anarchy", "Meridian Survey Corps": "Cooperative",
    "Independent": "Patronage", "Unclaimed": "None",
}
PLANET_BLURB = {
    "rocky": "A grey, cratered world of basalt plains and shattered highlands.",
    "barren": "An airless rock scoured by the solar wind; nothing moves on its surface.",
    "desert": "Dune seas and salt flats under a thin, dusty sky.",
    "lava": "A molten world laced with glowing fissures; its night side crawls with firelight.",
    "ice": "A frozen body of nitrogen ice and ancient impact scars.",
    "ocean": "A water world with no land to speak of, wrapped in storm fronts.",
    "earthlike": "Continents, weather and green — a world people would fight over.",
    "jungle": "A hot, wet world where forest canopies hide everything beneath them.",
    "toxic": "A thick yellow atmosphere of sulphur and acid rain.",
    "gas_giant": "A banded gas giant whose storms are larger than most worlds.",
    "ice_giant": "A pale ice giant, methane blue, with faint rings.",
}
HOOKS = [
    "derelict", "beacon", "anomaly", "wide_rings", "shattered_moon", "monastery", "wreck", "signal",
]


def _rng(star_id: str) -> random.Random:
    return random.Random(int(hashlib.sha256(("survey:" + star_id).encode()).hexdigest()[:12], 16))


def _word(rng: random.Random, n: int | None = None) -> str:
    n = n or rng.choice([2, 2, 3])
    parts = [rng.choice(SYLL_A)] + [rng.choice(SYLL_B) for _ in range(n - 1)]
    return "".join(parts).capitalize()


def _faction(settle: float, rng: random.Random) -> str:
    for threshold, options in FACTION_BY_SETTLE:
        if settle > threshold:
            return rng.choice(options)
    return "Unclaimed"


def _planet_type(a: float, hz: tuple[float, float], frost: float, rng: random.Random, cls: str) -> str:
    if a < hz[0] * 0.45:
        return rng.choice(["lava", "lava", "barren", "rocky"])
    if a < hz[0]:
        return rng.choice(["desert", "rocky", "toxic", "barren", "lava"])
    if a <= hz[1]:
        # garden worlds are rare; most habitable-zone planets are still rocks or oceans
        return rng.choices(["rocky", "desert", "ocean", "toxic", "earthlike", "jungle"], [4, 3, 3, 2, 1.2, 0.8])[0]
    if a < frost:
        return rng.choice(["rocky", "barren", "desert", "ice"])
    if a < frost * 3.5:
        return rng.choices(["gas_giant", "ice_giant", "ice", "rocky"], [4, 3, 2, 1])[0]
    return rng.choices(["ice_giant", "ice", "barren", "gas_giant"], [4, 4, 2, 1])[0]


def survey_system(draft: SystemDraft) -> SystemDraft:
    """Fill the draft in place and finalize it. Deterministic for a given star id."""
    star = draft.star
    rng = _rng(star.id)
    draft.survey()
    hz = draft.habitable_zone
    frost = draft.frost_line
    settle = star.settlement
    inhabited = settle > 0.05

    # ---------------------------------------------------------------- profile
    faction = _faction(settle, rng)
    economy = "None" if not inhabited else rng.choice(ECONOMIES if settle > 0.45 else ["Extraction", "Refinery", "Agriculture", "Colony", "Research"])
    security = "high" if settle > 0.7 else "medium" if settle > 0.4 else "low" if inhabited else "none"
    population = int(10 ** (3.5 + settle * 5.5) * rng.uniform(0.5, 1.5)) if inhabited else 0
    danger = 0 if settle > 0.7 else 1 if settle > 0.4 else 2 if settle > 0.2 else 3
    if faction == "Veyl Syndicate":
        danger = min(5, danger + 2)
        security = "low"
    region = star.region or "the Reach"
    lore = (f"Survey drones reached {star.name} ahead of the first settlers; {faction} holds the orbitals now and the {economy.lower()} trade pays for the patrols."
            if inhabited else f"No one has settled {star.name}. The survey record is a drone's flyby and a long silence.")
    draft.set_profile(
        description=f"{draft.survey()['star']['label']} system in {region}; " + ("settled and trafficked." if settle > 0.45 else "a frontier stopover." if inhabited else "empty and quiet."),
        lore=lore, faction=faction, government=GOV_BY_FACTION.get(faction, "None"), economy=economy, security=security,
        population=population, danger=danger, star_flavor=draft.survey()["star"]["label"],
    )
    draft.log.append(f"Onboard survey: {star.name} charted procedurally")

    # ---------------------------------------------------------------- planets
    base_count = {"O": 2, "B": 3, "A": 4, "F": 6, "G": 7, "K": 6, "M": 5, "L": 2, "WD": 2, "NS": 1}[star.cls]
    count = max(0, base_count + rng.randint(-2, 2))
    # first orbit sits around a third of the way to the habitable zone (Mercury-like), so a
    # typical system crosses the habitable zone and the frost line within 4-6 planets
    a = max(draft.min_orbit_au * 2.0, hz[0] * rng.uniform(0.22, 0.45), 0.02)
    outer = max(40 * math.sqrt(draft.lum), 2.0)
    planets: list[dict[str, Any]] = []
    use_roman = rng.random() < 0.6
    for i in range(count):
        if a > outer * 1.8:
            break
        ptype = _planet_type(a, hz, frost, rng, star.cls)
        lo, hi = {"rocky": (1500, 7500), "barren": (500, 5000), "desert": (2000, 8000), "lava": (1200, 7000), "ice": (400, 6000),
                  "ocean": (4000, 10000), "earthlike": (4800, 8500), "jungle": (4500, 8500), "toxic": (3500, 9000),
                  "gas_giant": (35000, 95000), "ice_giant": (15000, 32000)}[ptype]
        radius = rng.uniform(lo, hi)
        name = f"{star.name} {ROMAN[len(planets)]}" if use_roman else _word(rng)
        res = draft.add_planet(
            name=name, planet_type=ptype, orbit_au=round(a, 4), radius_km=round(radius), palette=[],
            description=PLANET_BLURB[ptype],
            atmosphere_color={"ocean": "#7fb8ff", "earthlike": "#8ec5ff", "jungle": "#a8d8a0", "toxic": "#d8d060", "gas_giant": "#f0dcb0", "ice_giant": "#a0d0f0"}.get(ptype),
            cloud_cover=0.55 if ptype in ("ocean", "earthlike") else 0.7 if ptype == "jungle" else 0.3 if ptype == "toxic" else 0.0,
            band_intensity=rng.uniform(0.5, 1.0) if ptype in GIANTS else 0.0,
            axial_tilt_deg=rng.uniform(-30, 30),
        )
        if res.get("ok"):
            planets.append({"id": res["id"], "type": ptype, "au": a, "radius_km": radius, "name": name})
        a *= rng.uniform(1.45, 2.1)

    # ---------------------------------------------------------------- rings, moons, belts
    hook = rng.choice(HOOKS)
    for p in planets:
        if p["type"] in GIANTS and rng.random() < (0.45 if p["type"] == "ice_giant" else 0.35):
            r = p["radius_km"]
            wide = hook == "wide_rings"
            draft.add_rings(p["id"], inner_km=round(r * rng.uniform(1.3, 1.7)), outer_km=round(r * (rng.uniform(3.6, 4.4) if wide else rng.uniform(2.0, 3.2))),
                            color=rng.choice(["#d6dbe0", "#c9b8a0", "#b8c8d8", "#ddd2c0"]), opacity=rng.uniform(0.6, 0.9))
            if wide:
                hook = "used"
        n_moons = 0
        if p["type"] in GIANTS:
            n_moons = rng.randint(1, 4)
        elif p["radius_km"] > 3000 and rng.random() < 0.45:
            n_moons = 1
        orbit = p["radius_km"] * rng.uniform(4.0, 7.0)
        for m in range(n_moons):
            mtype = rng.choice(["barren", "ice", "rocky", "barren", "ice"])
            moon_desc = "A small cratered moon." if mtype != "ice" else "An ice moon riddled with geysers."
            if hook == "shattered_moon":
                moon_desc = "A moon split by some ancient impact; its two halves drift a few hundred kilometres apart, tethered by a rubble bridge."
                hook = "used"
            draft.add_moon(parent_id=p["id"], name=f"{p['name']} {chr(ord('a') + m)}" if use_roman else _word(rng, 2),
                           moon_type=mtype, orbit_km=round(orbit), radius_km=round(rng.uniform(300, min(2500, p["radius_km"] * 0.3))),
                           palette=[], description=moon_desc, atmosphere_color=None)
            orbit *= rng.uniform(1.6, 2.4)
    if planets and rng.random() < 0.35:
        gap = rng.choice(planets)
        draft.add_belt(name=f"{star.name} Belt", orbit_au=round(gap["au"] * 1.35, 3), width_au=round(gap["au"] * 0.12, 3),
                       composition=rng.choice(["rocky", "metallic", "icy", "carbonaceous"]))

    # ---------------------------------------------------------------- stations
    if inhabited and planets:
        n_st = 1 if settle <= 0.45 else rng.randint(2, 3) if settle <= 0.7 else rng.randint(2, 4)
        if settle <= 0.3 and rng.random() < 0.4:
            n_st = 0 if settle < 0.2 else 1
        hosts = sorted(planets, key=lambda p: (p["type"] in ("earthlike", "ocean", "jungle", "rocky", "desert"), rng.random()), reverse=True)
        for i in range(min(n_st, len(hosts))):
            host = hosts[i]
            kind = "outpost" if settle <= 0.3 else rng.choice(["spire", "ring", "spire"]) if settle > 0.5 else rng.choice(["outpost", "spire"])
            services = ["refuel", "repair", "market"] + (["cartographics", "shipyard", "outfitting", "missions"] if settle > 0.45 else ["missions"])
            draft.add_station(name=f"{_word(rng)} {rng.choice(STATION_SUFFIX)}", host_id=host["id"], station_type=kind,
                              orbit_km=round(host["radius_km"] * rng.uniform(1.8, 4.0) + 400), faction=faction if i == 0 or rng.random() < 0.7 else _faction(settle, rng),
                              economy=economy if economy != "None" else "Extraction", services=services, tagline=rng.choice(TAGLINES),
                              description=f"A {kind} station in orbit over {host['name']}.")

    # ---------------------------------------------------------------- points of interest (the hook)
    if planets:
        host = rng.choice(planets)
        if hook in ("derelict", "wreck"):
            draft.add_poi(name=rng.choice(["Drifting Hulk", "Cold Freighter", "Silent Arkship"]), kind=hook, host_id=host["id"],
                          distance_km=round(host["radius_km"] * 6), description="A hull with no transponder and no heat, turning slowly in the dark.",
                          reward_credits=rng.randint(2000, 8000))
        elif hook in ("beacon", "signal"):
            draft.add_poi(name=rng.choice(["Pulsing Beacon", "Unidentified Signal Source", "Old Relay"]), kind=hook, host_id=host["id"],
                          distance_km=round(host["radius_km"] * 8), description="A repeating transmission in an obsolete cipher.",
                          reward_credits=rng.randint(1500, 5000))
        elif hook == "anomaly":
            draft.add_poi(name="Gravitational Anomaly", kind="anomaly", host_id=host["id"], distance_km=round(host["radius_km"] * 12),
                          description="Sensors disagree about where this is. Space here is slightly wrong.", reward_credits=rng.randint(4000, 12000))
        elif hook == "monastery" and not inhabited:
            draft.add_poi(name="Covenant Hermitage", kind="beacon", host_id=host["id"], distance_km=round(host["radius_km"] * 5),
                          description="A sealed monastery dome on the ice. Someone keeps the beacon lit.", reward_credits=2500)
    draft.finalize()
    return draft
