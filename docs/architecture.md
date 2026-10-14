# Architecture

The approved Approach B MVP uses:

```text
Apple Calendar → Core's EventKit SystemCalendar reader
                  → built-in Calendar plugin (calendar/event)
                    → this app's PebbleKit JS → AppMessage → native emery UI/cache
```

Timeline reads are not used: its watch database and internal firmware iterators are absent from the public application SDK. Core's Calendar plugin reuses the same native reader and enabled calendar list, with different window/filter semantics.

## Phone adapter

`src/pkjs/index.js` discovers the built-in UUID `00000000-0000-0000-0001-000000000004` and subscribes to `calendar/event`. It validates the returned provider UUID, because the API's preferred-provider parameter can fall back to another plugin. It requests title, location, start/end timestamps, all-day flag and calendar name. The 0.6 adapter deliberately requests no attendee property, ignores any unsolicited attendee field and disables automatic room extraction from Location. The prior extension parser is retained for legacy contract tests. EventKit is never called directly from JS.

`calendar-source.js` validates an entire envelope before accepting it; a partial bad response does not turn into an incomplete replacement. It preserves raw epoch timestamps and all-day UTC-anchored dates, orders events, sanitizes control characters and clips on UTF-8 character boundaries. Byte limits excluding NUL: title 255, location 127, calendar name 63, supplied attendee text 383, room name 63, office/building/room-number 15 each, facilities 95. A truncation flag is retained in the snapshot; the minimal detail page does not display it. No descriptions, private source identifiers or provider positional IDs are copied to the watch.

Official Core 1.15 exposes no attendee property. The earlier optional Core source proposal is retained in `integrations/core-calendar`; this minimal design needs no Core modification. It keeps title/location/calendar, timing and all-day semantics. Old unused attendee/parsed-room metadata is cleared from cache on opening and on replacement. Existing snapshot/protocol version 2 remains compatible; new room notes have their own versioned records.

## Watch

`main.c` owns a full-screen daily overview, a separate `day_timeline.c` window, a month window, native options/confirmation and AppMessage reception. Overview Up/Down browse dates; Select opens the day's list, including empty dates; held Select opens options. Timeline Up/Down move events, Select opens details and held Select opens options. Actual window stacking gives Back its normal meaning.

`day_timeline.c` draws white cards and calendar icons on a blue rail with a selected-event pointer. A public SDK Animation eases card/icon positions together for 180 ms on each move. SDK 3 destroys completed/canceled animations; a fresh object is created for each movement and the stopped handler clears the active handle. No animation or redraw loop runs when idle.

`event_detail.c` draws only the date glyph, start/end markers and wrapped title on a full-screen date color. The only optional section is a user-entered room with a door icon. It uses a public ScrollLayer with a drawing layer, a reserved down indicator, a Select indent and the public dictation API; errors are separate dialogs, not permanent detail-page instructions. The native window click provider supplies scrolling and Select; an already-open detail window is updated rather than pushed twice. Layout measurements are refreshed when the event, room or viewport changes. There are no attendee, calendar-source, location or cache-status sections. `ui_theme.c` owns system fonts and the shared past/today/future palette. Details match the browsed day's color, including when an event spans midnight. The date glyph and displayed times retain the event's original start; time-zone IDs are not supplied by the provider.

`calendar_groups.c` offers a native watch-only assignment menu for received calendar names. `event_model.c` defaults whole tokens Uber/Gmail to Work/Personal, with Other for unknown names; explicit assignments override that. Up to ten versioned preferences store only a 64-bit name hash and group under keys 350–359. This categorization never claims to identify accounts from labels; identical source names remain a provider limitation.

`room_input.c` normalizes building letters/digits and a floor/room number separated by a decimal point or spoken point/dot. It supports digit sequences, units, tens and hundreds, bounds the input and returns only the structured result. The parser uses a bounded scratch buffer outside the small app stack and wipes it through volatile stores before returning, including failures. Raw transcripts are not retained or logged. `room_notes.c` keeps ten checksummed records at keys 300–309, each with a calendar/title fingerprint, exact start/end, timestamp, build mode and structured room. Notes expire after 24 hours, are pruned on fresh snapshots and minute ticks, and are cleared with event-cache expiry, corruption, explicit clearing or permission revocation. Changes to identity fields drop the note conservatively. These checksums/hashes are not encryption or stable provider IDs.

Native DictationSession forwards the microphone session through the already installed Core application. Core 1.15.0.1's VoiceSessionManager/HybridTranscription/STTRouter source confirms local/cloud provider routing; the watch app does not select or verify the engine. No calendar payload or event title is passed to it. Production microphone recognition cannot be verified by the SDK emulator; demo Select injects one fictional room phrase and is compiled out of the live build.

A staging snapshot is committed only after every expected event has arrived. Bad or incomplete frames preserve the previous displayed snapshot. Details close on replacement to avoid stale positional selections. Time labels use the watch clock preference. A watch minute tick updates date colors and expires notes without requesting events over Bluetooth.

`event_model.c` treats timed events as intervals with exclusive ends and selects those overlapping a local day. It shifts dates through `mktime`, not fixed 86400-second arithmetic, constructs a 42-cell month grid and clamps day-of-month on month changes. All-day start/end are UTC-anchored **dates**, so date membership compares their UTC date labels against the browsed local date, preserving exclusive end semantics. Original event time zones are not provided.

`event_store.c` uses app-private persistent keys, 256-byte chunks, version/size and a checksum. It writes an inactive bank, then commits the bank pointer; incomplete writes leave the previous committed bank intact. After success the old bank is deleted. Version 0.2 uses snapshot/protocol version 2 and larger cache banks for structured metadata; an old-format cache is cleared and refreshed, not misinterpreted. This handles interrupted writes and detects corruption; it is not a cryptographic or encrypted store. Two snapshots exist transiently while replacing the cache.

Only the bounded latest snapshot is retained. The app checks 24-hour expiry and large backwards clock changes on opening and minute ticks. It deletes expired data when running, not while closed. Explicit clearing removes both banks and RAM/staging copies. `PERMISSION_DENIED` also clears the cache; OS-level EventKit denial can instead produce an empty envelope from Core and cannot be diagnosed reliably by this API. Other failures retain the snapshot. The requested minimal pages do not display cached/offline status; options still provide Refresh.

## Provider restrictions

Verified source at Core commit `ec2156cc38a850e5458addba2a2fb8efa34396df`:

- Uses enabled calendars and the same `SystemCalendar` implementation as Timeline.
- Queries now through now + seven days, filters `endTime > now`, sorts by start and takes ten total.
- Checks its observation flow every minute and samples native calendar changes over five seconds; identical envelopes are suppressed.
- Exposes title, location, start/end, all-day and calendar name, with positional instance IDs.
- No date-range parameters, pagination, stable occurrence IDs, descriptions, coverage or cancellation status.

The UI therefore says “Upcoming list” and never certifies an empty day as free. The source lacks a freshness/permission/coverage distinction sufficient for stronger guarantees. The app does not invent missing data, recover unseen events from its cache, or bypass those limits.

The API is experimental and Core must be running/responding. iOS suspension delays observations; PKJS ends when the watch app exits. No background worker or redundant polling timer is added. Reopening/reconnection/manual refresh requests a new subscription. A complete agenda requires further provider expansion; the optional attendee patch does not remove event-count/date-range limits. It changes the shared Core Calendar source for an independently built companion, without changing the native EventKit reader or Timeline synchronization.
