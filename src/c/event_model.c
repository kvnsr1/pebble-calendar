#include <pebble.h>
#include "calendar.h"
#include <string.h>

bool calendar_event_valid(const CalendarEvent *event) {
  return event->start > 0 && event->end > event->start &&
    !(event->flags & ~(FLAG_ALL_DAY | FLAG_TRUNCATED)) &&
    event->title[0] && memchr(event->title, 0, sizeof(event->title)) &&
    memchr(event->location, 0, sizeof(event->location)) &&
    memchr(event->calendar, 0, sizeof(event->calendar));
}

int calendar_day_code(time_t instant, bool utc) {
  struct tm value = *(utc ? gmtime(&instant) : localtime(&instant));
  return (value.tm_year + 1900) * 10000 + (value.tm_mon + 1) * 100 + value.tm_mday;
}

time_t calendar_midnight(time_t instant) {
  struct tm value = *localtime(&instant);
  value.tm_hour = value.tm_min = value.tm_sec = 0;
  value.tm_isdst = -1;
  return mktime(&value);
}

time_t calendar_shift_day(time_t day, int offset) {
  struct tm value = *localtime(&day);
  value.tm_mday += offset;
  value.tm_hour = value.tm_min = value.tm_sec = 0;
  value.tm_isdst = -1;
  return mktime(&value);
}

bool calendar_event_on_day(const CalendarEvent *event, time_t day) {
  if (event->flags & FLAG_ALL_DAY) {
    int code = calendar_day_code(day, false);
    return calendar_day_code(event->start, true) <= code && code < calendar_day_code(event->end, true);
  }
  return event->start < calendar_shift_day(day, 1) && event->end > day;
}

int calendar_day_events(const CalendarSnapshot *snapshot, time_t day, int *indices) {
  int count = 0;
  for (int i = 0; i < snapshot->count; ++i) {
    if (calendar_event_on_day(&snapshot->events[i], day)) indices[count++] = i;
  }
  return count;
}

bool calendar_expired(const CalendarSnapshot *snapshot, time_t now) {
  // A large backwards clock change also invalidates freshness.
  return snapshot->updated <= 0 || now - snapshot->updated > CACHE_TTL || snapshot->updated - now > 300;
}
