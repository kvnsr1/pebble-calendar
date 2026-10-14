#pragma once
#include <pebble.h>
#include "calendar.h"

Window *event_detail_create(void (*open_options)(void));
void event_detail_open(const CalendarEvent *event, bool cached);
void event_detail_close(void);
void event_detail_destroy(void);
void event_detail_relayout(void);
