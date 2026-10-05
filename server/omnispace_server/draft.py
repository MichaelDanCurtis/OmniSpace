"""SystemDraft: the validated, physics-aware builder the AI tools operate on.

Every mutation returns a dict that is sent back to the model, either
{"ok": True, ...} or {"ok": False, "error": "..."} with an actionable fix.
Keeping this logic free of any LLM code makes it unit-testable.
"""
from __future__ import annotations

import hashlib
import math
import random
import re
from dataclasses import dataclass, field
from datetime import datetime, timezone
from typing import Any, Optional

from .bible import CANON_FACTIONS, ECONOMIES, GOVERNMENTS, SERVICES
from .schema import POI, Belt, Body, Neighbor, Rings, StarDescriptor, StarSystem, Station, SystemProfile

AU = 149_597_870_700.0
KM = 1000.0
SOLAR_RADIUS_M = 695_700_000.0

CLASS_LABEL = {
    "O": "O-type blue supergiant", "B": "B-type blue-white star", "A": "A-type white star",
    "F": "F-type yellow-white star", "G": "G-type yellow star", "K": "K-type orange dwarf",
    "M": "M-type red dwarf", "L": "L-type brown dwarf", "WD": "White dwarf", "NS": "Neutron star",
}

RADIUS_KM = {
    "rocky": (1500, 7500), "barren": (500, 5000), "desert": (2000, 8000), "lava": (1200, 7000),
    "ice": (400, 6000), "ocean": (4000, 10000), "earthlike": (4800, 8500), "jungle": (4500, 8500),
    "toxic": (3500, 9000), "gas_giant": (35000, 95000), "ice_giant": (15000, 32000),
}
DEFAULT_PALETTE = {
    "rocky": ["#5b524a", "#8a7d6e", "#b8ab98"], "barren": ["#3e3b39", "#6b6662", "#9c9590"],
    "desert": ["#7a4a25", "#c18a4d", "#e8c48a"], "lava": ["#1a0d0a", "#3a1a12", "#ff5a1f"],
    "ice": ["#8fa6b8", "#cfe0ea", "#ffffff"], "ocean": ["#0b2a4a", "#1d5c8a", "#6fb6d8"],
    "earthlike": ["#123d6b", "#3b7a3a", "#c9b98a"], "jungle": ["#0f3a2a", "#2f6e2a", "#8ab04a"],
    "toxic": ["#4a5a1a", "#9aa83a", "#e2d27a"], "gas_giant": ["#7a5a3a", "#c8a070", "#efe0c0"],
    "ice_giant": ["#4a7a9a", "#8ab8d4", "#e0f0f8"],
}
HEX = re.compile(r"^#[0-9a-fA-F]{6}$")
GIANTS = {"gas_giant", "ice_giant"}
HABITABLE = {"earthlike", "ocean", "jungle"}


def slug(name: str) -> str:
    s = re.sub(r"[^a-z0-9]+", "-", name.lower()).strip("-")
    return s or "body"


def eq_temp(lum: float, au: float) -> float:
    return 278.0 * (max(lum, 1e-6) ** 0.25) / math.sqrt(max(au, 1e-4))


