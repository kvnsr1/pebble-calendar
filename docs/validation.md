# Validation record

Validated 2026-10-13 in the cloud execution workspace using Pebble Tool 5.0.40, SDK 4.33.1, Python 3.12 and Node 24.19. All calendar records used were synthetic.

## Completed

- Native emery compilation of live and demo modes; complete permission-packaged PBWs generated.
- Version 0.2 live memory report: 37,997 bytes RAM footprint / 128 KiB, 4,237 bytes resources / 256 KiB. Runtime heap consumption is additional; no long-term battery benchmark has been performed.
- Twenty-one Node tests covering source validation, provider discovery, fixed errors, optional fields, Unicode bounds, snapshots, acknowledgments, stale callbacks, attendee/resource separation, ordinal floors, matching floor metadata, capacities and explicit Location fallback.
- Host C tests for overlap/day membership, midnight-exclusive ends, UTC-anchored all-day dates, both DST transitions, leap-year month clamping, Monday-first 42-cell grids, room metadata validity, cache corruption/expiry, empty replacements, clearing and every cache write interruption point.
- Demo emery installation and screenshot inspection of the agenda, timed/all-day details, future-day navigation, returning to today, an empty received day, action menu and cache-clear confirmation.
- Emulator AppMessage injection of a timeout retained the received event list and displayed offline status. A BEGIN/COMMIT missing its declared event retained the list and displayed an interrupted-sync status.
- Installing the live app after the demo discarded the demo cache and correctly reported that the SDK emulator lacks the Calendar plugin. The live JS bundle contains the provider adapter and no demo event strings.
- PBW inspection verified native emery binaries, shared application UUID, `manifestVersion: 2`, and `usesPermissions: ["Calendar"]` in both packages.
- Version 0.2 emulator screenshots inspected for month navigation, view switching, event-name/start/end sections, parsed fictional room fields and synthetic attendee names. Core's live plugin still provides no attendees; no actual attendee data was accessed.
- Versions 0.1 and 0.2 installed successfully on the physical watch through the cloud Developer Connection. Version 0.2's first attempt timed out waiting for the phone; the retry reached the phone and reported “App install succeeded.” No watch screenshots or live event content were retrieved.

The emulator caught a platform constraint during development: apps cannot override a long Back press. The implemented action menu therefore uses **double Select**, and was verified afterward. Held Select still returns to today.

## Not completed

- Actual calendar delivery from Pebble Core 1.15.0.1/EventKit on the user's iPhone running iOS 27.
- Bluetooth reconnect and iOS background/suspension behavior.
- Personal/corporate calendar verification through authorized device access, and coexistence with native Timeline synchronization.
- Battery measurement, real-device firmware storage limits and extended runtime testing.

Follow [the device checklist](device-validation.md) for those checks. Do not interpret emery emulator success as certification of real calendar access or complete daily coverage.

## Version 0.3 attendee integration — 2026-10-14

- All 27 Node tests and the host C suite passed. New contract tests cover compatible declaration discovery, older/wrong providers, people and room extraction from the advertised live field, facilities commas, missing versus empty metadata, malformed replacements, and participant/room changes and deletions.
- Live and synthetic demo emery builds succeeded. Live RAM footprint remains 37,997 bytes; resources remain 4,237 bytes. No native UI/cache schema changed from 0.2.
- Both PBWs were inspected: version 0.3.0, emery target, manifest version 2, Calendar permission only. The live bundle contains the capability check and no fictional demo attendee names. SHA-256 checksums are generated in `dist/SHA256SUMS`.
- The Core patch, including five synthetic common tests, applied to a clean worktree at `ec2156cc38a850e5458addba2a2fb8efa34396df`. Reapplying the helper script correctly reported that the patch was already applied.
- Core's targeted JVM suite passed: four existing Calendar plugin tests and five new attendee tests, with no failures/errors. The production shared library and its test sources compiled. Linux validation used a portable Java 17 toolchain, workspace-local Gradle/Android/Kotlin Native caches, `--configure-on-demand`, and `-Pkotlin.mpp.enableCInteropCommonization=false`. Ordinary whole-project configuration still requires an Android SDK; no application build configuration was modified to obtain the targeted test result.
- Version 0.3 installed successfully on the physical watch through the cloud Developer Connection. The first attempt authenticated but timed out waiting for the phone; the retry connected and reported “App install succeeded.” No live calendar content or watch screenshots were retrieved. Official Core 1.15.0.1 still cannot supply attendees through the plugin.

