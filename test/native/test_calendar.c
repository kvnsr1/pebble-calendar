#define _POSIX_C_SOURCE 200809L
#include "calendar.h"
#include <pebble.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static unsigned char saved[200][256];
static int lengths[200], writes_until_failure = -1;
bool persist_exists(uint32_t key) { assert(key < 200); return lengths[key] > 0; }
int32_t persist_read_int(uint32_t key) { int32_t value = -1; persist_read_data(key, &value, sizeof(value)); return value; }
int persist_read_data(uint32_t key, void *buffer, size_t size) {
  assert(key < 200);
  if (lengths[key] == 0) return -1;
  int length = lengths[key] < (int)size ? lengths[key] : (int)size;
  memcpy(buffer, saved[key], length); return length;
}
int persist_write_data(uint32_t key, const void *buffer, size_t size) {
  assert(key < 200 && size <= 256);
  if (writes_until_failure == 0) return -1;
  if (writes_until_failure > 0) --writes_until_failure;
  memcpy(saved[key], buffer, size); lengths[key] = size; return size;
}
int persist_write_int(uint32_t key, int32_t value) { return persist_write_data(key, &value, sizeof(value)); }
int persist_delete(uint32_t key) { assert(key < 200); lengths[key] = 0; return 0; }
size_t persist_get_max_size(void) { return 1024 * 1024; }

static time_t date(int year, int month, int day, int hour, int minute) {
  struct tm value = { .tm_year = year - 1900, .tm_mon = month - 1, .tm_mday = day,
    .tm_hour = hour, .tm_min = minute, .tm_isdst = -1 };
  return mktime(&value);
}
static CalendarEvent event(time_t start, time_t end, bool all_day) {
  CalendarEvent result = { .start = start, .end = end, .flags = all_day ? FLAG_ALL_DAY : 0 };
  strcpy(result.title, "Synthetic event"); strcpy(result.calendar, "Test"); return result;
}
int main(void) {
  setenv("TZ", "UTC", 1); tzset();
  time_t day = date(2026, 10, 13, 0, 0);
  CalendarEvent overnight = event(date(2026, 10, 13, 23, 30), date(2026, 10, 14, 1, 0), false);
  assert(calendar_event_on_day(&overnight, day));
  assert(calendar_event_on_day(&overnight, calendar_shift_day(day, 1)));
  assert(!calendar_event_on_day(&overnight, calendar_shift_day(day, 2)));
  CalendarEvent midnight_end = event(day + 3600, calendar_shift_day(day, 1), false);
  assert(!calendar_event_on_day(&midnight_end, calendar_shift_day(day, 1)));
  CalendarEvent all_day = event(day, day + 2 * 86400, true);
  setenv("TZ", "America/Los_Angeles", 1); tzset();
  time_t local_day = date(2026, 10, 13, 0, 0);
  assert(calendar_event_on_day(&all_day, local_day));
  assert(calendar_event_on_day(&all_day, calendar_shift_day(local_day, 1)));
  assert(!calendar_event_on_day(&all_day, calendar_shift_day(local_day, -1)));
  assert(!calendar_event_on_day(&all_day, calendar_shift_day(local_day, 2)));
  time_t spring = date(2026, 3, 8, 0, 0), fall = date(2026, 11, 1, 0, 0);
  assert(calendar_shift_day(spring, 1) - spring == 23 * 3600);
  assert(calendar_shift_day(fall, 1) - fall == 25 * 3600);
  CalendarSnapshot old = { .version = 1, .updated = local_day, .count = 1 };
  old.events[0] = overnight;
  assert(calendar_store_save(&old));
  CalendarSnapshot loaded;
  assert(calendar_store_load(&loaded, local_day + 100)); assert(loaded.count == 1);
  // Fail at each chunk/header/commit write: the previous committed bank must survive.
  CalendarSnapshot replacement = old; strcpy(replacement.events[0].title, "Replacement");
  int write_count = (sizeof(replacement) + 255) / 256 + 2;
  for (int failure = 0; failure < write_count; ++failure) {
    writes_until_failure = failure;
    assert(!calendar_store_save(&replacement));
    assert(calendar_store_load(&loaded, local_day + 100));
    assert(strcmp(loaded.events[0].title, "Synthetic event") == 0);
  }
  writes_until_failure = -1;
  assert(calendar_store_save(&replacement));
  assert(calendar_store_load(&loaded, local_day + 100));
  assert(strcmp(loaded.events[0].title, "Replacement") == 0);
  CalendarSnapshot empty = { .version = 1, .updated = local_day + 100 };
  assert(calendar_store_save(&empty)); assert(calendar_store_load(&loaded, local_day + 110)); assert(!loaded.count);
  assert(!calendar_store_load(&loaded, local_day + 100 + CACHE_TTL + 1)); assert(!persist_exists(100));
  assert(calendar_store_save(&old));
  int active_base = 110 + persist_read_int(100) * 32;
  saved[active_base + 1][10] ^= 1;
  assert(!calendar_store_load(&loaded, local_day + 100)); assert(!persist_exists(100));
  assert(calendar_store_save(&old)); calendar_store_clear(); assert(!calendar_store_load(&loaded, local_day + 100));
  puts("Native date, DST, expiry, corruption and interrupted-cache tests passed.");
  return 0;
}
