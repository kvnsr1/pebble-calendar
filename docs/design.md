# PebbleCal 0.6 design

Reviewed 2026-10-14 against Pebble's live developer guides and the official documentation source at [coredevices/sdk-docs](https://github.com/coredevices/sdk-docs/tree/a9641e4db0426a026151f6d689255ea724904800). Research checkouts are separate from this application and from Flight/Transit. The original attendee integration is retained.

## Decisions

| Pebble guidance | PebbleCal implementation |
| --- | --- |
| Glanceable purpose and clear hierarchy | Date glyph, day/year and large Work/Personal counts on the overview; time, title and location/source in the day timeline. |
| Native button conventions | Overview Up/Down change dates; timeline Up/Down select events; Select advances or starts dictation in details; Back leaves the current window. Held Select on overview/list opens options. |
| Purposeful colors | Charcoal (`GColorDarkGray`) for past, navy (`GColorOxfordBlue`) for today and dark teal (`GColorMidnightGreen`) for future. White text provides contrast; date labels prevent relying only on color. |
| Full-screen, minimal pages | Overview/details fill the display with one dark color. No clock bars, instructions or status footers. Details show only the date icon, start/end and title until a room is entered. |
| Familiar native Timeline | White cards, blue icon rail, calendar glyphs and a selected-event triangle. Cards/icons ease together on button presses. |
| Lightweight feedback | One 180-ms SDK animation per timeline move, canceled cleanly on interruption; no idle timer or routine vibration. |
| Readability and overflow | System fonts, largest-text support, wrapped full titles and scrollable room sections. The content indicator uses the same dark background and does not overlap text. |
| Clear errors and confirmation | Native dictation confirmation; recognition/parse/storage errors appear in separate dialogs. Clearing saved events remains a native confirmation flow. |
| Minimal data | No attendee request in the current adapter; watch-only bounded room annotations and hashed calendar-group preferences. Provider coverage limits remain documented, even though the user requested no status labels on the pages. |

The separate month grid remains a native-style Monday-first calendar with size 18 day numbers, filled selection, outlined today and dots for received events.

## Timeline and dictation source references

