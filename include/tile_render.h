#pragma once
#include "mini_protocol.h"
#define TILE_RGBA_BYTES (80 * 80 * 4)
void tile_background(uint8_t *bmp, uint32_t rgb);
void tile_rgba(uint8_t *bmp, const uint8_t *rgba, unsigned size);
void tile_text(uint8_t *bmp, const char *text, int y, unsigned scale, uint32_t rgb);
void tile_arrow(uint8_t *bmp, bool next);

void tile_rgba_at(uint8_t *bmp, const uint8_t *rgba, unsigned size, unsigned top);

void tile_rect(uint8_t *bmp, int x, int y, int width, int height, uint32_t rgb);