The Core patch is a proposed interface, not an official release. iOS compilation/signing, actual EventKit attendee delivery and Timeline coexistence still require verification. This workspace is Linux and has no Xcode/iPhone signing environment; the user confirmed no Mac is available. No real attendee data was read and no upstream issue or PR was posted.

## Version 0.4 Pebble-guided redesign — 2026-10-14

- Reviewed the three requested guide indexes, all their child guide pages, the current official documentation source and 17 linked example repositories. The nested source/resource inventory and design decisions are recorded in [design.md](design.md).
- All 27 Node tests and the host C suite passed after the UI work. Live and synthetic demo emery builds succeeded, with no application compiler errors. The SDK's existing RWX segment linker warning remains. Live RAM footprint: 41,342 bytes / 128 KiB; resources: 4,629 bytes / 256 KiB. Runtime heap is additional.
- Both packaged PBWs were inspected: version 0.4.0, emery, manifest version 2, Calendar permission only, and no demo attendee strings in the live JS bundle. Checksums are in `dist/SHA256SUMS`.
- Synthetic emulator screens inspected for the agenda, 24-hour and 12-hour timed details, all-day details, room fields, people, native action menu, month navigation, calendar Back cancellation, selecting a date, and empty/out-of-window days.
- Extra-large text was checked after app restart, including complete focused agenda rows, stacked time fields, clear confirmation, actual clearing of the saved snapshot, and successful refresh afterward. Empty-menu positioning was corrected after testing. The detail indicator has a reserved strip and the patterned scroll shadow was removed.
- Final fictional previews are stored in `docs/screenshots`. They contain no personal/corporate records. No screenshot or calendar content was retrieved from the physical watch.
- Version 0.4 installed successfully on the physical watch through the cloud Developer Connection. The first attempt timed out waiting for the phone; the retry connected and reported “App install succeeded.” Physical data delivery and UI use are still user/device checks, separate from installation.

The provider still supplies at most ten upcoming events within seven days. Attendees require the prepared Core enhancement or a compatible official release; the redesign does not change the phone's installed Calendar plugin.

## Version 0.5 clipping and Timeline layout — 2026-10-14

- Split the overview and event list into separate windows. Up/Down change the overview date without requesting a new phone snapshot; Select opens the day's list or empty-day options. The dark date header, bounded preview and two-line footer no longer compete with a root menu.
- Replaced the event detail action bar with the native-style Select indent, using public SDK primitives. The full-width dark header contains a date glyph, start/end markers and the wrapped title. Room, expanded times/dates, attendee list and calendar remain scrollable.
- All 27 Node tests and host C checks passed. Both live and demo emery PBWs built and passed package inspection: 0.5.0, manifest version 2, Calendar permission only. The live JS bundle contains no demo attendee names. Live RAM footprint is 44,963 bytes / 128 KiB; resources 4,363 bytes / 256 KiB. Runtime heap is additional. Checksums were regenerated and verified after the final builds.
- Synthetic emulator inspection checked regular and extra-large overview/list/details, received empty days, a midnight-spanning event, 12/24-hour time formatting, a long event title and scrolling, fictional room/attendees, options from the Select indent, and clearing the saved list back to the overview. Selecting a month-grid date returns to the corresponding overview; the labels reflect that behavior.
- The live PBW was also launched in the SDK emulator: demo cache was discarded and the missing Calendar plugin showed the fixed enable-plugins error without clipping.
- Version 0.5 installed successfully on the physical watch through the cloud Developer Connection after the user requested another attempt. Earlier attempts timed out waiting for the iPhone; the successful retry connected and reported "App install succeeded." No real calendar data or watch screenshots were retrieved.
- No event title, URL or attendee content from the user's attached watch screenshots was copied into fixtures or artifacts. Physical watch installation does not involve retrieving its calendar data.

The provider limit and official Core attendee limitation remain as documented above.
