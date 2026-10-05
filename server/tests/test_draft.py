import math
from omnispace_server.draft import SystemDraft, AU
from tests.fixtures import star


def test_survey_zones():
    d = SystemDraft(star=star())
    s = d.survey()
    lo, hi = s["zones_au"]["habitable_zone"]
    assert 0.9 < lo < 1.0 and 1.3 < hi < 1.4
    assert "CORE" in s["settlement_guidance"]


def test_habitable_rules_and_spacing():
    d = SystemDraft(star=star())
    bad = d.add_planet("Hot", "earthlike", 0.2, 6000, [], "x", None, 0.5, 0, 10)
    assert not bad["ok"] and "habitable zone" in bad["error"]
    ok = d.add_planet("Home", "earthlike", 1.0, 6000, ["#112233", "#445566", "#778899"], "x", "#88aaff", 0.5, 0, 10)
    assert ok["ok"] and ok["id"] == "home"
    close = d.add_planet("Twin", "rocky", 1.05, 3000, [], "x", None, 0, 0, 0)
    assert not close["ok"] and "too close" in close["error"]
    clamped = d.add_planet("Big", "rocky", 1.6, 99999, [], "x", None, 0, 0, 0)
    assert clamped["ok"] and clamped["warnings"]


def test_rings_moons_stations_finalize():
    d = SystemDraft(star=star())
    assert not d.finalize()["ok"]
    d.set_profile("d", "l", "Nexus Astra Combine", "Corporate", "Industrial", "high", 1000, 1, "yellow star")
    g = d.add_planet("Giant", "gas_giant", 5.0, 60000, [], "x", "#ffeecc", 0, 0.9, 3)
    assert g["ok"]
    assert not d.add_rings("giant", 50000, 90000, "#ffffff", 0.7)["ok"]  # inner too small
    assert d.add_rings("giant", 80000, 160000, "#ffffff", 0.7)["ok"]
    m = d.add_moon("giant", "Moonlet", "ice", 100000, 1000, [], "x", None)
    assert not m["ok"]  # inside rings
    assert d.add_moon("giant", "Moonlet", "ice", 400000, 1000, [], "x", None)["ok"]
    assert not d.finalize()["ok"]  # core system needs a station
    st = d.add_station("Hub", "giant", "spire", 1000, "Nexus Astra Combine", "Industrial", ["market"], "t", "d")
    assert st["ok"] and st["warnings"]  # orbit raised
    assert d.stations[0].orbitM > 160000 * 1000
    assert "refuel" in d.stations[0].services
    assert d.finalize()["ok"]
    built = d.build("test-model")
    assert built.source == "ai" and len(built.bodies) == 2 and built.bodies[0].rings is not None


def test_deep_space_guidance():
    d = SystemDraft(star=star(settlement=0.0))
    assert "DEEP SPACE" in d.survey()["settlement_guidance"]
