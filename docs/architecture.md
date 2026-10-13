# Architecture

The approved Approach B MVP uses:

```text
Apple Calendar → Core's EventKit SystemCalendar reader
                  → built-in Calendar plugin (calendar/event)
                    → this app's PebbleKit JS → AppMessage → native emery UI/cache
```

Timeline reads are not used: its watch database and internal firmware iterators are absent from the public application SDK. Core's Calendar plugin reuses the same native reader and enabled calendar list, with different window/filter semantics.

## Phone adapter

`src/pkjs/index.js` discovers the built-in UUID `00000000-0000-0000-0001-000000000004` and subscribes to `calendar/event`. It validates the returned provider UUID, because the API's preferred-provider parameter can fall back to another plugin. It requests title, location, start/end timestamps, all-day flag and calendar name. EventKit is never called directly from JS.

`calendar-source.js` validates an entire envelope before accepting it; a partial bad response does not turn into an incomplete replacement. It preserves raw epoch timestamps and all-day UTC-anchored dates, orders events, sanitizes control characters and clips on UTF-8 character boundaries. Byte limits excluding NUL: title 255, location 127, calendar name 63, supplied attendee text 383, room name 63, office/building/room-number 15 each, facilities 95. A truncation flag is visible in details. No descriptions, private source identifiers or provider positional IDs are copied to the watch.

Core 1.15 exposes no attendee property, so live envelopes explicitly mark attendees unavailable. `event-metadata.js` provides bounded attendee/resource normalization for supplied metadata and the synthetic demo without inventing a native plugin attendee schema. A matching Location string is a room fallback. Resource attendees take priority and are separated from people; capacity, floor and room fields are parsed before clipping. The parser validates matching floor identifiers, strips leading zeroes from numeric room numbers, and leaves unmatched strings as ordinary locations. The resource format is documented in the README with no real corporate identifiers.

`bridge.js` sends one AppMessage at a time, waits for transport acknowledgments and coalesces later envelopes during transfer. A frame is retried twice after failure. BEGIN, EVENT and COMMIT carry a snapshot generation and the watch's request token; late frames/callbacks from superseded refreshes or clearing cannot populate a new request. Host error messages are mapped to fixed codes without including exception text. The watch can request refresh; clearing unsubscribes for that session. Calendar events are held only in bounded transient queues, never persisted in JS localStorage.

Only `Calendar` is declared in `appinfo.json`, with `manifestVersion: 2`, matching Core 1.15.0.1. The independently written PBW packaging script supplies these documented metadata fields; it does not bundle a custom provider or settings page. There are no internet/local-network permissions, companion packages, hosted configuration URLs or web requests in the application.

## Watch

`main.c` owns the month grid, agenda, action menu and AppMessage reception. The month grid starts Monday and supports local date planning twelve months in each direction; dots show only received events. `event_detail.c` owns a measured, scrollable detail layout with a title header, start/end columns, room card, people list and calendar source. A staging snapshot is committed only after every expected event has arrived. Bad or incomplete frames preserve the previous displayed snapshot. Details close on replacement to avoid stale positional selections. Time labels use the watch clock preference. A watch minute tick updates status/highlighting without requesting events over Bluetooth.

`event_model.c` treats timed events as intervals with exclusive ends and selects those overlapping a local day. It shifts dates through `mktime`, not fixed 86400-second arithmetic, constructs a 42-cell month grid and clamps day-of-month on month changes. All-day start/end are UTC-anchored **dates**, so date membership compares their UTC date labels against the browsed local date, preserving exclusive end semantics. Original event time zones are not provided.

`event_store.c` uses app-private persistent keys, 256-byte chunks, version/size and a checksum. It writes an inactive bank, then commits the bank pointer; incomplete writes leave the previous committed bank intact. After success the old bank is deleted. Version 0.2 uses snapshot/protocol version 2 and larger cache banks for structured metadata; an old-format cache is cleared and refreshed, not misinterpreted. This handles interrupted writes and detects corruption; it is not a cryptographic or encrypted store. Two snapshots exist transiently while replacing the cache.

Only the bounded latest snapshot is retained. The app checks 24-hour expiry and large backwards clock changes on opening and minute ticks. It deletes expired data when running, not while closed. Explicit clearing removes both banks and RAM/staging copies. `PERMISSION_DENIED` also clears the cache; OS-level EventKit denial can instead produce an empty envelope from Core and cannot be diagnosed reliably by this API. Other failures retain the snapshot and show cached/offline status.

## Provider restrictions

Verified source at Core commit `ec2156cc38a850e5458addba2a2fb8efa34396df`:

- Uses enabled calendars and the same `SystemCalendar` implementation as Timeline.
- Queries now through now + seven days, filters `endTime > now`, sorts by start and takes ten total.
- Checks its observation flow every minute and samples native calendar changes over five seconds; identical envelopes are suppressed.
- Exposes title, location, start/end, all-day and calendar name, with positional instance IDs.
- No date-range parameters, pagination, stable occurrence IDs, descriptions, coverage or cancellation status.

The UI therefore says “Upcoming list” and never certifies an empty day as free. The source lacks a freshness/permission/coverage distinction sufficient for stronger guarantees. The app does not invent missing data, recover unseen events from its cache, or bypass those limits.

The API is experimental and Core must be running/responding. iOS suspension delays observations; PKJS ends when the watch app exits. No background worker or redundant polling timer is added. Reopening/reconnection/manual refresh requests a new subscription. A complete agenda requires an officially expanded provider; this repository does not modify Core.
