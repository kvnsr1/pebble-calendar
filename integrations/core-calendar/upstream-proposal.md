# Draft: Expose attendee display names from the built-in Calendar source

Core's EventKit reader already collects attendee display names, including meeting-room resources, and uses them for native Timeline pins. The built-in Calendar source omits them, leaving native calendar watchapps unable to show the same participant and room information through the supported plugin interface.

This change adds an `attendees` property with the existing `longText` shape. Its text contains one display name per newline. It preserves resource names and commas, normalizes embedded line breaks, skips blank names, and never substitutes email addresses. An empty string distinguishes supplied-but-empty metadata from an omitted property. Existing explicit subscriptions omit the field unless requested; a null property selection includes it.

It reuses the current `SystemCalendar` reader, enabled-calendar list and Calendar caller permission. No EventKit, Timeline synchronization, database, authentication, refresh interval, date-window or event-limit change is included.

Five synthetic common tests exercise the actual source observation flow, its declaration and permissions, attendee/resource serialization, excluding email addresses, omitted/empty lists, changed participants and denied/disabled access. PebbleCal's adapter discovers the declaration before requesting it and has synthetic contract tests for old and enhanced providers.

Validation status and environment limitations are recorded in `docs/validation.md` in the PebbleCal repository. This draft has not been submitted, and no iOS device validation is claimed.
