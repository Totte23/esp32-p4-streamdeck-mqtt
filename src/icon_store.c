#include "icon_store.h"
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include "tile_render.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "esp_littlefs.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#ifndef ICON_STORE_BASE
#define ICON_STORE_BASE "/icons"
#endif

static SemaphoreHandle_t mutex;
static bool ready;
static char keys[MINI_KEYS][ICON_NAME_MAX + 1];
static const char *TAG = "icons";

static void image_path(char *out, size_t size, const char *name)
{ snprintf(out, size, ICON_STORE_BASE "/%s.bmp", name); }

static esp_err_t read_locked(const char *name, uint8_t *bmp)
{
    char path[64]; image_path(path, sizeof(path), name);
    FILE *f = fopen(path, "rb");
    if (!f) return ESP_ERR_NOT_FOUND;
    fseek(f, 0, SEEK_END); long size = ftell(f); rewind(f);
    bool ok = false;
    if (size == TILE_RGBA_BYTES) {
        uint8_t *rgba = malloc(TILE_RGBA_BYTES);
        if (rgba) {
            ok = fread(rgba, 1, TILE_RGBA_BYTES, f) == TILE_RGBA_BYTES && !ferror(f);
            if (ok) { tile_background(bmp, 0x18222f); tile_rgba(bmp, rgba, 80); }
            free(rgba);
        }
    } else if (size == MINI_BMP_BYTES) {
        size_t n = fread(bmp, 1, MINI_BMP_BYTES, f);
        ok = !ferror(f) && icon_bmp_valid(bmp, n);
    }
    fclose(f);
    return ok ? ESP_OK : ESP_FAIL;
}

static unsigned list_locked(char names[ICON_COUNT_MAX][ICON_NAME_MAX + 1])
{
    unsigned count = 0;
    DIR *dir = opendir(ICON_STORE_BASE);
    if (!dir) return 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) && count < ICON_COUNT_MAX) {
        size_t n = strlen(entry->d_name);
        if (n <= 4 || n > ICON_NAME_MAX + 4 || strcmp(entry->d_name + n - 4, ".bmp")) continue;
        char name[ICON_NAME_MAX + 1] = {0};
        memcpy(name, entry->d_name, n - 4);
        if (!icon_name_valid(name)) continue;
        if (names) strcpy(names[count], name);
        ++count;
    }
    closedir(dir);
    return count;
}

// Only a truly erased partition may be formatted automatically. A failed
// mount of existing user data must not silently erase the icon library.
static bool partition_blank(const esp_partition_t *partition)
{
    uint8_t block[256];
    for (size_t offset = 0; offset < partition->size; offset += sizeof(block)) {
        if (esp_partition_read(partition, offset, block, sizeof(block)) != ESP_OK) return false;
        for (size_t i = 0; i < sizeof(block); ++i) if (block[i] != 0xff) return false;
    }
    return true;
}

esp_err_t icon_store_init(void)
{
    if (mutex) return ESP_ERR_INVALID_STATE;
    mutex = xSemaphoreCreateMutex();
    if (!mutex) return ESP_ERR_NO_MEM;
    const esp_partition_t *partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                                               ESP_PARTITION_SUBTYPE_ANY, "icons");
    if (!partition) return ESP_ERR_NOT_FOUND;
    esp_vfs_littlefs_conf_t cfg = {
        .base_path = ICON_STORE_BASE, .partition_label = "icons", .format_if_mount_failed = false
    };
    esp_err_t err = esp_vfs_littlefs_register(&cfg);
    if (err != ESP_OK && partition_blank(partition)) {
        cfg.format_if_mount_failed = true;
        ESP_LOGI(TAG, "Initializing empty LittleFS partition");
        err = esp_vfs_littlefs_register(&cfg);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LittleFS unavailable; existing data will not be erased: %s", esp_err_to_name(err));
        return err;
    }
    // Interrupted writes leave only these unreferenced temporary files.
    unlink(ICON_STORE_BASE "/.upload.tmp"); unlink(ICON_STORE_BASE "/.key.tmp");
    for (unsigned k = 0; k < MINI_KEYS; ++k) {
        char path[32]; snprintf(path, sizeof(path), ICON_STORE_BASE "/.key%u", k);
        FILE *f = fopen(path, "rb");
        if (!f) continue;
        size_t n = fread(keys[k], 1, ICON_NAME_MAX, f);
        keys[k][n] = 0;
        if (ferror(f) || fgetc(f) != EOF || (n && !icon_name_valid(keys[k]))) keys[k][0] = 0;
        fclose(f);
    }
    ready = true;
    return ESP_OK;
}

