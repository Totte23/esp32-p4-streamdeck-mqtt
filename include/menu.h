#pragma once
#include "esp_err.h"
#include "mini_protocol.h"
esp_err_t menu_init(void);
esp_err_t menu_upload(const char *json, size_t length, char *error, size_t capacity);
char *menu_export(void);
void menu_key(uint8_t key, bool pressed, void *context);
bool menu_image(uint8_t key, uint8_t *bmp, void *context);
bool menu_uses_icon(const char *name);
void menu_state(const char *id, const char *style, const char *value);
void menu_offline(void);
