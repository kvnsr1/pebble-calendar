#!/bin/sh
set -eu
if [ "$#" -ne 1 ]; then
  echo 'Usage: scripts/apply-core-calendar-patch.sh /absolute/path/to/core-checkout' >&2
  exit 1
fi
PROJECT_DIR=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
CORE_CHECKOUT=$(CDPATH= cd -- "$1" && pwd)
PATCH_FILE="$PROJECT_DIR/integrations/core-calendar/0001-calendar-attendees.patch"
if [ ! -f "$CORE_CHECKOUT/libpebble3/src/commonMain/kotlin/io/rebble/libpebblecommon/plugin/CalendarPlugin.kt" ]; then
  echo 'The selected checkout does not contain Core CalendarPlugin.kt.' >&2
  exit 1
fi
if git -C "$CORE_CHECKOUT" apply --reverse --check "$PATCH_FILE" 2>/dev/null; then
  echo 'Calendar attendee patch is already applied.'
  exit 0
fi
git -C "$CORE_CHECKOUT" apply --check "$PATCH_FILE"
git -C "$CORE_CHECKOUT" apply "$PATCH_FILE"
echo 'Applied Calendar attendee patch. Run Core Calendar plugin tests before building iOS.'
