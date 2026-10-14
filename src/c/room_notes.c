#include <pebble.h>
#include <string.h>
#include "room_notes.h"
#define ROOM_BASE 300
#define ROOM_VERSION 1

typedef struct {
  uint64_t fingerprint;
  uint32_t version;
  int32_t start, end, updated;
  SpokenRoom room;
  bool demo;
  uint32_t checksum;
} RoomNote;
static RoomNote s_notes[EVENT_LIMIT];
static uint32_t checksum(const RoomNote *note) {
  const uint8_t *bytes = (const uint8_t *)note; uint32_t result = 2166136261u;
  for (size_t i = 0; i < offsetof(RoomNote, checksum); ++i) result = (result ^ bytes[i]) * 16777619u;
  return result;
}
static uint64_t fingerprint(const CalendarEvent *event) {
  uint64_t hash = UINT64_C(14695981039346656037);
  const char *fields[] = {event->calendar, event->title};
  for (size_t i = 0; i < 2; ++i) {
    const unsigned char *p = (const unsigned char *)fields[i];
    do { hash = (hash ^ *p) * UINT64_C(1099511628211); } while (*p++);
  }
  return hash;
}
static bool valid(const RoomNote *note, time_t now) {
  return note->version == ROOM_VERSION && note->checksum == checksum(note) && note->start > 0 && note->end > note->start &&
    note->updated > 0 && now - note->updated <= CACHE_TTL && note->updated - now <= 300 && room_input_valid(&note->room);
}
static bool matches(const RoomNote *note, const CalendarEvent *event, bool demo) {
  return note->version == ROOM_VERSION && note->demo == demo && note->fingerprint == fingerprint(event) && note->start == event->start && note->end == event->end;
}
void room_notes_init(void) {
  memset(s_notes, 0, sizeof(s_notes));
  for (int i = 0; i < EVENT_LIMIT; ++i) {
    if (persist_read_data(ROOM_BASE + i, &s_notes[i], sizeof(RoomNote)) != (int)sizeof(RoomNote) || !valid(&s_notes[i], time(NULL))) {
      memset(&s_notes[i], 0, sizeof(RoomNote)); persist_delete(ROOM_BASE + i);
    }
  }
}
void room_notes_deinit(void) { memset(s_notes, 0, sizeof(s_notes)); }
void room_notes_clear(void) {
  room_notes_deinit(); for (int i = 0; i < EVENT_LIMIT; ++i) persist_delete(ROOM_BASE + i);
}
void room_notes_prune(const CalendarSnapshot *snapshot, time_t now) {
  for (int i = 0; i < EVENT_LIMIT; ++i) {
    if (!s_notes[i].version) continue;
    bool found = false;
    if (valid(&s_notes[i], now))
      for (int j = 0; j < snapshot->count; ++j) if (matches(&s_notes[i], &snapshot->events[j], snapshot->demo)) { found = true; break; }
    if (!found) { memset(&s_notes[i], 0, sizeof(RoomNote)); persist_delete(ROOM_BASE + i); }
  }
}
bool room_notes_get(const CalendarEvent *event, bool demo, time_t now, SpokenRoom *room) {
  for (int i = 0; i < EVENT_LIMIT; ++i)
    if (valid(&s_notes[i], now) && matches(&s_notes[i], event, demo)) { *room = s_notes[i].room; return true; }
  return false;
}
bool room_notes_set(const CalendarEvent *event, bool demo, time_t now, const SpokenRoom *room) {
  if (!room_input_valid(room) || now <= 0) return false;
  int slot = -1;
  for (int i = 0; i < EVENT_LIMIT; ++i) {
    if (matches(&s_notes[i], event, demo)) { slot = i; break; }
    if (slot < 0 && !valid(&s_notes[i], now)) slot = i;
  }
  if (slot < 0) return false;
  RoomNote note; memset(&note, 0, sizeof(note)); note.version = ROOM_VERSION; note.fingerprint = fingerprint(event); note.start = event->start; note.end = event->end;
  note.updated = now; note.demo = demo; note.room = *room; note.checksum = checksum(&note);
  if (persist_write_data(ROOM_BASE + slot, &note, sizeof(note)) != (int)sizeof(note)) return false;
  memcpy(&s_notes[slot], &note, sizeof(note)); return true;
}
