# PebbleCal

A native month calendar and button-driven **upcoming agenda** for Pebble Time 2 (`emery`). It reads the calendars already enabled in Pebble Core through the built-in experimental Calendar plugin. No Google login, ICS link, hosted settings page, backend, separate iPhone app, or modified Core app is needed.

**Current provider limit: the next 10 events across all enabled calendars, within seven days. Completed events are excluded. This is not a complete daily agenda.** A day with no displayed events means none were in the received list, not that your calendar is free.

Synthetic emulator previews (no real calendar data):

![Demo calendar](docs/screenshots/month-demo.png) ![Demo event details](docs/screenshots/details-demo.png)

The detail screen has separate Event Name, Start Time, End Time, Room, Attendee List and Calendar sections. **Core's current Calendar plugin does not expose attendees** even though native Timeline can show them. Live attendee lists therefore say “Not provided by phone.” PebbleCal parses room resources from attendees when supplied to its adapter, and uses a matching event Location string as a labeled fallback. It does not read Timeline pins or modify Core to obtain missing fields.

The room format is `OFFICE | address BUILDING-floorOrdinal-floor.room Room Name (capacity) [facilities]`. The parser extracts office, building, floor, room number, name, capacity and optional facilities, checks that both floor identifiers agree, and separates resource attendees from people. Numeric room numbers drop leading zeroes. The room card identifies whether the source was an attendee or Location. Unrecognized locations remain visible as ordinary text; no office/floor/capacity is guessed.

## iPhone prerequisites

- Pebble Core **1.15.0.1** or a compatible newer release. The preview API can change.
- Settings → Debug → Show debug options → **Use experimental plugins / permissions** enabled.
- Core has full Calendar access in iOS Settings, and your desired calendars are enabled in Core.
- Your Time 2 is connected to the phone.

The app declares only the `Calendar` permission in a version-2 app manifest. If Core requests permission, approve it on the phone and refresh the watch app. Do not grant access to corporate calendars unless your employer's rules permit this custom app to read/display them.

## Controls

| Button | Month calendar | Agenda | Event details |
| --- | --- | --- | --- |
| Up / Down | Previous / next day | Previous / next event | Scroll |
| Select | Open selected day's agenda | Open details | — |
| Back | Exit | Return to month | Return to agenda |
| Hold Up / Down | Previous / next month | Previous / next date | — |
| Hold Select | Return to today | Return to today | — |
| Double Select | View switch, Today, Refresh, Clear cache | Same actions | — |

PebbleCal opens to the month view, with Monday first, a selected date and an outline around today. Month dots mark received events only. You can browse twelve months backward/forward for date planning; browsing does not fetch more events, and dates outside the received window explicitly show no schedule data. The agenda uses a filled dot for an ongoing timed event and an outlined dot for the next one. All-day events are labeled separately. Times follow the watch's clock preference. Long titles/details scroll; exceptionally long metadata is bounded and marked as shortened.

## Build locally

Use the same local SDK flow as `pebble-flight` and `pebble-transit`. Clone this separate project under `~/Developer/pebble-calendar` if working on your Mac.

If your existing `pebble` CLI and SDK work, the scripts use them when no project-local virtual environment exists. The tested versions for this project are **pebble-tool 5.0.40** and **SDK 4.33.1**.

For a fresh Mac setup:

```sh
brew install uv python@3.13 node
cd ~/Developer/pebble-calendar
./scripts/setup.sh
```

Setup creates an ignored `.tools/venv`; Linux SDK/cache data also stays under `.tools`. On macOS the SDK uses its normal Application Support directory. The restricted cloud workspace additionally needs a temporary CLI symlink adjustment; `prepare-toolchain.py` applies it only if `/var/tmp` is read-only. It does not change Core or the watch SDK API.

Build the live app:

```sh
./scripts/build.sh
```

The installable package is **`dist/pebble-calendar.pbw`**. `build/pebble-calendar.pbw` is also generated, but always represents whichever mode you built most recently. Use the explicitly named `dist` file for installation. The packaging step adds Core 1.15's permission metadata; plain `pebble build` alone is not the complete production workflow.

## Synthetic emulator demo

```sh
./scripts/build.sh --demo
./scripts/emulator.sh
./scripts/pebble.sh screenshot --emulator emery --vnc --no-open test-output/agenda.png
./scripts/pebble.sh emu-button click select --emulator emery --vnc
./scripts/pebble.sh logs --emulator emery --vnc
```

The demo PBW is `dist/pebble-calendar-demo.pbw`, named **PebbleCal Demo**. It shows **DEMO** and uses only made-up events, attendees and a fictional room resource. It never substitutes synthetic events in the live app. The two builds intentionally share an app UUID: installing one replaces the other, and a cache from the other mode is discarded on opening. Version 0.2 upgrades the event/cache format; an old 0.1 cache is discarded and refreshed through Core.

