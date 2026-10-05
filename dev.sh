#!/usr/bin/env bash
# Development mode: Python API with auto-reload + Vite dev server with hot reload.
set -euo pipefail
cd "$(dirname "$0")"
[ -d server/.venv ] || { echo "Run ./start.sh once first to create the virtualenv."; exit 1; }
# shellcheck disable=SC1091
source server/.venv/bin/activate
(cd server && python -m uvicorn omnispace_server.main:app --reload --port 8000) &
API=$!
trap 'kill $API 2>/dev/null' EXIT
cd client
[ -d node_modules ] || npm install
npm run dev -- --open
