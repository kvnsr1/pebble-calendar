#pragma once
#include "calendar.h"
#include "room_input.h"
void room_notes_init(void);
void room_notes_deinit(void);
void room_notes_clear(void);
void room_notes_prune(const CalendarSnapshot *snapshot, time_t now);
bool room_notes_get(const CalendarEvent *event, bool demo, time_t now, SpokenRoom *room);
bool room_notes_set(const CalendarEvent *event, bool demo, time_t now, const SpokenRoom *room);
