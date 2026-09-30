#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    MINI_KEYS = 6, MINI_SIDE = 80, MINI_REPORT_BYTES = 1024,
    MINI_HEADER_BYTES = 16, MINI_PAYLOAD_BYTES = 1008,
    MINI_FEATURE_BYTES = 17, MINI_BMP_BYTES = 54 + 80 * 80 * 3,
    MINI_IMAGE_PAGES = (MINI_BMP_BYTES + MINI_PAYLOAD_BYTES - 1) / MINI_PAYLOAD_BYTES
};

bool mini_supported(uint16_t vid, uint16_t pid);
// Leaves *mask unchanged on malformed input. Accepts padded reports.
bool mini_parse_keys(const uint8_t *report, size_t length, uint8_t *mask);
bool mini_brightness(uint8_t percent, uint8_t report[MINI_FEATURE_BYTES]);
// Image is an already transformed, native 80x80 BMP; key is zero based.
bool mini_image_page(uint8_t key, size_t page, const uint8_t *bmp,
                     size_t length, uint8_t report[MINI_REPORT_BYTES]);
// Built-in numbered tile, prepared in the Mini's native BMP orientation.
void mini_demo_bmp(uint8_t key, bool active, uint8_t bmp[MINI_BMP_BYTES]);