The standard emulator does not implement Core's native Calendar plugin. Use the demo for UI testing. Installing the live PBW in that emulator should report that plugins need enabling; it cannot prove iPhone calendar access. `--vnc` works in a headless environment as well as on a Mac.

## Install on your Time 2

For the local deployment path, enable **Use LAN developer connection** in Core's Phone → Connectivity settings, then enable **Dev Connection** for the connected watch in Devices. Use the phone's displayed address on a trusted local network:

```sh
./scripts/build.sh
./scripts/install.sh PHONE_IP
./scripts/pebble.sh logs --phone PHONE_IP
```

Alternatively, send `dist/pebble-calendar.pbw` to your iPhone with AirDrop and open it in Pebble Core from Files/the share sheet. A cloud Developer Connection also works through `./scripts/pebble.sh login` and `./scripts/pebble.sh install --cloudpebble dist/pebble-calendar.pbw`; LAN or Files avoids needing that service for installation.

Do not capture screenshots of actual corporate events or enable logging that includes event content. This app's own log output includes counts and cache status only.

## Offline behavior and privacy

The watch opens its last complete snapshot immediately and asks Core for fresh events. While open, it subscribes to provider changes; Bluetooth updates contain only changed snapshots. An interrupted transfer, bad payload or ordinary sync failure leaves the last complete list available. A successful empty snapshot clears the previous event list. Revoking the app's Calendar permission clears its cache when Core reports `PERMISSION_DENIED`.

The app keeps no persistent phone-side event cache, makes no web requests and uploads no events. Watch caching creates a bounded extra local copy. Cached snapshots are valid for **24 hours**; expired data is deleted when the app next runs/checks its clock. The app cannot erase flash on a timer while closed. Double Select → Clear cache → Clear now erases the app's saved data and pauses refresh for that session until you choose Refresh. Keep real event data out of repository files and shared artifacts.

## Tests and validation

```sh
./scripts/test.sh
```

Requires Node.js and a host C compiler (`cc`; Xcode Command Line Tools on macOS). Tests use synthetic events only. They cover provider validation, chronological merging, missing fields, UTF-8 bounds, empty/update/delete snapshots, message acknowledgments, stale subscription callbacks, and error redaction. Native tests exercise date boundaries, midnight spans, all-day date semantics, 23/25-hour DST days, cache expiry/corruption and every interruption point in a cache write.

Build and emulator validation were performed in the cloud workspace. The original live app was successfully installed through the cloud Developer Connection. Actual EventKit delivery through your iPhone on iOS 27 and background suspension/reconnect behavior still need the [device checklist](docs/device-validation.md). No real personal or corporate events were accessed during development.

## Limitations and troubleshooting

- **Only 10 events / seven days:** busy calendars can exhaust the limit before tomorrow. There is no pagination, complete-day coverage indicator, historical query or descriptions in the provider. Extending those needs an upstream Core API change.
- **Canceled/declined events:** the provider omits cancellation status and differs from Timeline's declined-event filter. The app cannot independently filter canceled records that EventKit still returns.
- **Updates while closed:** PKJS stops when the watch app closes. iOS can suspend Core, delaying timers and updates. Open Core and use Refresh when needed; no real-time/background guarantee is made.
- **Enable plugins:** verify the debug toggle and supported Core version; reopen this app after enabling it.
- **Allow Calendar access:** check Core's iOS full calendar permission, its enabled calendar list, and the custom app's Calendar permission; refresh afterward. Core may return an empty list when OS permission is missing, which cannot be distinguished from no available events by this provider alone.
- **Sync failed/offline:** check watch connection, open Core, then Double Select → Refresh. Cached data stays visibly marked.
- **RAM only:** firmware storage is too small or a cache write failed; the current list works until exit but may not survive restarting. Current emery SDK supplies `persist_get_max_size`; this MVP is intended for current firmware.
- **Attendees missing:** Core's plugin does not expose them. Native Timeline uses a separate synchronization path that includes attendee text. An upstream plugin extension is required for live attendee lists and rooms found only in attendee resources.
- **Demo data after installation:** install the live `dist/pebble-calendar.pbw`, named PebbleCal; the demo is a separate explicitly named build.
- **SDK/editor issues:** use `./scripts/pebble.sh --version`; install an emery-capable SDK; avoid mixing `.tools` with another repository's virtual environment. In sandboxed cloud execution, emulator commands need local socket permission. STPyV8 requires ICU data in its standard per-user cache; this was prepared separately in the development environment.

See [architecture](docs/architecture.md) and [development references](docs/references.md). Future work includes an official bounded date-range provider with stable occurrence IDs, coverage/pagination and cancellation metadata. No CI/CD or additional hosting is introduced.
