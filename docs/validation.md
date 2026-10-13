# Validation record

Validated 2026-10-13 in the cloud execution workspace using Pebble Tool 5.0.40, SDK 4.33.1, Python 3.12 and Node 24.19. All calendar records used were synthetic.

## Completed

- Native emery compilation of live and demo modes; complete permission-packaged PBWs generated.
- Live memory report: 20,804 bytes RAM footprint / 128 KiB, 4,237 bytes resources / 256 KiB. Runtime heap consumption (e.g. detail text layers) is additional; no long-term battery benchmark has been performed.
- Fourteen Node tests covering source validation and live provider discovery, fixed error codes, optional fields, Unicode bounds, complete snapshots, acknowledgments, coalescing and stale callbacks.
- Host C tests for overlap/day membership, midnight-exclusive ends, UTC-anchored all-day dates, both DST transitions, cache corruption/expiry, empty replacements, clearing, and failure at every chunk/header/pointer write of a replacement.
- Demo emery installation and screenshot inspection of the agenda, timed/all-day details, future-day navigation, returning to today, an empty received day, action menu and cache-clear confirmation.
- Emulator AppMessage injection of a timeout retained the received event list and displayed offline status. A BEGIN/COMMIT missing its declared event retained the list and displayed an interrupted-sync status.
- Installing the live app after the demo discarded the demo cache and correctly reported that the SDK emulator lacks the Calendar plugin. The live JS bundle contains the provider adapter and no demo event strings.
- PBW inspection verified native emery binaries, shared application UUID, `manifestVersion: 2`, and `usesPermissions: ["Calendar"]` in both packages.

The emulator caught a platform constraint during development: apps cannot override a long Back press. The implemented action menu therefore uses **double Select**, and was verified afterward. Held Select still returns to today.

## Not completed

- Actual calendar delivery from Pebble Core 1.15.0.1/EventKit on the user's iPhone running iOS 27.
- Physical Time 2 installation, Bluetooth reconnect and iOS background/suspension behavior.
- Personal/corporate calendar verification through authorized device access, and coexistence with native Timeline synchronization.
- Battery measurement, real-device firmware storage limits and extended runtime testing.

Follow [the device checklist](device-validation.md) for those checks. Do not interpret emery emulator success as certification of real calendar access or complete daily coverage.
