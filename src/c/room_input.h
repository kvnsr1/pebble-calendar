#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct { char building[16]; uint16_t floor, number; } SpokenRoom;
bool room_input_parse(const char *transcription, SpokenRoom *room);
bool room_input_valid(const SpokenRoom *room);
