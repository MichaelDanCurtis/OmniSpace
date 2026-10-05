# OmniSpace

A cinematic space flight game in the spirit of Elite Dangerous, where **an AI agent builds every new star system the moment you jump into it**.

The galaxy's star map is fixed and deterministic (positions, names and stellar classes never change, like Elite's Stellar Forge). What orbits each star is created live by the **Stellar Forge**, an OpenAI Agents SDK agent that uses a validated tool system (`survey_star`, `add_planet`, `add_moon`, `add_rings`, `add_station`, …). Its tool calls appear as telemetry while you're in the hyperspace tunnel. Each system is saved to a shared galactic record, so it stays the same on every later visit.

```
Browser (React + Three.js)                      Python (FastAPI + OpenAI Agents SDK)
┌────────────────────────────────┐   HTTP   ┌──────────────────────────────────────┐
│ deterministic galaxy (2e5+ ly) │ ───────► │ POST /api/systems/{id}/generate      │
│ flight model / SC / hyperspace │          │   Stellar Forge agent ──► tools ──►  │
│ renderer, HUD, maps, stations  │ ◄─────── │   SystemDraft (physics + canon       │
│ procedural fallback generator  │  polling │   validation) ──► SQLite record      │
└────────────────────────────────┘          └──────────────────────────────────────┘
```

## Quick start (macOS)

```bash
cd "/Volumes/Main External/OmniSpace"
./start.sh
```

`start.sh` creates a Python virtualenv, installs the server, creates `.env` from `.env.example` on first run, and opens <http://localhost:8000>. The game client is prebuilt in `client/dist`, so you don't need Node to play.

**Turn on AI generation:** put your key in `.env`:

```
OPENAI_API_KEY=sk-...
OMNISPACE_MODEL=            # optional; blank = Agents SDK default model
```

> The Agents SDK uses an **OpenAI Platform API key**, which is billed through platform.openai.com. A ChatGPT Plus/Pro subscription does not include API access.

If there's no key, the game still runs and every system comes from the onboard procedural generator. To try the full agent pipeline and hyperspace telemetry without any API calls, set `OMNISPACE_AI=scripted` in `.env`.

Needs Python 3.10+ (`brew install python@3.12`). Node 20+ is only needed if you change the client.

## How to play

| Key | Action |
| --- | --- |
| Mouse (click the view) / arrow keys | Pitch & yaw |
| A / D | Roll |
| W / S, X, C | Throttle up/down, full stop, 75% (optimal turn) |
| Q / E, R / F | Strafe, vertical thrusters |
| Tab or Space | Boost |
| J | Supercruise engage / drop |
| H | Hyperspace jump to the next route hop |
| T / Shift+T | Cycle nav targets |
| L | Request docking / launch |
| P | Autopilot: flies to the target, drops from supercruise and docks. With no target and a route plotted, it aligns and jumps |
| Z | Flight assist (STABILIZED) on/off |
| G | Landing gear |
| M / N | Galaxy map / system map |
| 1–5 | Dashboard tabs: NAV radar, COM log, SHIP, MAP nav list, SYS |
| Esc | Release mouse, then pause |

**Loop:** dock at Tanara Station → take contracts and trade commodities → open the galaxy map (M), pick a star and **Plot route** → launch, center the violet jump marker, press **H** → the Stellar Forge charts the new system while you're in hyperspace → scan planets by flying close in supercruise, investigate POIs, scoop fuel from K/G/B/F/O/A/M stars → sell exploration data at any station with Cartographics. First discoveries pay 50% more.

Supercruise speed is limited by gravity wells and by your distance to the target. Hold ~75% throttle on approach and you'll drop out next to stations automatically.

## Project layout

```
client/                      React 19 + Three.js (Vite, TypeScript)
  src/game/galaxy.ts         deterministic sector-based galaxy, A* route plotting
  src/game/procgen.ts        procedural fallback system generator
  src/game/kelvar.ts         hand-authored home system (the concept-art scene)
  src/game/engine.ts         game loop, flight model, docking, economy, HUD bridge
  src/game/render/*          planet/ring/star shaders, stations, ships, asteroids,
                             cockpit, hyperspace tunnel, skybox, space dust
  src/ui/*                   HUD, dashboard, galaxy map, system map, station menu
server/
  omnispace_server/forge.py  Stellar Forge agent + function tools (Agents SDK)
  omnispace_server/draft.py  SystemDraft: physics/canon validation behind every tool
  omnispace_server/bible.py  universe bible + agent directive (edit to change tone/canon)
  omnispace_server/main.py   FastAPI API + static hosting of client/dist
  omnispace_server/scripted.py  offline stand-in model (tests / demo)
  tests/                     pytest suite (draft rules, real SDK loop, API flow)
```

## The Stellar Forge tool system

The agent never writes JSON for the game. It calls tools that change a `SystemDraft`, and each tool checks the physics before accepting anything:

* `survey_star` returns the fixed stellar data, habitable zone, frost line, settlement guidance (core / frontier / deep space) and summaries of already-charted neighbors for continuity.
* `add_planet` rejects orbits inside the star's danger zone and orbits closer than 18% to a neighbor. It also refuses habitable worlds outside the habitable zone, lava worlds that are too cold and ice worlds that are too warm. Radii outside a type's range are clamped, with a warning.
* `add_moon`, `add_rings`, `add_station` and `add_point_of_interest` check clearances against the host body, its rings and its other moons.
* `finalize_system` won't run until the profile is set, the system has bodies, and core systems have at least one station. The run stops as soon as it succeeds.

Errors come back as actionable text so the model can correct itself. If the agent fails or times out, the client falls back to the procedural generator and locks that result into the record, which keeps the galaxy consistent.

Edit `server/omnispace_server/bible.py` to change the factions, tone and design rules.

## Development

```bash
./dev.sh                      # API with --reload + Vite dev server (hot reload) on :5173
cd server && python -m pytest # backend tests
cd client && npm run build    # rebuild client/dist
curl -X DELETE localhost:8000/api/systems/<id>   # forget a system so it is re-forged
```

Generated systems live in `data/galaxy.sqlite3`. Delete the file to reset the galaxy. Your commander save is in the browser's localStorage; use **Esc → Reset commander** to clear it.

## Why web tech instead of Unreal

Unreal was the suggestion. I went with Three.js + React + Python because it could be built, run and verified end to end in one session: every screenshot during development came from the real game in a headless browser. It also runs anywhere with no engine install, and it fits your preference for React frontends with Python backends. The architecture carries over to an engine later: the galaxy math, the `StarSystem` JSON contract and the whole Stellar Forge backend are engine-agnostic. An Unreal or Godot client would call the same API.
