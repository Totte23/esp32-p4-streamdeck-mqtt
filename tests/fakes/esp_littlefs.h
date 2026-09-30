#pragma once
#include "fake_idf.h"
typedef struct { const char *base_path, *partition_label; bool format_if_mount_failed; } esp_vfs_littlefs_conf_t;
esp_err_t esp_vfs_littlefs_register(const esp_vfs_littlefs_conf_t *cfg);
esp_err_t esp_littlefs_info(const char *label, size_t *total, size_t *used);
