#include "mini_protocol.h"
#include "deck_descriptors.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static void test_input(void)
{
    uint8_t report[65] = {1,1,0,1,0,1,0}, mask = 0;
    assert(mini_parse_keys(report, 7, &mask) && mask == 0x15);
    assert(mini_parse_keys(report, 17, &mask) && mask == 0x15);
    assert(mini_parse_keys(report, 65, &mask) && mask == 0x15);
    for (size_t n = 0; n < 7; ++n) {
        mask = 42; assert(!mini_parse_keys(report, n, &mask)); assert(mask == 42);
    }
    report[0] = 2; assert(!mini_parse_keys(report, 65, &mask));
    report[0] = 1; report[6] = 2; assert(!mini_parse_keys(report, 65, &mask));
    assert(!mini_parse_keys(NULL, 65, &mask));
    assert(!mini_parse_keys(report, 65, NULL));
    for (unsigned m = 0; m < 64; ++m) {
        for (unsigned k = 0; k < 6; ++k) report[k + 1] = (m >> k) & 1;
        assert(mini_parse_keys(report, 65, &mask) && mask == m);
    }
    assert(mini_supported(0x0fd9, 0x0063));
    assert(mini_supported(0x0fd9, 0x0090));
    assert(!mini_supported(0x0fd9, 0x0060));
    assert(!mini_supported(0xffff, 0x0063));
}

static void test_output(void)
{
    uint8_t feature[17];
    for (unsigned b = 0; b <= 100; ++b) {
        memset(feature, 0xcc, sizeof(feature));
        assert(mini_brightness(b, feature));
        const uint8_t prefix[] = {5,0x55,0xaa,0xd1,1};
        assert(!memcmp(feature, prefix, sizeof(prefix)) && feature[5] == b);
        for (unsigned i = 6; i < sizeof(feature); ++i) assert(!feature[i]);
    }
    assert(!mini_brightness(101, feature));
    assert(!mini_brightness(255, feature));
    static uint8_t bmp[MINI_BMP_BYTES], recovered[MINI_BMP_BYTES];
    uint8_t report[1024];
    for (size_t i = 0; i < sizeof(bmp); ++i) bmp[i] = (uint8_t)(i * 37);
    for (unsigned k = 0; k < 6; ++k) {
        for (size_t page = 0; page < MINI_IMAGE_PAGES; ++page) {
            memset(report, 0xcc, sizeof(report));
            assert(mini_image_page(k, page, bmp, sizeof(bmp), report));
            assert(report[0] == 2 && report[1] == 1 && report[2] == page);
            assert(report[3] == 0 && report[4] == (page == 19) && report[5] == k + 1);
            for (unsigned i = 6; i < 16; ++i) assert(report[i] == 0);
            size_t offset = page * 1008, count = sizeof(bmp) - offset;
            if (count > 1008) count = 1008;
            memcpy(recovered + offset, report + 16, count);
            for (size_t i = 16 + count; i < sizeof(report); ++i) assert(report[i] == 0);
        }
        assert(!memcmp(bmp, recovered, sizeof(bmp)));
    }
    assert(!mini_image_page(6, 0, bmp, sizeof(bmp), report));
    assert(!mini_image_page(0, 20, bmp, sizeof(bmp), report));
    assert(!mini_image_page(0, SIZE_MAX, bmp, sizeof(bmp), report));
    assert(!mini_image_page(0, 0, bmp, 0, report));
    assert(!mini_image_page(0, 0, NULL, sizeof(bmp), report));
    mini_demo_bmp(0, false, bmp);
    assert(bmp[0] == 'B' && bmp[1] == 'M');
    assert(le32(bmp + 2) == sizeof(bmp) && le32(bmp + 10) == 54);
    assert(le32(bmp + 18) == 80 && le32(bmp + 22) == 80);
    assert(bmp[26] == 1 && bmp[28] == 24 && le32(bmp + 34) == 19200);
    mini_demo_bmp(0, true, recovered);
    assert(memcmp(bmp + 54, recovered + 54, 19200) != 0);
    // Inverse-transform the native image: top stripe at source (10,7),
    // not at bottom (10,72). Catches flipped/rotated BMP regression.
    size_t stripe = 54 + ((79 - 10) * 80 + 7) * 3;
    size_t bottom = 54 + ((79 - 10) * 80 + 72) * 3;
    assert(bmp[stripe] == 245 && bmp[bottom] == 95);
}

static void test_descriptors(void)
{
    uint8_t descriptor[] = {
        9,2,41,0,1,1,0,0x80,100,
        9,4,2,0,2,3,0,0,0,
        9,0x21,0x11,1,0,1,0x22,42,0,
        7,5,0x81,3,64,0,1,
        7,5,0x02,3,0,4,1
    };
    deck_interface_t iface;
    assert(deck_find_interface(descriptor, sizeof(descriptor), &iface));
    assert(iface.interface_number == 2 && iface.in_address == 0x81 && iface.out_address == 2);
    assert(iface.in_mps == 64 && iface.out_mps == 1024);
    for (size_t n = 0; n < sizeof(descriptor); ++n)
        assert(!deck_find_interface(descriptor, n, &iface));
    descriptor[9] = 0; assert(!deck_find_interface(descriptor, sizeof(descriptor), &iface));
    descriptor[9] = 9; descriptor[12] = 1;
    assert(!deck_find_interface(descriptor, sizeof(descriptor), &iface));
    descriptor[12] = 0; descriptor[14] = 0xff;
    assert(!deck_find_interface(descriptor, sizeof(descriptor), &iface));
    descriptor[14] = 3; descriptor[39] = 0x14; // high-bandwidth multiplier unsupported
    assert(!deck_find_interface(descriptor, sizeof(descriptor), &iface));
    descriptor[39] = 4; descriptor[34] = 8; // descriptor longer than remaining buffer
    assert(!deck_find_interface(descriptor, sizeof(descriptor), &iface));
    assert(!deck_find_interface(NULL, 0, &iface));
    // Endpoints across different interfaces must never be paired.
    uint8_t split[] = {
        9,2,41,0,2,1,0,0x80,100,
        9,4,0,0,1,3,0,0,0, 7,5,0x81,3,64,0,1,
        9,4,1,0,1,3,0,0,0, 7,5,0x02,3,0,4,1
    };
    assert(!deck_find_interface(split, sizeof(split), &iface));
}

int main(void)
{
    test_input(); test_output(); test_descriptors();
    puts("PASS: keys, brightness, BMP, all 20 image pages, USB descriptors");
    return 0;
}
