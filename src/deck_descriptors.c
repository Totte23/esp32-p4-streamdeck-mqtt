#include "deck_descriptors.h"

bool deck_find_interface(const uint8_t *config, size_t length, deck_interface_t *out)
{
    if (!config || !out || length < 9 || config[0] < 9 || config[1] != 2) return false;
    size_t total = config[2] | ((size_t)config[3] << 8);
    if (total > length || total < config[0]) return false;
    deck_interface_t candidate = {0}, found = {0};
    bool hid = false, have = false;
    for (size_t pos = config[0]; pos < total;) {
        if (total - pos < 2) return false;
        const uint8_t *d = config + pos;
        if (d[0] < 2 || d[0] > total - pos) return false;
        if (d[1] == 4) {
            if (d[0] < 9) return false;
            candidate = (deck_interface_t){.interface_number = d[2]};
            hid = d[3] == 0 && d[5] == 3;
        } else if (d[1] == 5 && hid) {
            if (d[0] < 7) return false;
            unsigned mps = d[4] | ((unsigned)d[5] << 8);
            if ((d[3] & 3) == 3 && (d[2] & 0x0f) && !(d[2] & 0x70) &&
                mps > 0 && mps <= 1024) {
                if (d[2] & 0x80) {
                    candidate.in_address = d[2]; candidate.in_mps = mps;
                } else {
                    candidate.out_address = d[2]; candidate.out_mps = mps;
                }
            }
            if (!have && candidate.in_address && candidate.out_address) {
                found = candidate; have = true;
            }
        }
        pos += d[0];
    }
    if (have) *out = found;
    return have;
}
