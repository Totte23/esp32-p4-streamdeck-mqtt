#pragma once
#include "esp_err.h"
#include "icon_format.h"
#include "mini_protocol.h"
typedef struct {
    bool ready;
    unsigned count;
    size_t total, used;
    char names[ICON_COUNT_MAX][ICON_NAME_MAX + 1];
    char keys[MINI_KEYS][ICON_NAME_MAX + 1];
} icon_inventory_t;
esp_err_t icon_store_init(void);
esp_err_t icon_store_inventory(icon_inventory_t *out);
esp_err_t icon_store_write(const char *name, const uint8_t *bmp, size_t length);
esp_err_t icon_store_read(const char *name, uint8_t bmp[MINI_BMP_BYTES]);
esp_err_t icon_store_delete(const char *name);
esp_err_t icon_store_assign(unsigned key, const char *name);
bool icon_store_key_image(uint8_t key, uint8_t *bmp, void *context);

esp_err_t icon_store_rgba(const char *name, uint8_t *rgba);

// Seed bundled icons once; existing files are never overwritten.
esp_err_t icon_store_seed(const uint8_t *bundle, size_t length);