@dataclass
class SystemDraft:
    star: StarDescriptor
    neighbors: list[Neighbor] = field(default_factory=list)
    neighbor_summaries: list[dict[str, Any]] = field(default_factory=list)
    commander: str = "Unknown"
    profile: Optional[SystemProfile] = None
    star_flavor: Optional[str] = None
    bodies: list[Body] = field(default_factory=list)
    stations: list[Station] = field(default_factory=list)
    belts: list[Belt] = field(default_factory=list)
    pois: list[POI] = field(default_factory=list)
    log: list[str] = field(default_factory=list)
    finalized: bool = False
    surveyed: bool = False

    def __post_init__(self) -> None:
        seed = int(hashlib.sha256(self.star.id.encode()).hexdigest()[:12], 16)
        self.rng = random.Random(seed)

    # ------------------------------------------------------------ derived
    @property
    def lum(self) -> float:
        return max(self.star.luminosity, 1e-5)

    @property
    def star_radius_au(self) -> float:
        return self.star.radiusSolar * SOLAR_RADIUS_M / AU

    @property
    def min_orbit_au(self) -> float:
        base = max(self.star_radius_au * 5, 0.012)
        if self.star.cls == "NS":
            base = max(base, 0.2)
        return base

    @property
    def habitable_zone(self) -> tuple[float, float]:
        return (math.sqrt(self.lum / 1.1), math.sqrt(self.lum / 0.53))

    @property
    def frost_line(self) -> float:
        return 2.7 * math.sqrt(self.lum)

    def settlement_guidance(self) -> str:
        s = self.star.settlement
        if s > 0.6:
            return "CORE: well settled, heavy traffic. 2-4 stations, canon factions, high security."
        if s > 0.3:
            return "INNER FRONTIER: settled but contested. 1-2 stations, mixed factions, medium security."
        if s > 0.05:
            return "FRONTIER: sparse. 0-1 small outposts (station_type 'outpost'), low security, independents/pirates/surveyors."
        return "DEEP SPACE: no permanent human presence. No stations. Faction 'Unclaimed', population 0, security 'none'. A derelict, beacon or anomaly POI is welcome."

    def _unique_id(self, name: str) -> str:
        base = slug(name)
        existing = {b.id for b in self.bodies} | {s.id for s in self.stations} | {b.id for b in self.belts} | {p.id for p in self.pois}
        i, cand = 2, base
        while cand in existing or cand == "star":
            cand = f"{base}-{i}"
            i += 1
        return cand

    def _palette(self, palette: list[str] | None, ptype: str) -> list[str]:
        pal = [c for c in (palette or []) if isinstance(c, str) and HEX.match(c)]
        default = DEFAULT_PALETTE[ptype]
        while len(pal) < 3:
            pal.append(default[len(pal)])
        return pal[:3]

    def _find_body(self, body_id: str) -> Optional[Body]:
        b = next((x for x in self.bodies if x.id == body_id), None)
        if b is None:
            b = next((x for x in self.bodies if x.name.lower() == body_id.lower()), None)
        return b

    def _note(self, line: str) -> None:
        self.log.append(line)

    # ------------------------------------------------------------ tools
    def survey(self) -> dict[str, Any]:
        self.surveyed = True
        hz = self.habitable_zone
        self._note(f"Stellar survey: {self.star.name} — {CLASS_LABEL[self.star.cls]}, {int(self.star.temperature)} K")
        return {
            "star": {
                "name": self.star.name,
                "class": self.star.cls,
                "label": CLASS_LABEL[self.star.cls],
                "temperature_k": round(self.star.temperature),
                "radius_solar": round(self.star.radiusSolar, 5),
                "mass_solar": round(self.star.massSolar, 3),
                "luminosity_solar": round(self.star.luminosity, 5),
                "age_gyr": self.star.ageGyr,
            },
            "zones_au": {
                "min_safe_orbit": round(self.min_orbit_au, 4),
                "habitable_zone": [round(hz[0], 4), round(hz[1], 4)],
                "frost_line": round(self.frost_line, 3),
                "practical_outer_limit": round(max(40 * math.sqrt(self.lum), 2.0), 2),
            },
            "region": self.star.region,
            "settlement_index": round(self.star.settlement, 2),
            "settlement_guidance": self.settlement_guidance(),
            "neighbors": self.neighbor_summaries
            or [{"name": n.name, "distance_ly": round(n.distanceLy, 1), "charted": False} for n in self.neighbors[:8]],
            "canon_factions": list(CANON_FACTIONS.keys()),
            "economies": ECONOMIES,
            "governments": GOVERNMENTS,
            "station_services": SERVICES,
        }

    def set_profile(self, description: str, lore: str, faction: str, government: str, economy: str,
                    security: str, population: int, danger: int, star_flavor: str) -> dict[str, Any]:
        if security not in ("none", "low", "medium", "high"):
            return {"ok": False, "error": "security must be none|low|medium|high"}
        if economy not in ECONOMIES and economy != "None":
            return {"ok": False, "error": f"economy must be one of {ECONOMIES} or 'None'"}
        self.profile = SystemProfile(
            description=description.strip()[:400], lore=lore.strip()[:900], faction=faction.strip()[:60] or "Unclaimed",
            government=government.strip()[:40] or "None", economy=economy, security=security,  # type: ignore[arg-type]
            population=max(0, int(population)), danger=max(0, min(5, int(danger))),
        )
        self.star_flavor = (star_flavor or CLASS_LABEL[self.star.cls]).strip()[:120]
        self._note(f"System profile: {self.profile.faction} · {self.profile.economy} · security {security}")
        return {"ok": True}

    def add_planet(self, name: str, planet_type: str, orbit_au: float, radius_km: float, palette: list[str],
                   description: str, atmosphere_color: Optional[str], cloud_cover: float, band_intensity: float,
                   axial_tilt_deg: float) -> dict[str, Any]:
        if planet_type not in RADIUS_KM:
            return {"ok": False, "error": f"planet_type must be one of {list(RADIUS_KM)}"}
        if len([b for b in self.bodies if b.kind == "planet"]) >= 14:
            return {"ok": False, "error": "System already has 14 planets; that is the maximum."}
        if orbit_au < self.min_orbit_au:
            return {"ok": False, "error": f"orbit_au {orbit_au} is inside the star's danger zone; minimum is {self.min_orbit_au:.4f} AU."}
        if orbit_au > 400:
            return {"ok": False, "error": "orbit_au must be <= 400."}
        for b in self.bodies:
            if b.kind != "planet":
                continue
            a = b.orbitM / AU
            ratio = max(a, orbit_au) / min(a, orbit_au)
            if ratio < 1.18:
                return {"ok": False, "error": f"Orbit {orbit_au} AU is too close to {b.name} at {a:.3f} AU. Neighbouring orbits must differ by at least 18% (ideally 40%+)."}
        for belt in self.belts:
            if belt.parent == "star" and abs(belt.radiusM / AU - orbit_au) < (belt.widthM / AU) * 0.6:
                return {"ok": False, "error": f"Orbit overlaps the asteroid belt {belt.name}."}
        temp = eq_temp(self.lum, orbit_au)
        if planet_type in HABITABLE and not (175 <= temp <= 360):
            hz = self.habitable_zone
            return {"ok": False, "error": f"A {planet_type} world needs liquid water: equilibrium temperature here is {temp:.0f} K. Place it in the habitable zone {hz[0]:.3f}-{hz[1]:.3f} AU, or choose another type."}
        if planet_type == "lava" and temp < 450:
            return {"ok": False, "error": f"Equilibrium temperature at {orbit_au} AU is {temp:.0f} K, too cold for a lava world. Move it inward or choose rocky/barren."}
        if planet_type == "ice" and temp > 260:
            return {"ok": False, "error": f"Equilibrium temperature at {orbit_au} AU is {temp:.0f} K, too warm for an ice world."}
        lo, hi = RADIUS_KM[planet_type]
        warnings: list[str] = []
        r = float(radius_km)
        if not (lo * 0.7 <= r <= hi * 1.3):
            r = min(max(r, lo), hi)
            warnings.append(f"radius_km clamped to {r:.0f} (range for {planet_type}: {lo}-{hi}).")
        pid = self._unique_id(name)
        giant = planet_type in GIANTS
        grav = (r * KM / 6.371e6) * (2.2 if giant else 0.9)
        body = Body(
            id=pid, name=name.strip()[:48], kind="planet", type=planet_type,  # type: ignore[arg-type]
            parent="star", orbitM=orbit_au * AU, phase=self.rng.uniform(0, math.tau),
            inclination=self.rng.uniform(-0.06, 0.06), radiusM=r * KM, palette=self._palette(palette, planet_type),
            atmosphere=atmosphere_color if atmosphere_color and HEX.match(atmosphere_color) else None,
            clouds=max(0.0, min(1.0, float(cloud_cover))), bands=max(0.0, min(1.0, float(band_intensity))),
            tilt=math.radians(max(-90.0, min(90.0, float(axial_tilt_deg)))),
            temperatureK=round(temp), gravityG=round(min(grav, 4.0) if giant else grav, 2),
            description=description.strip()[:400],
        )
        self.bodies.append(body)
        self._note(f"Planet charted: {body.name} — {planet_type.replace('_', ' ')} at {orbit_au:.2f} AU ({temp:.0f} K)")
        return {"ok": True, "id": pid, "equilibrium_temp_k": round(temp), "warnings": warnings}

    def add_moon(self, parent_id: str, name: str, moon_type: str, orbit_km: float, radius_km: float,
                 palette: list[str], description: str, atmosphere_color: Optional[str]) -> dict[str, Any]:
        parent = self._find_body(parent_id)
        if parent is None or parent.kind != "planet":
            return {"ok": False, "error": f"No planet with id '{parent_id}'. Planet ids: {[b.id for b in self.bodies if b.kind == 'planet']}"}
        if moon_type in GIANTS or moon_type not in RADIUS_KM:
            return {"ok": False, "error": "moon_type must be a solid body type (not a giant)."}
        moons = [b for b in self.bodies if b.parent == parent.id]
        if len(moons) >= 6:
            return {"ok": False, "error": f"{parent.name} already has 6 moons."}
        pr_km = parent.radiusM / KM
        min_orbit = max(pr_km * 2.0, (parent.rings.outerM / KM * 1.15) if parent.rings else 0)
        if orbit_km < min_orbit:
            return {"ok": False, "error": f"orbit_km must be at least {min_orbit:.0f} km (outside the planet and its rings)."}
        hill_km = (parent.orbitM / KM) * (1 / 3) ** (1 / 3) * 0.02 * max(1.0, (pr_km / 6000) ** 1.5) + pr_km * 40
        if orbit_km > hill_km:
            return {"ok": False, "error": f"orbit_km {orbit_km:.0f} is outside the stable region; keep it below {hill_km:.0f} km."}
        for m in moons:
            mo = m.orbitM / KM
            if max(mo, orbit_km) / min(mo, orbit_km) < 1.2:
                return {"ok": False, "error": f"Too close to moon {m.name} at {mo:.0f} km; separate orbits by 20%+."}
        r = float(radius_km)
        warnings: list[str] = []
        if r > pr_km * 0.45:
            r = pr_km * 0.45
            warnings.append(f"radius_km clamped to {r:.0f} (moons must be < 45% of the parent).")
        r = max(r, 50)
        mid = self._unique_id(name)
        temp = parent.temperatureK or 150
        body = Body(
            id=mid, name=name.strip()[:48], kind="moon", type=moon_type,  # type: ignore[arg-type]
            parent=parent.id, orbitM=orbit_km * KM, phase=self.rng.uniform(0, math.tau), inclination=self.rng.uniform(-0.2, 0.2),
            radiusM=r * KM, palette=self._palette(palette, moon_type),
            atmosphere=atmosphere_color if atmosphere_color and HEX.match(atmosphere_color) else None,
            tilt=self.rng.uniform(-0.3, 0.3), temperatureK=temp, gravityG=round(r * KM / 6.371e6 * 0.8, 2),
            description=description.strip()[:400],
        )
        self.bodies.append(body)
        self._note(f"Moon charted: {body.name} orbiting {parent.name}")
        return {"ok": True, "id": mid, "warnings": warnings}

    def add_rings(self, planet_id: str, inner_km: float, outer_km: float, color: str, opacity: float) -> dict[str, Any]:
        p = self._find_body(planet_id)
        if p is None:
            return {"ok": False, "error": f"No body with id '{planet_id}'."}
        r = p.radiusM / KM
        if inner_km < r * 1.15:
            return {"ok": False, "error": f"inner_km must be >= {r * 1.15:.0f} (1.15x the planet radius {r:.0f} km)."}
        if outer_km <= inner_km * 1.08:
            return {"ok": False, "error": "outer_km must be at least 8% larger than inner_km."}
        if outer_km > r * 4.5:
            return {"ok": False, "error": f"outer_km must be <= {r * 4.5:.0f} (4.5x planet radius)."}
        inner_moon = [m for m in self.bodies if m.parent == p.id and m.orbitM / KM < outer_km * 1.1]
        if inner_moon:
            return {"ok": False, "error": f"Moon {inner_moon[0].name} orbits inside these rings; shrink outer_km below {inner_moon[0].orbitM / KM / 1.1:.0f}."}
        p.rings = Rings(innerM=inner_km * KM, outerM=outer_km * KM, color=color if HEX.match(color or "") else "#d0ccc4",
                        opacity=max(0.15, min(1.0, float(opacity))))
        self._note(f"Ring system mapped around {p.name}: {inner_km:,.0f}–{outer_km:,.0f} km")
        return {"ok": True}

    def add_belt(self, name: str, orbit_au: float, width_au: float, composition: str) -> dict[str, Any]:
        if orbit_au < self.min_orbit_au:
            return {"ok": False, "error": f"orbit_au must be >= {self.min_orbit_au:.4f}"}
        width_au = max(0.01, min(float(width_au), orbit_au * 0.5))
        for b in self.bodies:
            if b.kind == "planet" and abs(b.orbitM / AU - orbit_au) < width_au * 0.6:
                return {"ok": False, "error": f"Belt overlaps the orbit of {b.name} ({b.orbitM / AU:.2f} AU)."}
        if len(self.belts) >= 3:
            return {"ok": False, "error": "Maximum of 3 belts."}
        bid = self._unique_id(name)
        self.belts.append(Belt(id=bid, name=name.strip()[:48], parent="star", radiusM=orbit_au * AU, widthM=width_au * AU,
                               composition=(composition or "rocky")[:24]))
        self._note(f"Asteroid belt detected: {name} at {orbit_au:.2f} AU")
        return {"ok": True, "id": bid}

    def add_station(self, name: str, host_id: str, station_type: str, orbit_km: float, faction: str, economy: str,
                    services: list[str], tagline: str, description: str) -> dict[str, Any]:
        host = self._find_body(host_id)
        if host is None:
            return {"ok": False, "error": f"No body with id '{host_id}'. Body ids: {[b.id for b in self.bodies]}"}
        if len(self.stations) >= 5:
            return {"ok": False, "error": "Maximum of 5 stations."}
        if station_type not in ("spire", "ring", "outpost"):
            return {"ok": False, "error": "station_type must be spire|ring|outpost"}
        if economy not in ECONOMIES:
            return {"ok": False, "error": f"economy must be one of {ECONOMIES}"}
        hr = host.radiusM / KM
        min_orbit = max(hr * 1.25, (host.rings.outerM / KM * 1.12) if host.rings else 0, hr + 400)
        warnings: list[str] = []
        if orbit_km < min_orbit:
            orbit_km = min_orbit * 1.1
            warnings.append(f"orbit_km raised to {orbit_km:.0f} to clear the host body/rings.")
        if orbit_km > hr * 30:
            orbit_km = hr * 30
            warnings.append(f"orbit_km lowered to {orbit_km:.0f}.")
        svc = [s for s in services if s in SERVICES] or ["refuel", "repair", "market"]
        if "refuel" not in svc:
            svc.insert(0, "refuel")
        sid = self._unique_id(name)
        st = Station(
            id=sid, name=name.strip()[:48], kind=station_type, parent=host.id, orbitM=orbit_km * KM,  # type: ignore[arg-type]
            phase=self.rng.uniform(0, math.tau), inclination=self.rng.uniform(0.12, 0.3), faction=faction.strip()[:60],
            economy=economy, services=svc, tagline=tagline.strip()[:60], description=description.strip()[:400],
        )
        self.stations.append(st)
        self._note(f"Station signal acquired: {st.name} ({station_type}) above {host.name}")
        return {"ok": True, "id": sid, "warnings": warnings}

    def add_poi(self, name: str, kind: str, host_id: str, distance_km: float, description: str, reward_credits: int) -> dict[str, Any]:
        host = self._find_body(host_id)
        if host is None:
            return {"ok": False, "error": f"No body with id '{host_id}'."}
        if kind not in ("derelict", "signal", "anomaly", "beacon", "wreck"):
            return {"ok": False, "error": "kind must be derelict|signal|anomaly|beacon|wreck"}
        if len(self.pois) >= 4:
            return {"ok": False, "error": "Maximum of 4 points of interest."}
        hr = host.radiusM / KM
        d = max(float(distance_km), hr * 1.5 + 500, (host.rings.outerM / KM * 1.1) if host.rings else 0)
        pid = self._unique_id(name)
        self.pois.append(POI(id=pid, name=name.strip()[:48], kind=kind, parent=host.id, offsetM=d * KM,  # type: ignore[arg-type]
                             phase=self.rng.uniform(0, math.tau), description=description.strip()[:400],
                             reward=max(0, min(50000, int(reward_credits)))))
        self._note(f"Unresolved contact: {name} near {host.name}")
        return {"ok": True, "id": pid}

    def review(self) -> dict[str, Any]:
        return {
            "profile_set": self.profile is not None,
            "planets": [{"id": b.id, "name": b.name, "type": b.type, "orbit_au": round(b.orbitM / AU, 3), "radius_km": round(b.radiusM / KM),
                         "rings": bool(b.rings)} for b in self.bodies if b.kind == "planet"],
            "moons": [{"id": b.id, "name": b.name, "parent": b.parent} for b in self.bodies if b.kind == "moon"],
            "stations": [{"id": s.id, "name": s.name, "host": s.parent} for s in self.stations],
            "belts": [b.name for b in self.belts],
            "pois": [p.name for p in self.pois],
            "issues": self.issues(),
        }

    def issues(self) -> list[str]:
        out: list[str] = []
        if self.profile is None:
            out.append("Call set_system_profile.")
        if not self.bodies and self.star.cls not in ("NS", "L", "O", "WD"):
            out.append("Add at least one planet.")
        if self.star.settlement > 0.6 and not self.stations and self.bodies:
            out.append("This is a CORE system: add at least one station.")
        if self.star.settlement < 0.03 and len(self.stations) > 1:
            out.append("Deep space systems should have at most one (lonely) station.")
        return out

    def finalize(self) -> dict[str, Any]:
        issues = self.issues()
        if issues:
            return {"ok": False, "error": "Cannot finalize yet: " + " ".join(issues)}
        self.finalized = True
        self._note("Cartography complete. Plotting exit vector…")
        return {"ok": True, "message": "System committed to the galactic record."}

    # ------------------------------------------------------------ output
    def build(self, model: Optional[str]) -> StarSystem:
        profile = self.profile or SystemProfile(
            description=f"{CLASS_LABEL[self.star.cls]} system.", lore="Uncharted.", faction="Unclaimed",
            government="None", economy="None", security="none", population=0, danger=1,
        )
        return StarSystem(
            id=self.star.id, name=self.star.name, source="ai", model=model, discoveredBy=self.commander,
            createdAt=datetime.now(timezone.utc).isoformat(), star=self.star,
            starFlavor=self.star_flavor or CLASS_LABEL[self.star.cls], profile=profile,
            bodies=self.bodies, stations=self.stations, belts=self.belts, pois=self.pois,
        )
