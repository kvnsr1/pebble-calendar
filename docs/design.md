# PebbleCal 0.5 design

Reviewed 2026-10-14 against Pebble's live developer guides and the official documentation source at [coredevices/sdk-docs](https://github.com/coredevices/sdk-docs/tree/a9641e4db0426a026151f6d689255ea724904800). Research checkouts are separate from this application and from Flight/Transit. The original attendee integration is retained.

## Decisions

| Pebble guidance | PebbleCal implementation |
| --- | --- |
| Prioritize the app's main purpose and quick access | Open today's overview. Up/Down browse dates; Select opens the received event list; held Select opens calendar/options. |
| Familiar menu navigation and clear actions | Native MenuLayer for events; native ActionMenu for date navigation, refresh and clearing. A visible Calendar & options list row, held Select on the overview and the detail Select indent expose options. |
| Back leaves the current window | Separate day overview, event list, calendar, detail and confirmation windows. Back from calendar cancels browsing; Select applies the chosen date. |
| Large type and clear hierarchy | Size 28 date/title headings, size 24 agenda titles, size 18 body/labels. Extra-large content size uses size 24 bodies and size 28 event titles, with stacked time fields. |
| Few purposeful colors | Dark Oxford Blue headers with white titles, white body, black primary information and cobalt selection/labels; NOW/NEXT text conveys event state without relying only on color. |
| Make overflow and additional actions apparent | Full-width ScrollLayer with a reserved bottom indicator strip and a small Select indent instead of a fixed navigation bar. Titles wrap fully in details. |
| Use the status bar when current time helps | Native clock on agenda, calendar and detail screens. The clock and detail page use the full screen width. |
| Give feedback without unnecessary distraction | Native menu/window/scroll transitions. No new animation loop or vibration on routine navigation. |
| Clear feedback and confirmation for destructive actions | A bold confirmation dialog explains that clearing removes this watch's saved copy. Select confirms; Back cancels. |
| Cache data and explain connection failures | Existing bounded cache/expiry remains intact. Saved/offline states and incomplete upcoming coverage remain visible. |

The calendar keeps a Monday-first seven-column grid with size 18 day numbers, a filled selection, outlined today and dots only for received events. It uses actual display bounds rather than a hardcoded resolution. The rectangular emery target is the supported platform; reading round-design guidance does not imply a round-watch build was tested.

## Timeline layout reference

The 0.5 revision follows the user's native Timeline screenshot without retaining its event content. Source inspection also confirmed the layout in [calendar_layout.c](https://github.com/coredevices/PebbleOS/blob/8bbd9407edc63198e39c1996f866af6e2be3ef96/fw/services/timeline/calendar_layout.c), the Select affordance in [action_button.c](https://github.com/coredevices/PebbleOS/blob/8bbd9407edc63198e39c1996f866af6e2be3ef96/fw/applib/ui/action_button.c), and its use in [pin_window.c](https://github.com/coredevices/PebbleOS/blob/8bbd9407edc63198e39c1996f866af6e2be3ef96/fw/apps/system/timeline/pin_window.c). These internal firmware components are references, not callable SDK APIs or dependencies. PebbleCal draws its own date icon, triangle/square start/end markers and a 13-pixel black Select circle centered four pixels outside the screen using public SDK primitives.

The overview has no MenuLayer competing for its buttons or drawing over the footer. Its body is bounded between the measured header and footer; long preview titles use ellipsis, while full detail titles wrap and scroll. The event list is a separate native menu window with focused rows centered so their calendar source is not cut off. Detail text reserves a small right margin for the indent, preserving substantially more width than an ActionBarLayer. No additional timer, phone request or persistent copy was introduced for date browsing.

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

Four unmodified action PNGs (up, down, ellipsis, check) come from [the official documentation assets](https://github.com/coredevices/sdk-docs/tree/a9641e4db0426a026151f6d689255ea724904800/source/assets/images/guides/design-and-interaction/icons), offered under the repository's Apache 2.0 license. They are stored in `resources/images/pebble`. The [license copy](licenses/pebble-sdk-docs-Apache-2.0.txt) is included. Only the confirmation check is packaged in version 0.5; the other three reference icons no longer consume watch resources. The original launcher icon and primitive calendar glyph remain independently authored. Example C implementations were references; no example module is bundled into the app. All preview calendar data is fictional.
