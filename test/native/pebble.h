#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
bool persist_exists(uint32_t key);
int32_t persist_read_int(uint32_t key);
int persist_read_data(uint32_t key, void *buffer, size_t size);
int persist_write_int(uint32_t key, int32_t value);
int persist_write_data(uint32_t key, const void *buffer, size_t size);
int persist_delete(uint32_t key);
size_t persist_get_max_size(void);
