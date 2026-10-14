# Core Calendar attendee enhancement

This directory contains a reviewable source patch for [Core's mobile app](https://github.com/coredevices/mobileapp), based on commit `ec2156cc38a850e5458addba2a2fb8efa34396df`. It extends the existing Calendar plugin; it is not a separate iOS app and does not read the watch's Timeline database.

The patch has not been submitted upstream or shipped in an official Core release. Installing the PebbleCal PBW alone does **not** add attendees to Core 1.15.0.1.

## Contract

- Built-in plugin UUID: `00000000-0000-0000-0001-000000000004`.
- Source: `calendar/event`.
- New property declaration: `attendees: ["longText"]`.
- Payload: `attendees.longText.text`, with **one attendee display name per newline**, including room/resource participants. Commas remain within names.
- Names with embedded CR/LF have those line breaks replaced by a space and are trimmed. Blank/missing names are omitted. Email addresses are never substituted or exported.
- An explicit empty string means EventKit supplied no usable attendee display names. It cannot establish that a meeting has no participants.
- The property is emitted when requested explicitly, or when `properties` is null (all properties). Existing explicit subscribers receive their previous fields.
- The existing `Calendar` caller permission, enabled-calendar selection, ten-event/seven-day limits, refresh behavior and EventKit reader are retained. No new database, credential, OS permission or network request is added.

Example using fictional data:

```json
{
  "attendees": {
    "longText": {
      "text": "Alex Sample\nABC | 1000 MB2-3rd-3.07 Example Room (8) [Team Room, Video]\nJordan Sample"
    }
  }
}
```

PebbleCal 0.3 discovers this declaration using `Pebble.enumeratePlugins()` before requesting the field. Unsupported Core releases still work with the original six properties. Missing metadata stays unavailable; malformed advertised metadata rejects the replacement and leaves the last valid watch snapshot intact. Resource attendees take priority over a matching Location fallback; people and room fields are bounded before AppMessage transmission.

## Apply and test

Use a separate Core checkout. Do not apply this patch to Flight or Transit.

```bash
git clone https://github.com/coredevices/mobileapp.git core-calendar-dev
git -C core-calendar-dev switch --detach ec2156cc38a850e5458addba2a2fb8efa34396df
./scripts/apply-core-calendar-patch.sh /absolute/path/to/core-calendar-dev
cd /absolute/path/to/core-calendar-dev
./gradlew :libpebble3:jvmTest --tests '*CalendarPlugin*Test'
```

The script checks applicability before changing files and recognizes an already-applied patch. The patch adds five synthetic Core tests covering permission/declaration, resource names and commas, excluding emails, empty versus omitted lists, changed attendees, disabled calendars and denied calendar access. No production calendar content is used.

The cloud validation outcome is recorded in [validation.md](../../docs/validation.md). Full iOS signing and on-device attendee delivery need a Mac and an iPhone; the Linux workspace cannot perform them. The user confirmed no Mac is available, so adoption into an official Core release is the preferred installation path. The patch and contribution description are prepared for that route, but have not been posted and no release timeline is known.

## Build on a Mac and install on iPhone

Follow [Core's current iOS build instructions](https://github.com/coredevices/mobileapp#ios). At this source revision they require Java 17, CocoaPods, an Android SDK (also for the iOS Gradle configuration), and Xcode with an installed iOS SDK/device support matching the phone. The reported iOS 27 device compatibility is not verified here.

1. Apply the patch to the separate Core checkout and run the Calendar plugin tests.
2. Set `local.properties` to the installed Android SDK path. Use the provided dummy Firebase plist for a local build when Firebase-dependent features are unnecessary; no hosted calendar service is required.
3. Run `./gradlew podInstall`, then open `iosApp/iosApp.xcworkspace` in Xcode. Use your own bundle identifier and signing team. Configure entitlements and versioning as Core's README describes.
4. Build and run on the physical iPhone. The source README reports an existing iOS simulator audio-library linking problem, and the simulator has no watch Bluetooth connection.
5. A separately signed bundle does not update the App Store application in place. Its calendar permissions and app data are separate; grant Calendar access and enable the intended calendars again. Treat it as the watch's active companion for testing. Pairing/connection handoff from the official app and return to it have not been verified; avoid two active companions trying to own the connection.
6. Enable experimental plugins in the test build and grant PebbleCal's existing Calendar permission if prompted. Install the PebbleCal 0.3 PBW through that companion's Developer Connection.
7. Test with a synthetic invitation and a fictional room resource **in attendees**, not only in Location. Check names, room extraction, participant changes/removal, permission revocation, offline cache and continued native Timeline updates. Keep real calendar data out of logs and screenshots.

Core's README says some developer-connection/Firebase features need configuration. The cloud Developer Connection used with the official app is **not guaranteed** with a dummy-config custom build. Configure the supported developer connection available in that build and verify it before installation. Core may remain suspended in the background; this patch adds no new background-execution capability.

If Core accepts the enhancement, using a future official release avoids ongoing custom signing and fork maintenance. [upstream-proposal.md](upstream-proposal.md) provides a draft contribution description; it has not been posted.

## Licensing and attribution

The patch modifies Core Devices' source, offered under GPLv3 or its separate commercial license. The GPLv3 license copy is provided as [LICENSE-GPL-3.0.txt](LICENSE-GPL-3.0.txt). Original source copyright remains Core Devices LLC. The patch and its synthetic test additions are supplied under GPLv3. Core's README also describes its additional App Store permission. The independently written PebbleCal adapter consumes the serialized interface and does not bundle Core source into the PBW.
