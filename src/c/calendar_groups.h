#pragma once
#include <pebble.h>
#include "calendar.h"
void calendar_groups_init(void (*changed)(void));
void calendar_groups_open(const CalendarSnapshot *snapshot);
void calendar_groups_destroy(void);
