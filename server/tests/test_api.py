import os
import time

os.environ["OMNISPACE_AI"] = "scripted"

from fastapi.testclient import TestClient  # noqa: E402


def test_generate_flow(tmp_path):
    os.environ["OMNISPACE_DB"] = str(tmp_path / "g.sqlite3")
    import importlib
    import omnispace_server.main as main
    importlib.reload(main)
    from tests.fixtures import star

    with TestClient(main.app) as c:
        assert c.get("/api/health").json()["ai"]["enabled"]
        s = star(settlement=0.6).model_dump()
        r = c.post(f"/api/systems/{s['id']}/generate", json={"star": s, "neighbors": [], "commander": "Michael"}).json()
        assert r["status"] in ("generating", "ready")
        for _ in range(100):
            j = c.get(f"/api/systems/{s['id']}/job").json()
            if j["status"] != "generating":
                break
            time.sleep(0.1)
        assert j["status"] == "ready", j
        assert j["system"]["discoveredBy"] == "Michael"
        assert len(j["log"]) > 3
        assert c.get("/api/systems").json()[0]["id"] == s["id"]
        # lock-in of procedural systems is ignored when one already exists
        assert c.put(f"/api/systems/{s['id']}", json=j["system"]).json()["stored"] is False


def test_static_path_traversal_blocked(tmp_path):
    os.environ["OMNISPACE_DB"] = str(tmp_path / "g.sqlite3")
    import importlib
    import omnispace_server.main as main
    importlib.reload(main)
    with TestClient(main.app) as c:
        for url in ("/..%2F..%2Fstart.sh", "/..%2F..%2F.env.example", "/../../server/requirements.txt"):
            r = c.get(url)
            assert "openai-agents" not in r.text and "#!/usr/bin/env bash" not in r.text
