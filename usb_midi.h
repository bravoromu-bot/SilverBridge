#ifndef USB_MIDI_H
#define USB_MIDI_H

#include <stddef.h>
#include <stdint.h>

typedef void (*usb_midi_event_fn)(void *context, uint8_t cable,
                                  const uint8_t *midi, size_t length);

/* Return the number of valid four-byte events, or -1 for malformed length. */
int usb_midi_decode(const uint8_t *data, size_t length,
                    usb_midi_event_fn callback, void *context);

/* Encode raw MIDI bytes as 4-byte USB-MIDI events for one cable.
   Returns bytes written to out (multiple of 4), or -1 if out is too small.
   Running status is not expanded; SysEx may be split across calls only by
   the caller sending complete messages. */
int usb_midi_encode(uint8_t cable, const uint8_t *midi, size_t length,
                    uint8_t *out, size_t capacity);

#endif
