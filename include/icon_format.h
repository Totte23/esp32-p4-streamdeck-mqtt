#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define ICON_NAME_MAX 32
#define ICON_COUNT_MAX 64
bool icon_name_valid(const char *name);
bool icon_bmp_valid(const uint8_t *data, size_t length);
