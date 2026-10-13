#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <time.h>

#define EVENT_LIMIT 10
#define CACHE_TTL (24 * 60 * 60)
#define FLAG_ALL_DAY 1
#define FLAG_TRUNCATED 2

typedef struct {
  int32_t start;
  int32_t end;
  uint8_t flags;
  char title[256];
  char location[128];
  char calendar[64];
} CalendarEvent;

typedef struct {
  uint32_t version;
  int32_t updated;
  uint8_t count;
  bool demo;
  CalendarEvent events[EVENT_LIMIT];
} CalendarSnapshot;

bool calendar_event_valid(const CalendarEvent *event);
int calendar_day_code(time_t instant, bool utc);
time_t calendar_midnight(time_t instant);
time_t calendar_shift_day(time_t day, int offset);
bool calendar_event_on_day(const CalendarEvent *event, time_t day);
int calendar_day_events(const CalendarSnapshot *snapshot, time_t day, int *indices);
bool calendar_expired(const CalendarSnapshot *snapshot, time_t now);
bool calendar_store_load(CalendarSnapshot *snapshot, time_t now);
bool calendar_store_save(const CalendarSnapshot *snapshot);
void calendar_store_clear(void);
