#include "usb_midi.h"

int usb_midi_decode(const uint8_t *data, size_t length,
                    usb_midi_event_fn callback, void *context) {
    static const uint8_t cin_length[16] = {
        0, 0, 2, 3, 3, 1, 2, 3, 3, 3, 3, 3, 2, 2, 3, 1
    };
    if ((!data && length) || length % 4) return -1;
    int count = 0;
    for (size_t i = 0; i < length; i += 4) {
        uint8_t cable = data[i] >> 4;
        uint8_t cin = data[i] & 15;
        size_t n = cin_length[cin];
        if (!n) continue;
        if (callback) callback(context, cable, data + i + 1, n);
        ++count;
    }
    return count;
}

static size_t message_length(uint8_t status) {
    if (status >= 0xf8) return 1;
    if (status >= 0xf0) {
        switch (status) { case 0xf1: case 0xf3: return 2; case 0xf2: return 3; default: return 1; }
    }
    return (status & 0xf0) == 0xc0 || (status & 0xf0) == 0xd0 ? 2 : 3;
}

int usb_midi_encode(uint8_t cable, const uint8_t *midi, size_t length,
                    uint8_t *out, size_t capacity) {
    size_t o = 0, i = 0;
    uint8_t hi = (uint8_t)(cable << 4);
#define EMIT(cin, a, b, c) do { if (o + 4 > capacity) return -1; \
        out[o] = hi | (cin); out[o+1] = (a); out[o+2] = (b); out[o+3] = (c); o += 4; } while (0)
    while (i < length) {
        uint8_t s = midi[i];
        if (s == 0xf0) {
            size_t j = i;
            while (j < length && midi[j] != 0xf7) ++j;
            int closed = j < length;
            size_t end = closed ? j + 1 : length;
            while (end - i > 3) { EMIT(4, midi[i], midi[i+1], midi[i+2]); i += 3; }
            size_t left = end - i;
            if (left == 3 && closed) EMIT(7, midi[i], midi[i+1], midi[i+2]);
            else if (left == 3) EMIT(4, midi[i], midi[i+1], midi[i+2]);
            else if (left == 2) EMIT(6, midi[i], midi[i+1], 0);
            else if (left == 1) EMIT(5, midi[i], 0, 0);
            i = end;
        } else if (s & 0x80) {
            size_t n = message_length(s);
            if (i + n > length) break;
            uint8_t cin = s >= 0xf0 ? (n == 1 ? 0xf : n == 2 ? 2 : 3) : (s >> 4);
            EMIT(cin, s, n > 1 ? midi[i+1] : 0, n > 2 ? midi[i+2] : 0);
            i += n;
        } else {
            ++i; /* stray data byte without status: dropped */
        }
    }
#undef EMIT
    return (int)o;
}
