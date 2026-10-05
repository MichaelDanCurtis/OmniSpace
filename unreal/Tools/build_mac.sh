#!/usr/bin/env bash
# Build the OmniSpace editor target on macOS and (optionally) run the editor bootstrap.
#   Tools/build_mac.sh            # build editor
#   Tools/build_mac.sh setup      # build + create map/materials
#   Tools/build_mac.sh game       # build + launch standalone game window
set -euo pipefail
cd "$(dirname "$0")/.."
PROJ="$PWD/OmniSpace.uproject"
UE="${UE_ROOT:-}"
if [ -z "$UE" ]; then
  for d in "/Users/Shared/Epic Games"/UE_5.* /Volumes/*/Epic\ Games/UE_5.* /Volumes/*/UE_5.*; do
    [ -d "$d/Engine" ] && UE="$d"
  done
fi
[ -n "$UE" ] || { echo "Unreal Engine not found; set UE_ROOT"; exit 1; }
echo "▸ Engine: $UE"
"$UE/Engine/Build/BatchFiles/Mac/Build.sh" OmniSpaceEditor Mac Development -project="$PROJ" -waitmutex
case "${1:-}" in
  setup)
    "$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PROJ" -run=pythonscript -script="$PWD/Tools/ue_setup.py" -unattended -nop4 -nosplash
    ;;
  game)
    "$UE/Engine/Binaries/Mac/UnrealEditor" "$PROJ" -game -windowed -resx=1600 -resy=900 -log "${@:2}"
    ;;
esac
