# Development references and provenance

Source inspected on 2026-10-13:

- [Pebble Flight](https://github.com/kvnsr1/pebble-flight/tree/1cad409f5a93511f1555f6cf45fb8601988aaaab): native C/emery project organization, wscript and local build conventions. Owned by this project's author; no files in Flight were changed.
- [Pebble Transit](https://github.com/kvnsr1/pebble-transit/tree/f877ca515d820b56da2de48d72706f23e7789c49): button selection/detail/back and local deployment conventions. No files in Transit were changed.
- [Core plugin preview documentation](https://github.com/coredevices/mobileapp/blob/ec2156cc38a850e5458addba2a2fb8efa34396df/test-apps/plugins.md): public source API and version-2 permission packaging contract.
- [CalendarPlugin.kt](https://github.com/coredevices/mobileapp/blob/ec2156cc38a850e5458addba2a2fb8efa34396df/libpebble3/src/commonMain/kotlin/io/rebble/libpebblecommon/plugin/CalendarPlugin.kt): actual fields, provider UUID, calendar selection, refresh behavior and limits.
- [IosSystemCalendar.kt](https://github.com/coredevices/mobileapp/blob/ec2156cc38a850e5458addba2a2fb8efa34396df/libpebble3/src/iosMain/kotlin/io/rebble/libpebblecommon/calendar/IosSystemCalendar.kt): EventKit/full-access permission and all-day date anchoring.
- [Core changelog](https://ndocs.repebble.com/changelog): release compatibility and experimental feature status.
- [SDK Storage API](https://developer.repebble.com/docs/c/Foundation/Storage/), [Developer Connection](https://developer.repebble.com/guides/tools-and-resources/developer-connection/), and [PKJS lifecycle](https://developer.repebble.com/guides/communication/using-pebblekit-js/).
- [Calendar for Pebble](https://github.com/alex523ap/calendar-for-pebble/tree/4858da43c82331f303bacd87862c3672fff856b1): investigated as a reference; its ICS/OAuth-independent feed architecture does not satisfy this project's corporate-calendar constraint. No code or images copied.

Core is GPL-3.0/commercial licensed. The watch PBW uses its runtime interface and does not bundle Core code or its packaging helper. The optional source patch in [integrations/core-calendar](../integrations/core-calendar/README.md) modifies Core's Calendar plugin under GPLv3, with attribution and a license copy. It adds participant display names to the existing source without changing the EventKit reader or Timeline synchronization. The small PBW metadata wrapper and watch cache/adapter are independently written. The launcher icon is original simple calendar artwork. Version 0.4 includes four unmodified Apache-licensed action icons from the official documentation repository, with attribution and a license copy in [the design notes](design.md). No downloaded calendar records are bundled.

Tool dependencies: Pebble SDK 4.33.1 and CLI 5.0.40, their shipped compiler/JS bundler/emulator dependencies, Python, Node and a host C compiler for native tests. There are no app npm dependencies. `.tools`, `node_modules`, build products and validation screenshots are ignored. Toolchain packages retain their respective upstream licenses; they are not committed or redistributed in this source repository.

CloudPebble was evaluated but adds no Calendar data access. This project uses the established local SDK workflow and introduces no hosted configuration, backend, CI or service subscription.