esp_err_t icon_store_inventory(icon_inventory_t *out)
{
    if (!out) return ESP_ERR_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    if (!ready) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(mutex, portMAX_DELAY);
    out->ready = true;
    out->count = list_locked(out->names);
    memcpy(out->keys, keys, sizeof(keys));
    esp_err_t err = esp_littlefs_info("icons", &out->total, &out->used);
    xSemaphoreGive(mutex);
    return err;
}

static esp_err_t atomic_write(const char *tmp, const char *destination, const void *data, size_t size)
{
    FILE *f = fopen(tmp, "wb");
    if (!f) return ESP_FAIL;
    bool ok = fwrite(data, 1, size, f) == size;
    if (fflush(f) != 0) ok = false;
    if (fsync(fileno(f)) != 0) ok = false;
    if (fclose(f) != 0) ok = false;
    if (ok && rename(tmp, destination) == 0) return ESP_OK;
    unlink(tmp);
    return ESP_FAIL;
}

esp_err_t icon_store_write(const char *name, const uint8_t *bmp, size_t length)
{
    if (!bmp || !icon_name_valid(name) || (length != TILE_RGBA_BYTES && !icon_bmp_valid(bmp, length))) return ESP_ERR_INVALID_ARG;
    if (!ready) return ESP_ERR_INVALID_STATE;
    char path[64]; image_path(path, sizeof(path), name);
    xSemaphoreTake(mutex, portMAX_DELAY);
    esp_err_t err;
    if (access(path, F_OK) != 0 && list_locked(NULL) >= ICON_COUNT_MAX) err = ESP_ERR_NO_MEM;
    else err = atomic_write(ICON_STORE_BASE "/.upload.tmp", path, bmp, length);
    xSemaphoreGive(mutex);
    return err;
}

esp_err_t icon_store_read(const char *name, uint8_t *bmp)
{
    if (!icon_name_valid(name) || !bmp) return ESP_ERR_INVALID_ARG;
    if (!ready) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(mutex, portMAX_DELAY);
    esp_err_t err = read_locked(name, bmp);
    xSemaphoreGive(mutex);
    return err;
}

esp_err_t icon_store_assign(unsigned key, const char *name)
{
    if (key >= MINI_KEYS || !name || (*name && !icon_name_valid(name))) return ESP_ERR_INVALID_ARG;
    if (!ready) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(mutex, portMAX_DELAY);
    esp_err_t err = ESP_OK;
    char path[64];
    if (*name) {
        image_path(path, sizeof(path), name);
        if (access(path, F_OK) != 0) err = ESP_ERR_NOT_FOUND;
    }
    if (err == ESP_OK) {
        snprintf(path, sizeof(path), ICON_STORE_BASE "/.key%u", key);
        err = atomic_write(ICON_STORE_BASE "/.key.tmp", path, name, strlen(name));
        if (err == ESP_OK) strcpy(keys[key], name);
    }
    xSemaphoreGive(mutex);
    return err;
}

esp_err_t icon_store_delete(const char *name)
{
    if (!icon_name_valid(name)) return ESP_ERR_INVALID_ARG;
    if (!ready) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(mutex, portMAX_DELAY);
    esp_err_t err = ESP_OK;
    for (unsigned k = 0; k < MINI_KEYS; ++k)
        if (!strcmp(keys[k], name)) err = ESP_ERR_INVALID_STATE;
    if (err == ESP_OK) {
        char path[64]; image_path(path, sizeof(path), name);
        if (unlink(path) != 0) err = errno == ENOENT ? ESP_ERR_NOT_FOUND : ESP_FAIL;
    }
    xSemaphoreGive(mutex);
    return err;
}

