#pragma once
#include "fake_idf.h"
typedef struct { size_t size; } esp_partition_t;
#define ESP_PARTITION_TYPE_DATA 1
#define ESP_PARTITION_SUBTYPE_ANY 255
const esp_partition_t *esp_partition_find_first(int type, int subtype, const char *name);
esp_err_t esp_partition_read(const esp_partition_t *p, size_t off, void *buffer, size_t n);
