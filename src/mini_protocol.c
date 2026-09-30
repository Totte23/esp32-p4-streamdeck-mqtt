#include "mini_protocol.h"
#include <string.h>

bool mini_supported(uint16_t vid, uint16_t pid)
{
    return vid == 0x0fd9 && (pid == 0x0063 || pid == 0x0090);
}

bool mini_parse_keys(const uint8_t *report, size_t length, uint8_t *mask)
{
    if (!report || !mask || length < 1 + MINI_KEYS || report[0] != 1) return false;
    uint8_t next = 0;
    for (unsigned k = 0; k < MINI_KEYS; ++k) {
        if (report[k + 1] > 1) return false;
        next |= report[k + 1] << k;
    }
    *mask = next;
    return true;
}

bool mini_brightness(uint8_t percent, uint8_t report[MINI_FEATURE_BYTES])
{
    if (!report || percent > 100) return false;
    memset(report, 0, MINI_FEATURE_BYTES);
    const uint8_t prefix[] = {0x05, 0x55, 0xaa, 0xd1, 0x01};
    memcpy(report, prefix, sizeof(prefix));
    report[5] = percent;
    return true;
}

bool mini_image_page(uint8_t key, size_t page, const uint8_t *bmp,
                     size_t length, uint8_t report[MINI_REPORT_BYTES])
{
    if (!bmp || !report || key >= MINI_KEYS || length != MINI_BMP_BYTES ||
        page >= MINI_IMAGE_PAGES) return false;
    size_t offset = page * MINI_PAYLOAD_BYTES;
    size_t count = length - offset;
    if (count > MINI_PAYLOAD_BYTES) count = MINI_PAYLOAD_BYTES;
    memset(report, 0, MINI_REPORT_BYTES);
    report[0] = 2;
    report[1] = 1;
    report[2] = (uint8_t)page;
    report[4] = offset + count == length;
    report[5] = key + 1;
    memcpy(report + MINI_HEADER_BYTES, bmp + offset, count);
    return true;
}

static void le32(uint8_t *p, uint32_t v)
{
    for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (i * 8));
}

void mini_demo_bmp(uint8_t key, bool active, uint8_t bmp[MINI_BMP_BYTES])
{
    // Original 3x5 bitmaps, one row per byte. Top stripe makes orientation visible.
    static const uint8_t digits[6][5] = {
        {2,6,2,2,7}, {7,1,7,4,7}, {7,1,7,1,7},
        {5,5,7,1,1}, {7,4,7,1,7}, {7,4,7,5,7}
    };
    memset(bmp, 0, MINI_BMP_BYTES);
    bmp[0] = 'B'; bmp[1] = 'M';
    le32(bmp + 2, MINI_BMP_BYTES);
    le32(bmp + 10, 54); le32(bmp + 14, 40);
    le32(bmp + 18, MINI_SIDE); le32(bmp + 22, MINI_SIDE);
    bmp[26] = 1; bmp[28] = 24;
    le32(bmp + 34, MINI_SIDE * MINI_SIDE * 3);
    for (unsigned y = 0; y < MINI_SIDE; ++y) {
        for (unsigned x = 0; x < MINI_SIDE; ++x) {
            uint8_t r = active ? 16 : 18;
            uint8_t g = active ? 150 : 42;
            uint8_t b = active ? 85 : 95;
            bool ink = y >= 6 && y < 9 && x >= 8 && x < 72;
            if (key < MINI_KEYS && x >= 28 && x < 52 && y >= 20 && y < 60)
                ink |= (digits[key][(y - 20) / 8] >> (2 - (x - 28) / 8)) & 1;
            if (ink) r = g = b = 245;
            // Python StreamDeck transform: rotate(90 CCW), then vertical flip;
            // followed by BMP bottom-up row storage. Source (x,y) -> file (y,79-x).
            size_t p = 54 + ((MINI_SIDE - 1 - x) * MINI_SIDE + y) * 3;
            bmp[p] = b; bmp[p + 1] = g; bmp[p + 2] = r;
        }
    }
}