bool icon_store_key_image(uint8_t key, uint8_t *bmp, void *context)
{
    (void)context;
    if (!ready || key >= MINI_KEYS) return false;
    xSemaphoreTake(mutex, portMAX_DELAY);
    bool ok = keys[key][0] && read_locked(keys[key], bmp) == ESP_OK;
    xSemaphoreGive(mutex);
    return ok;
}

esp_err_t icon_store_rgba(const char *name, uint8_t *rgba)
{
    if (!rgba || !icon_name_valid(name)) return ESP_ERR_INVALID_ARG;
    if (!ready) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(mutex, portMAX_DELAY);
    char path[64]; image_path(path, sizeof(path), name);
    FILE *f = fopen(path, "rb");
    esp_err_t err = ESP_ERR_NOT_FOUND;
    if (f) {
        fseek(f, 0, SEEK_END); long size = ftell(f); rewind(f);
        err = ESP_FAIL;
        if (size == TILE_RGBA_BYTES) {
            if (fread(rgba, 1, TILE_RGBA_BYTES, f) == TILE_RGBA_BYTES && !ferror(f)) err = ESP_OK;
        } else if (size == MINI_BMP_BYTES) {
            uint8_t *bmp = malloc(MINI_BMP_BYTES);
            if (bmp) {
                if (fread(bmp, 1, MINI_BMP_BYTES, f) == MINI_BMP_BYTES && icon_bmp_valid(bmp, MINI_BMP_BYTES)) {
                    for(unsigned y=0;y<80;y++) for(unsigned x=0;x<80;x++) {
                        unsigned d=(y*80+x)*4, p=54+((79-x)*80+y)*3;
                        rgba[d]=bmp[p+2]; rgba[d+1]=bmp[p+1]; rgba[d+2]=bmp[p]; rgba[d+3]=255;
                    }
                    err = ESP_OK;
                }
                free(bmp);
            } else err = ESP_ERR_NO_MEM;
        }
        fclose(f);
    }
    xSemaphoreGive(mutex);
    return err;
}

esp_err_t icon_store_seed(const uint8_t *bundle, size_t length)
{
    const size_t entry_size = 33 + TILE_RGBA_BYTES;
    if (!bundle || length < 9 || memcmp(bundle, "SDICONS1", 8) ||
        bundle[8] > ICON_COUNT_MAX || length != 9 + bundle[8] * entry_size)
        return ESP_ERR_INVALID_ARG;
    // Validate the complete directory before writing any of its entries.
    for (unsigned i = 0; i < bundle[8]; ++i) {
        const char *name = (const char *)bundle + 9 + i * entry_size;
        if (!memchr(name, 0, 33) || !icon_name_valid(name)) return ESP_ERR_INVALID_ARG;
        for (unsigned j = 0; j < i; ++j)
            if (!strcmp(name, (const char *)bundle + 9 + j * entry_size)) return ESP_ERR_INVALID_ARG;
    }
    if (!ready) return ESP_ERR_INVALID_STATE;
    esp_err_t err = ESP_OK;
    xSemaphoreTake(mutex, portMAX_DELAY);
    for (unsigned i = 0; i < bundle[8]; ++i) {
        const char *name = (const char *)bundle + 9 + i * entry_size;
        char path[64]; image_path(path, sizeof(path), name);
        if (access(path, F_OK) == 0) continue;
        if (list_locked(NULL) >= ICON_COUNT_MAX) { err = ESP_ERR_NO_MEM; break; }
        err = atomic_write(ICON_STORE_BASE "/.upload.tmp", path, name + 33, TILE_RGBA_BYTES);
        if (err != ESP_OK) break;
    }
    xSemaphoreGive(mutex);
    return err;
}
