#include "room_notes.h"
#include <pebble.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned char saved[512][256];
static int lengths[512], writes_until_failure = -1;
bool persist_exists(uint32_t key) { assert(key < 512); return lengths[key] > 0; }
int32_t persist_read_int(uint32_t key) { int32_t value = -1; persist_read_data(key, &value, sizeof(value)); return value; }
int persist_read_data(uint32_t key, void *buffer, size_t size) {
  assert(key < 512);
  if (lengths[key] == 0) return -1;
  int length = lengths[key] < (int)size ? lengths[key] : (int)size;
  memcpy(buffer, saved[key], length); return length;
}
int persist_write_data(uint32_t key, const void *buffer, size_t size) {
  assert(key < 512 && size <= 256);
  if (writes_until_failure == 0) return -1;
  if (writes_until_failure > 0) --writes_until_failure;
  memcpy(saved[key], buffer, size); lengths[key] = size; return size;
}
int persist_write_int(uint32_t key, int32_t value) { return persist_write_data(key, &value, sizeof(value)); }
int persist_delete(uint32_t key) { assert(key < 512); lengths[key] = 0; return 0; }
size_t persist_get_max_size(void) { return 1024 * 1024; }


static void parsed(const char *spoken, const char *building, int floor, int number) {
  SpokenRoom room = {0}; assert(room_input_parse(spoken, &room));
  assert(!strcmp(room.building, building)); assert(room.floor == floor && room.number == number);
}
int main(void) {
  parsed("MB2 3.07", "MB2", 3, 7);
  parsed("MB4 9.17", "MB4", 9, 17);
  parsed("M B two three point seven", "MB2", 3, 7);
  parsed("Building MB two three dot zero seven.", "MB2", 3, 7);
  parsed("MB2 twenty one point one seven", "MB2", 21, 17);
  parsed("MB2 one hundred and two point thirty four", "MB2", 102, 34);
  parsed("AB10 0.101", "AB10", 0, 101);
  SpokenRoom room = {0};
  const char *bad[] = {"", "9.17", "MB2 3 7", "MB2 point seven", "MB2 3.0", "MB2 1000.7", "MB2 3.10000", "MB0 3.07", "MB2 3.07 extra", "MB2 3..7", "MB2 three point", "MB2 three point negative seven"};
  for (size_t i = 0; i < sizeof(bad)/sizeof(*bad); ++i) assert(!room_input_parse(bad[i], &room));
  char huge[140]; memset(huge, 'a', sizeof(huge)); huge[139] = 0; assert(!room_input_parse(huge, &room));
  assert(calendar_group("UBER") == CalendarGroupWork);
  assert(calendar_group("Synthetic Uber calendar") == CalendarGroupWork);
  assert(calendar_group("Gmail - Personal") == CalendarGroupPersonal);
  assert(calendar_group("Gmailish") == CalendarGroupOther);
  assert(calendar_group("Unknown") == CalendarGroupOther);
  assert(calendar_group_restore(calendar_name_hash("Synthetic calendar"), CalendarGroupPersonal));
  assert(calendar_group("Synthetic calendar") == CalendarGroupPersonal);
  assert(!calendar_group_restore(0, CalendarGroupWork));
  calendar_groups_reset();
  time_t now = time(NULL), day = calendar_midnight(now);
  assert(calendar_day_relation(day, now) == 0);
  assert(calendar_day_relation(calendar_shift_day(day, -1), now) == -1);
  assert(calendar_day_relation(calendar_shift_day(day, 1), now) == 1);
  CalendarEvent event = {.start = day + 3600, .end = day + 7200};
  strcpy(event.title, "Synthetic planning review"); strcpy(event.calendar, "Uber");
  CalendarSnapshot snapshot = {.version = SNAPSHOT_VERSION, .updated = now, .count = 3};
  snapshot.events[0] = event; snapshot.events[1] = event; snapshot.events[2] = event;
  strcpy(snapshot.events[1].calendar, "Gmail"); strcpy(snapshot.events[2].calendar, "Unknown");
  int counts[3]; calendar_day_counts(&snapshot, day, counts); assert(counts[0] == 1 && counts[1] == 1 && counts[2] == 1);
  calendar_day_counts(&snapshot, calendar_shift_day(day, -1), counts); assert(!counts[0] && !counts[1] && !counts[2]);
  room_notes_init(); assert(!room_notes_get(&event, false, now, &room));
  assert(room_input_parse("MB2 3.07", &room)); assert(room_notes_set(&event, false, now, &room));
  SpokenRoom loaded; assert(room_notes_get(&event, false, now, &loaded)); assert(loaded.floor == 3 && loaded.number == 7);
  room_notes_deinit(); room_notes_init(); assert(room_notes_get(&event, false, now, &loaded));
  assert(!room_notes_get(&event, true, now, &loaded));
  CalendarEvent other = event; other.start++; assert(!room_notes_get(&other, false, now, &loaded));
  other = event; other.end++; assert(!room_notes_get(&other, false, now, &loaded));
  other = event; strcpy(other.title, "Different event"); assert(!room_notes_get(&other, false, now, &loaded));
  other = event; strcpy(other.calendar, "Gmail"); assert(!room_notes_get(&other, false, now, &loaded));
  assert(!room_notes_get(&event, false, now + CACHE_TTL + 1, &loaded));
  room_notes_prune(&snapshot, now); assert(room_notes_get(&event, false, now, &loaded));
  snapshot.count = 0; room_notes_prune(&snapshot, now); assert(!room_notes_get(&event, false, now, &loaded));
  assert(room_notes_set(&event, false, now, &room));
  saved[300][0] ^= 1; room_notes_deinit(); room_notes_init(); assert(!room_notes_get(&event, false, now, &loaded));
  assert(room_notes_set(&event, false, now, &room)); writes_until_failure = 0;
  SpokenRoom replacement = room; replacement.number = 9; assert(!room_notes_set(&event, false, now, &replacement));
  assert(room_notes_get(&event, false, now, &loaded) && loaded.number == 7); writes_until_failure = -1;
  calendar_store_clear(); assert(!room_notes_get(&event, false, now, &loaded) && !persist_exists(300));
  puts("Native room dictation parsing, calendar grouping, annotation persistence and privacy checks passed.");
  return 0;
}
