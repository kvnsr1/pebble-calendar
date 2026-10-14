#include "room_input.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TOKEN_LIMIT 32
static int word_number(const char *word) {
  static const char ones[] = "zero\0one\0two\0three\0four\0five\0six\0seven\0eight\0nine\0ten\0eleven\0twelve\0thirteen\0fourteen\0fifteen\0sixteen\0seventeen\0eighteen\0nineteen";
  static const char tens[] = "twenty\0thirty\0forty\0fifty\0sixty\0seventy\0eighty\0ninety";
  if (!strcmp(word, "oh")) return 0;
  const char *value = ones;
  for (int i = 0; i < 20; ++i, value += strlen(value) + 1) if (!strcmp(word, value)) return i;
  value = tens;
  for (int i = 0; i < 8; ++i, value += strlen(value) + 1) if (!strcmp(word, value)) return (i + 2) * 10;
  return -1;
}
static bool number(char tokens[TOKEN_LIMIT][20], int from, int to, int *out) {
  if (from >= to) return false;
  if (isdigit((unsigned char)tokens[from][0])) {
    if (to != from + 1) return false;
    char *end; long value = strtol(tokens[from], &end, 10);
    if (*end || value < 0 || value > 9999) return false;
    *out = value; return true;
  }
  int first = word_number(tokens[from]);
  if (first < 0) return false;
  if (to == from + 1) { *out = first; return true; }
  if (first >= 20 && to == from + 2) {
    int last = word_number(tokens[from + 1]);
    if (last < 1 || last > 9) return false;
    *out = first + last; return true;
  }
  if (first >= 1 && first <= 9 && !strcmp(tokens[from + 1], "hundred")) {
    if (to == from + 2) { *out = first * 100; return true; }
    int tail = from + 2, remainder;
    if (!strcmp(tokens[tail], "and")) ++tail;
    if (!number(tokens, tail, to, &remainder) || remainder > 99) return false;
    *out = first * 100 + remainder; return true;
  }
  // Dictation often returns room digits individually, e.g. "zero seven".
  int value = 0;
  for (int i = from; i < to; ++i) {
    int digit = word_number(tokens[i]);
    if (digit < 0 || digit > 9 || value > 999) return false;
    value = value * 10 + digit;
  }
  *out = value; return true;
}
bool room_input_valid(const SpokenRoom *room) {
  size_t length = 0;
  while (length < sizeof(room->building) && isupper((unsigned char)room->building[length])) ++length;
  if (!length || length > 4) return false;
  size_t digits = length;
  while (length < sizeof(room->building) && isdigit((unsigned char)room->building[length])) ++length;
  return length > digits && length - digits <= 3 && length < sizeof(room->building) && !room->building[length] && room->floor <= 999 && room->number > 0 && room->number <= 9999;
}
static bool parse(const char *transcription, SpokenRoom *room, char tokens[TOKEN_LIMIT][20]) {
  if (!transcription || !room || !transcription[0] || strlen(transcription) >= 128) return false;
  int count = 0;
  const unsigned char *p = (const unsigned char *)transcription;
  while (*p) {
    if (isspace(*p) || *p == ',' || *p == '-') { ++p; continue; }
    if (count >= TOKEN_LIMIT) return false;
    if (*p == '.') { strcpy(tokens[count++], "."); ++p; continue; }
    if (!isalnum(*p) || *p > 127) return false;
    bool digit = isdigit(*p); int length = 0;
    while (*p && (digit ? isdigit(*p) : isalpha(*p))) {
      if (length == 19) return false;
      tokens[count][length++] = tolower(*p++);
    }
    ++count;
  }
  int cursor = !strcmp(tokens[0], "building") ? 1 : 0;
  char prefix[5] = {0}; int prefix_length = 0;
  if (cursor >= count || !isalpha((unsigned char)tokens[cursor][0]) || word_number(tokens[cursor]) >= 0) return false;
  int length = strlen(tokens[cursor]);
  if (length < 1 || length > 4) return false;
  for (int i = 0; i < length; ++i) prefix[prefix_length++] = toupper((unsigned char)tokens[cursor][i]);
  ++cursor;
  while (cursor < count && strlen(tokens[cursor]) == 1 && isalpha((unsigned char)tokens[cursor][0])) {
    if (prefix_length == 4) return false;
    prefix[prefix_length++] = toupper((unsigned char)tokens[cursor++][0]);
  }
  if (cursor >= count) return false;
  int building_end = cursor + 1, building;
  if (word_number(tokens[cursor]) >= 20 && building_end < count && word_number(tokens[building_end]) > 0 && word_number(tokens[building_end]) < 10) ++building_end;
  if (!number(tokens, cursor, building_end, &building) || building < 1 || building > 999) return false;
  cursor = building_end;
  if (cursor < count && !strcmp(tokens[cursor], "floor")) ++cursor;
  int split = cursor;
  while (split < count && strcmp(tokens[split], ".") && strcmp(tokens[split], "point") && strcmp(tokens[split], "dot") && strcmp(tokens[split], "period")) ++split;
  if (split >= count) return false;
  int floor, room_start = split + 1, room_number;
  if (room_start < count && !strcmp(tokens[room_start], "room")) ++room_start;
  // Ignore a final sentence period, but require an explicit floor/room separator.
  int end = count; if (end > room_start && !strcmp(tokens[end - 1], ".")) --end;
  if (!number(tokens, cursor, split, &floor) || floor > 999 || !number(tokens, room_start, end, &room_number) || room_number < 1) return false;
  SpokenRoom result = {0}; snprintf(result.building, sizeof(result.building), "%s%d", prefix, building);
  result.floor = floor; result.number = room_number;
  if (!room_input_valid(&result)) return false;
  *room = result; return true;
}
bool room_input_parse(const char *transcription, SpokenRoom *room) {
  // App callbacks are serial; keep the 640-byte scratch off the watch's small stack.
  static char tokens[TOKEN_LIMIT][20];
  memset(tokens, 0, sizeof(tokens));
  bool valid = parse(transcription, room, tokens);
  volatile unsigned char *bytes = (volatile unsigned char *)tokens;
  for (size_t i = 0; i < sizeof(tokens); ++i) bytes[i] = 0;
  return valid;
}
