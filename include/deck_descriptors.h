#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t interface_number, in_address, out_address;
    uint16_t in_mps, out_mps;
} deck_interface_t;
// Finds one alternate-setting-0 HID interface with interrupt IN and OUT.
bool deck_find_interface(const uint8_t *config, size_t length, deck_interface_t *out);
