"""Pydantic mirror of client/src/game/types.ts.

The AI never writes this JSON directly. It calls validated tools (see forge.py)
and the server assembles the final StarSystem, so the client always receives a
well-formed system.
"""
from __future__ import annotations

from typing import Literal, Optional

from pydantic import BaseModel, Field

StarClass = Literal["O", "B", "A", "F", "G", "K", "M", "L", "WD", "NS"]
PlanetType = Literal[
    "rocky", "barren", "desert", "lava", "ice", "ocean", "earthlike", "jungle", "toxic", "gas_giant", "ice_giant"
]
StationKind = Literal["spire", "ring", "outpost"]
PoiKind = Literal["derelict", "signal", "anomaly", "beacon", "wreck"]
Security = Literal["none", "low", "medium", "high"]


class StarDescriptor(BaseModel):
    id: str
    name: str
    cls: StarClass
    pos: list[float]
    temperature: float
    radiusSolar: float
    massSolar: float
    luminosity: float
    color: str
    ageGyr: float
    settlement: float
    region: str


class Rings(BaseModel):
    innerM: float
    outerM: float
    color: str
    opacity: float


class Body(BaseModel):
    id: str
    name: str
    kind: Literal["planet", "moon"]
    type: PlanetType
    parent: str
    orbitM: float
    phase: float
    inclination: float
    radiusM: float
    palette: list[str]
    atmosphere: Optional[str] = None
    clouds: float = 0
    bands: float = 0
    tilt: float = 0
    rings: Optional[Rings] = None
    temperatureK: Optional[float] = None
    gravityG: Optional[float] = None
    description: str = ""


class Station(BaseModel):
    id: str
    name: str
    kind: StationKind
    parent: str
    orbitM: float
    phase: float
    inclination: float
    faction: str
    economy: str
    services: list[str]
    tagline: str
    description: str


class Belt(BaseModel):
    id: str
    name: str
    parent: str
    radiusM: float
    widthM: float
    composition: str


class POI(BaseModel):
    id: str
    name: str
    kind: PoiKind
    parent: str
    offsetM: float
    phase: float
    description: str
    reward: int


class SystemProfile(BaseModel):
    description: str
    lore: str
    faction: str
    government: str
    economy: str
    security: Security
    population: int
    danger: int


class StarSystem(BaseModel):
    id: str
    name: str
    source: Literal["ai", "procedural", "authored"]
    model: Optional[str] = None
    discoveredBy: Optional[str] = None
    createdAt: Optional[str] = None
    star: StarDescriptor
    starFlavor: Optional[str] = None
    profile: SystemProfile
    bodies: list[Body] = Field(default_factory=list)
    stations: list[Station] = Field(default_factory=list)
    belts: list[Belt] = Field(default_factory=list)
    pois: list[POI] = Field(default_factory=list)


class Neighbor(BaseModel):
    id: str
    name: str
    distanceLy: float
    cls: Optional[str] = None


class GenerateRequest(BaseModel):
    star: StarDescriptor
    neighbors: list[Neighbor] = Field(default_factory=list)
    commander: str = "Unknown"
    prefetch: bool = False
