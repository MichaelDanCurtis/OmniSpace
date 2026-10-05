#!/usr/bin/env bash
# OmniSpace launcher (macOS / Linux).
# Sets up a Python virtualenv, installs the Stellar Forge server, builds the
# client if needed, starts everything and opens the game in your browser.
set -euo pipefail
cd "$(dirname "$0")"
PORT="${PORT:-8000}"

PY=""
for cand in python3.13 python3.12 python3.11 python3.10 python3; do
  if command -v "$cand" >/dev/null 2>&1; then
    if "$cand" -c 'import sys; sys.exit(0 if sys.version_info >= (3, 10) else 1)' 2>/dev/null; then PY="$cand"; break; fi
  fi
done
if [ -z "$PY" ]; then
  echo "OmniSpace needs Python 3.10+.  On macOS:  brew install python@3.12"
  exit 1
fi

if [ ! -d server/.venv ]; then
  echo "▸ Creating Python virtualenv ($PY)…"
  "$PY" -m venv server/.venv
fi
# shellcheck disable=SC1091
source server/.venv/bin/activate
if [ ! -f server/.venv/.installed ] || [ server/requirements.txt -nt server/.venv/.installed ]; then
  echo "▸ Installing server dependencies…"
  pip install --upgrade pip >/dev/null
  pip install -r server/requirements.txt
  touch server/.venv/.installed
fi

if [ ! -f client/dist/index.html ]; then
  if command -v npm >/dev/null 2>&1; then
    echo "▸ Building the game client…"
    (cd client && npm install && npm run build)
  else
    echo "client/dist is missing and npm is not installed. Install Node.js 20+ (brew install node) and re-run."
    exit 1
  fi
fi

if [ ! -f .env ]; then
  cp .env.example .env
  echo "▸ Created .env — add your OPENAI_API_KEY there to enable AI-generated systems."
fi

URL="http://localhost:${PORT}"
echo "▸ OmniSpace running at ${URL}  (Ctrl+C to stop)"
( sleep 2; (command -v open >/dev/null && open "$URL") || (command -v xdg-open >/dev/null && xdg-open "$URL") || true ) &
cd server
exec python -m uvicorn omnispace_server.main:app --host 127.0.0.1 --port "$PORT"
