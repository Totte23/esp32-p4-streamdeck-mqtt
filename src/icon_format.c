#include "icon_format.h"
#include "mini_protocol.h"

bool icon_name_valid(const char *name)
{
    if (!name || !*name) return false;
    size_t n = 0;
    for (; name[n] && n <= ICON_NAME_MAX; ++n) {
        unsigned char c = (unsigned char)name[n];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
    }
    return n <= ICON_NAME_MAX;
}
static uint32_t read32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
bool icon_bmp_valid(const uint8_t *p, size_t length)
{
    if (!p || length != MINI_BMP_BYTES) return false;
    return p[0] == 'B' && p[1] == 'M' && read32(p + 2) == MINI_BMP_BYTES &&
        read32(p + 6) == 0 && read32(p + 10) == 54 && read32(p + 14) == 40 &&
        read32(p + 18) == 80 && read32(p + 22) == 80 &&
        p[26] == 1 && p[27] == 0 && p[28] == 24 && p[29] == 0 &&
        read32(p + 30) == 0 && read32(p + 34) == 19200 &&
        read32(p + 46) == 0 && read32(p + 50) == 0;
}
