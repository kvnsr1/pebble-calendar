#!/bin/sh
set -eu
. "$(dirname "$0")/env.sh"
cd "$PROJECT_DIR"
if [ "$#" -ne 1 ]; then
  echo 'Usage: scripts/install.sh PHONE_IP (LAN Dev Connection)' >&2; exit 1
fi
exec pebble install --phone "$1" dist/pebble-calendar.pbw
