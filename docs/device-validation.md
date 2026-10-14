# Physical device validation

Target: Pebble Time 2 / emery, iPhone on iOS 27, Pebble Core 1.15.0.1 with experimental plugins enabled. The user confirmed the Core version/toggle. The remaining device checks have **not** been run from the cloud workspace.

Use harmless synthetic test events in an authorized calendar before checking actual data. Do not copy real work events into logs, screenshots, fixtures, issues or this repository.

1. Build/install the live PBW. Confirm it appears as PebbleCal, not PebbleCal Demo, and opens to today's agenda.
2. Open it with Core running. Confirm the built-in Calendar provider supplies events. If prompted, approve the app's Calendar permission and refresh.
3. Add synthetic personal and authorized work-calendar events. Confirm both appear with calendar names and optional locations; check long title scrolling, separate start/end time sections and the room card. A fictional resource-formatted Location should yield office, building, floor/room and capacity, marked “From event location.” Live attendees should say “Not provided by phone” under the current Core API.
4. Create overlapping timed events, a single-day and multi-day all-day event, and an event ending at midnight. Confirm date membership, all-day end dates and details. Compare watch and phone time zones/clock formats.
5. On the month grid use Up/Down for dates, Hold Up/Down for months, Select for the day's agenda and Back to return. The agenda uses native Up/Down menu scrolling; Select on Calendar & options opens the action menu. The detail ellipsis button and held Select shortcuts also open options. Today, day/month navigation, Refresh and Clear saved events are visible actions. Back exits the agenda, cancels calendar browsing or returns from details, according to the native window stack. Check empty-list wording and the ten-event cap without assuming full coverage.
6. Update an event and remove another. Keep the app/Core active, then refresh; confirm the new snapshot replaces old records. Cancel a synthetic invitation and document whether EventKit/Core removes it; the provider has no cancellation field.
7. Disconnect the phone after a successful refresh. Reopen Calendar; check cached indication and retained events. Reconnect and verify a successful update. Quit/suspend Core and compare refresh failure behavior.
8. Revoke this app's Calendar permission in Core and refresh; verify cached events disappear after the reported denial. Re-enable/approve and refresh. Also test Core's OS calendar permission separately; its provider can return an empty list for OS denial.
9. Options → Clear saved events → Select to confirm. Confirm it clears and stays cleared until Refresh during that session. Reopening intentionally requests events again.
10. Confirm native Timeline still synchronizes while this app uses the shared calendar reader. Test app reopening after a phone/watch restart.

## Enhanced Core attendee checks

These require a signed Core build containing [the attendee patch](../integrations/core-calendar/README.md), or an official release adopting that contract, and PebbleCal 0.3 or later. The official Core 1.15.0.1 installation cannot perform these checks.

1. Confirm the companion advertises `calendar/event` property `attendees: ["longText"]`. Reopen PebbleCal after changing companion builds, so provider discovery runs again.
2. Use synthetic attendee names and a fictional room resource in a test event. Confirm the attendee section lists people, the room section shows the resource fields, and its source reads “From room attendee.” Confirm a conflicting Location does not override the resource attendee.
3. Check a synthetic event without usable attendee names. The UI should distinguish the supplied empty list from “Not provided by phone”; neither wording proves there are no participants hidden by the calendar provider.
4. Change and remove attendees and the room; refresh and verify the entire snapshot replaces old people/room data. Disconnect to confirm the bounded cached snapshot remains readable, then clear it.
5. Revoke PebbleCal's Calendar grant and verify the cached data clears on denial. Check native Timeline still updates, and repeat while Core is foregrounded versus suspended. No new background guarantee is introduced.
6. Return to official Core, reconnect as needed and reopen PebbleCal. Verify the ordinary agenda works and newly received records return to unavailable attendee metadata.

Automated native tests cover DST transitions with synthetic dates; physical time-zone changes and background delivery are separate checks. Document results as counts/pass/fail/status only. Corporate calendar availability in Timeline does not itself authorize new retention or exporting it elsewhere.