The requested screenshots were used as layout references without copying their real event titles or URLs. Source inspection includes [calendar_layout.c](https://github.com/coredevices/PebbleOS/blob/8bbd9407edc63198e39c1996f866af6e2be3ef96/fw/services/timeline/calendar_layout.c), [timeline/layer.c](https://github.com/coredevices/PebbleOS/blob/8bbd9407edc63198e39c1996f866af6e2be3ef96/fw/apps/system/timeline/layer.c), [animations.h](https://github.com/coredevices/PebbleOS/blob/8bbd9407edc63198e39c1996f866af6e2be3ef96/fw/apps/system/timeline/animations.h) and [action_button.c](https://github.com/coredevices/PebbleOS/blob/8bbd9407edc63198e39c1996f866af6e2be3ef96/fw/applib/ui/action_button.c). These are design references, not firmware APIs called by the app. The app uses public SDK primitives, Animation, Layer and DictationSession; icons are independently drawn.

Core's existing voice path was checked against its `1.15.0.1` tag: [VoiceSessionManager](https://github.com/coredevices/mobileapp/blob/1.15.0.1/libpebble3/src/commonMain/kotlin/io/rebble/libpebblecommon/connection/endpointmanager/audio/VoiceSessionManager.kt), [HybridTranscription](https://github.com/coredevices/mobileapp/blob/1.15.0.1/pebble/src/commonMain/kotlin/coredevices/pebble/services/HybridTranscription.kt) and [STTRouter](https://github.com/coredevices/mobileapp/blob/1.15.0.1/pebble/src/commonMain/kotlin/coredevices/pebble/services/STTRouter.kt). Speech processing depends on the selected Core mode; this app neither guarantees offline speech nor introduces another speech service.

## Guide pages and nested resources accessed

All child guide pages listed in the three requested indexes were accessed, including the less applicable configuration, AppGlance and round-display guides. Their source files and linked images were available in the documentation repository. Relevant SDK API contracts were checked in the installed emery SDK 4.33.1 header.

- [Design and Interaction](https://developer.repebble.com/guides/design-and-interaction/): [benefits](https://developer.repebble.com/guides/design-and-interaction/benefits/), [core experience](https://developer.repebble.com/guides/design-and-interaction/core-experience/), [recommended patterns](https://developer.repebble.com/guides/design-and-interaction/recommended/), [one-click actions](https://developer.repebble.com/guides/design-and-interaction/one-click-actions/), [round design](https://developer.repebble.com/guides/design-and-interaction/in-the-round/), and [example implementations](https://developer.repebble.com/guides/design-and-interaction/implementation/).
- [Graphics and Animations](https://developer.repebble.com/guides/graphics-and-animations/): [animations](https://developer.repebble.com/guides/graphics-and-animations/animations/), [primitives/images/text](https://developer.repebble.com/guides/graphics-and-animations/drawing-primitives-images-and-text/), [framebuffer graphics](https://developer.repebble.com/guides/graphics-and-animations/framebuffer-graphics/), and [vectors](https://developer.repebble.com/guides/graphics-and-animations/vector-graphics/).
- [User Interfaces](https://developer.repebble.com/guides/user-interfaces/): [configuration](https://developer.repebble.com/guides/user-interfaces/app-configuration/), [exit reason](https://developer.repebble.com/guides/user-interfaces/app-exit-reason/), [C AppGlance](https://developer.repebble.com/guides/user-interfaces/appglance-c/), [REST AppGlance](https://developer.repebble.com/guides/user-interfaces/appglance-rest/), [JS AppGlance](https://developer.repebble.com/guides/user-interfaces/appglance-pebblekit-js/), [content size](https://developer.repebble.com/guides/user-interfaces/content-size/), [layers](https://developer.repebble.com/guides/user-interfaces/layers/), [round UI](https://developer.repebble.com/guides/user-interfaces/round-app-ui/), and [unobstructed area](https://developer.repebble.com/guides/user-interfaces/unobstructed-area/).
- Additional linked resources: [App Assets](https://developer.repebble.com/guides/app-resources/app-assets/), native MenuLayer/ActionBarLayer/ActionMenu/ScrollLayer/StatusBarLayer/ContentIndicator APIs, example window C files, action icon PNGs and the PDC examples' SVG/PDC resources.

Some legacy asset-download links returned errors through the web reader. The corresponding current PNG files and vector example resources were accessed from their public source repositories. REST AppGlance redirects to the C guide on the current site, and the current documentation explicitly says PKJS AppGlance callbacks are unsupported in Core. No REST/JS AppGlance, hosted configuration, PDC animation or framebuffer manipulation was added: the calendar's native layers and small static icons meet its needs.

## Example repositories accessed

Repositories were cloned for source inspection; they are research references, not application dependencies.

| Repository | Commit | Relevant material |
| --- | --- | --- |
| [ui-patterns](https://github.com/pebble-examples/ui-patterns) | `f86bd9a` | Menu/list message, choice/message dialogs, progress and text animation C files; screenshots. |
| [cards-example](https://github.com/pebble-examples/cards-example) | `b8e045b` | Glanceable hierarchy, system fonts, card transitions and vector resource tool. |
| [feature-content-size](https://github.com/pebble-examples/feature-content-size) | `fd9b113` | System text preference detection. |
| [unobstructed-area-example](https://github.com/pebble-examples/unobstructed-area-example) | `8b32e99` | Bounds and obstruction callbacks. |
| [watchface-tutorial-unobstructed](https://github.com/pebble-examples/watchface-tutorial-unobstructed) | `9c9c9fb` | Additional obstruction example. |
| [feature-property-animation](https://github.com/pebble-examples/feature-property-animation) | `ca34b05` | Animation lifecycle. |
| [composite-animations-example](https://github.com/pebble-examples/composite-animations-example) | `0d41475` | Sequence/spawn examples. |
| [pdc-image](https://github.com/pebble-examples/pdc-image) | `b4069d0` | Vector image resources. |
| [pdc-sequence](https://github.com/pebble-examples/pdc-sequence) | `12b693d` | System-style SVG/PDC sequences. |
| [text-flow-techniques](https://github.com/pebble-examples/text-flow-techniques) | `acdd6ae` | Round paging/overflow. |
| [time-dots](https://github.com/pebble-examples/time-dots) | `de0410d` | Round geometry. |
| [one-click-action-example](https://github.com/pebble-examples/one-click-action-example) | `e1d7d24` | Action/exit feedback. |
| [app-glance-hello-world](https://github.com/pebble-examples/app-glance-hello-world) | `d821739` | Native glance example. |
| [app-glance-virtual-pet](https://github.com/pebble-examples/app-glance-virtual-pet) | `9d84693` | Native glance lifecycle. |
| [app-glance-rest-example](https://github.com/pebble-examples/app-glance-rest-example) | `63ebbed` | Historical REST example; not used. |
| [clay-example](https://github.com/pebble-examples/clay-example) | `55b988f` | Configuration example; not used. |
| [Clay](https://github.com/pebble-dev/clay) | `627d35f` | Linked configuration components/documentation; no dependency added. |

## Assets and licensing

Four unmodified action PNGs (up, down, ellipsis, check) come from [the official documentation assets](https://github.com/coredevices/sdk-docs/tree/a9641e4db0426a026151f6d689255ea724904800/source/assets/images/guides/design-and-interaction/icons), offered under the repository's Apache 2.0 license. They are stored in `resources/images/pebble`. The [license copy](licenses/pebble-sdk-docs-Apache-2.0.txt) is included. Only the confirmation check is packaged in versions 0.5–0.6; the other three reference icons no longer consume watch resources. The original launcher icon and primitive calendar glyph remain independently authored. Example C implementations were references; no example module is bundled into the app. All preview calendar data is fictional.
