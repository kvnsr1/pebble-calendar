#!/bin/sh
set -eu
. "$(dirname "$0")/env.sh"
cd "$PROJECT_DIR"
export CALENDAR_DEMO=0
if [ "${1:-}" = '--demo' ]; then
  export CALENDAR_DEMO=1
elif [ "$#" -ne 0 ]; then
  echo 'Usage: scripts/build.sh [--demo]' >&2; exit 1
fi
pebble clean
pebble build
python3 scripts/package-pbw.py
