#pragma once
#include <pebble.h>
#include "calendar.h"
Window *day_timeline_create(void (*open_event)(const CalendarEvent *, time_t), void (*options)(void));
void day_timeline_update(const CalendarSnapshot *snapshot, const int *indices, int count, time_t day);
void day_timeline_open(void);
void day_timeline_close(void);
void day_timeline_destroy(void);
void day_timeline_relayout(void);
