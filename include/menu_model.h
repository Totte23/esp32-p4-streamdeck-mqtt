#pragma once
#include "cJSON.h"
#include <stdbool.h>
#include <stddef.h>
#define MENU_MAX_BYTES 65536
#define MENU_MAX_PAGES 48
#define MENU_MAX_STATES 192
const cJSON *menu_get(const cJSON *obj, const char *key);
const char *menu_string(const cJSON *obj, const char *key, const char *fallback);
bool menu_validate(const cJSON *root, char *error, size_t capacity);
const cJSON *menu_button(const cJSON *page, unsigned physical_key);
void menu_icon_name(const char *filename, char name[33]);

bool menu_action_valid(const cJSON *action);
