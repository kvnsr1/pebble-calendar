#include <pebble.h>
#include "calendar.h"
#include <string.h>

bool calendar_event_valid(const CalendarEvent *event) {
  return event->start > 0 && event->end > event->start &&
    !(event->flags & ~(FLAG_ALL_DAY | FLAG_TRUNCATED | FLAG_ATTENDEES_AVAILABLE)) &&
    event->title[0] && memchr(event->title, 0, sizeof(event->title)) &&
    memchr(event->location, 0, sizeof(event->location)) &&
    memchr(event->calendar, 0, sizeof(event->calendar)) &&
    memchr(event->attendees, 0, sizeof(event->attendees)) &&
    memchr(event->room.name, 0, sizeof(event->room.name)) &&
    memchr(event->room.office, 0, sizeof(event->room.office)) &&
    memchr(event->room.building, 0, sizeof(event->room.building)) &&
    memchr(event->room.number, 0, sizeof(event->room.number)) &&
    memchr(event->room.facilities, 0, sizeof(event->room.facilities)) &&
    event->room.source <= 2 && event->room.floor >= 0 && event->room.floor <= 999 &&
    event->room.capacity >= 0 && event->room.capacity <= 10000 &&
    (!event->room.source || (event->room.name[0] && event->room.office[0] && event->room.building[0] &&
      event->room.number[0] && event->room.capacity > 0)) &&
    (event->room.source != 1 || (event->flags & FLAG_ATTENDEES_AVAILABLE));
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

time_t calendar_month_start(time_t day) {
  struct tm value = *localtime(&day);
  value.tm_mday = 1;
  value.tm_hour = value.tm_min = value.tm_sec = 0;
  value.tm_isdst = -1;
  return mktime(&value);
}

time_t calendar_shift_month(time_t day, int offset) {
  struct tm value = *localtime(&day);
  int previous_day = value.tm_mday;
  value.tm_mday = 1; value.tm_mon += offset; value.tm_isdst = -1;
  time_t first = mktime(&value);
  struct tm next = *localtime(&first);
  next.tm_mon++; next.tm_mday = 0; next.tm_isdst = -1;
  time_t last = mktime(&next);
  int days = localtime(&last)->tm_mday;
  return calendar_shift_day(first, (previous_day < days ? previous_day : days) - 1);
}

void calendar_month_grid(time_t day, time_t *cells) {
  time_t first = calendar_month_start(day);
  int weekday = (localtime(&first)->tm_wday + 6) % 7;
  cells[0] = calendar_shift_day(first, -weekday);
  for (int i = 1; i < 42; ++i) cells[i] = calendar_shift_day(cells[i - 1], 1);
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
