#include "usb_midi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct counts { unsigned note_on, pressure, note_off, other; };
static void receive(void *context, uint8_t cable, const uint8_t *midi, size_t n) {
    struct counts *c = context;
    if (cable != 1 || n != 3 || midi[1] != 0x25) { ++c->other; return; }
    if (midi[0] == 0x99) ++c->note_on;
    else if (midi[0] == 0xa9) ++c->pressure;
    else if (midi[0] == 0x89) ++c->note_off;
    else ++c->other;
}

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    FILE *f = fopen(argv[1], "r");
    if (!f) return 2;
    struct counts counts = {0};
    char line[1024];
    unsigned transfers = 0;
    while (fgets(line, sizeof(line), f)) {
        char *cursor = line, *end = NULL;
        strtod(cursor, &end); if (end == cursor) return 3; cursor = end;
        long n = strtol(cursor, &end, 10); if (end == cursor || n < 0 || n > 64) return 3; cursor = end;
        uint8_t bytes[64];
        for (long i = 0; i < n; ++i) {
            long value = strtol(cursor, &end, 16);
            if (end == cursor || value < 0 || value > 255) return 3;
            bytes[i] = (uint8_t)value;
            cursor = end;
        }
        if (usb_midi_decode(bytes, (size_t)n, receive, &counts) < 0) return 3;
        ++transfers;
    }
    fclose(f);
    printf("transfers=%u note_on=%u pressure=%u note_off=%u other=%u\n",
           transfers, counts.note_on, counts.pressure, counts.note_off, counts.other);
    return counts.note_on && counts.note_off && counts.pressure && !counts.other ? 0 : 1;
}
