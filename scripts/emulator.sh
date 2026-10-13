#!/bin/sh
set -eu
. "$(dirname "$0")/env.sh"
cd "$PROJECT_DIR"
if [ ! -f dist/pebble-calendar-demo.pbw ]; then
  echo 'Build the synthetic demo first: ./scripts/build.sh --demo' >&2; exit 1
fi
exec pebble install --emulator emery --vnc dist/pebble-calendar-demo.pbw
