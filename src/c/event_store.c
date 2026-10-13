#include <pebble.h>
#include "calendar.h"

#define ACTIVE_KEY 100
#define BANK_BASE 110
#define BANK_STRIDE 32
#define CHUNK_SIZE 256
#define STORE_VERSION 1

typedef struct {
  uint32_t version;
  uint32_t checksum;
  uint32_t size;
} StoreHeader;

static uint32_t checksum(const void *data, size_t size) {
  const uint8_t *bytes = data;
  uint32_t result = 2166136261u;
  for (size_t i = 0; i < size; ++i) result = (result ^ bytes[i]) * 16777619u;
  return result;
}

void calendar_store_clear(void) {
  persist_delete(ACTIVE_KEY);
  for (int bank = 0; bank < 2; ++bank)
    for (int key = 0; key < BANK_STRIDE; ++key) persist_delete(BANK_BASE + bank * BANK_STRIDE + key);
}

bool calendar_store_load(CalendarSnapshot *snapshot, time_t now) {
  if (!persist_exists(ACTIVE_KEY)) return false;
  int bank = persist_read_int(ACTIVE_KEY);
  if (bank != 0 && bank != 1) { calendar_store_clear(); return false; }
  int base = BANK_BASE + bank * BANK_STRIDE;
  StoreHeader header;
  if (persist_read_data(base, &header, sizeof(header)) != (int)sizeof(header) ||
      header.version != STORE_VERSION || header.size != sizeof(*snapshot)) {
    calendar_store_clear(); return false;
  }
  for (size_t offset = 0; offset < sizeof(*snapshot); offset += CHUNK_SIZE) {
    size_t length = sizeof(*snapshot) - offset;
    if (length > CHUNK_SIZE) length = CHUNK_SIZE;
    if (persist_read_data(base + 1 + offset / CHUNK_SIZE, (uint8_t *)snapshot + offset, length) != (int)length) {
      calendar_store_clear(); return false;
    }
  }
  if (checksum(snapshot, sizeof(*snapshot)) != header.checksum || snapshot->version != 1 ||
      snapshot->count > EVENT_LIMIT || calendar_expired(snapshot, now)) {
    calendar_store_clear(); return false;
  }
  for (int i = 0; i < snapshot->count; ++i)
    if (!calendar_event_valid(&snapshot->events[i])) { calendar_store_clear(); return false; }
  return true;
}

bool calendar_store_save(const CalendarSnapshot *snapshot) {
  // Double buffering keeps the last complete cache if power/transport is interrupted.
  if (persist_get_max_size() < 2 * sizeof(*snapshot) + 2048) return false;
  int bank = persist_exists(ACTIVE_KEY) && persist_read_int(ACTIVE_KEY) == 0 ? 1 : 0;
  int base = BANK_BASE + bank * BANK_STRIDE;
  persist_delete(base);
  for (size_t offset = 0; offset < sizeof(*snapshot); offset += CHUNK_SIZE) {
    size_t length = sizeof(*snapshot) - offset;
    if (length > CHUNK_SIZE) length = CHUNK_SIZE;
    if (persist_write_data(base + 1 + offset / CHUNK_SIZE, (const uint8_t *)snapshot + offset, length) != (int)length) return false;
  }
  StoreHeader header = { STORE_VERSION, checksum(snapshot, sizeof(*snapshot)), sizeof(*snapshot) };
  if (persist_write_data(base, &header, sizeof(header)) != (int)sizeof(header)) return false;
  if (persist_write_int(ACTIVE_KEY, bank) != (int)sizeof(int32_t)) return false;
  // The previous bank is needed only until the pointer commits, not for ongoing retention.
  int previous = BANK_BASE + (1 - bank) * BANK_STRIDE;
  for (int key = 0; key < BANK_STRIDE; ++key) persist_delete(previous + key);
  return true;
}
