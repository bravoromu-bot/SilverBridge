#include "usb_midi.h"
#include <CoreMIDI/CoreMIDI.h>
#include <CoreFoundation/CoreFoundation.h>
#include <libusb.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum { PORT_COUNT = 3 };
static const CFStringRef port_names[PORT_COUNT] = {
    CFSTR("MPC Private"), CFSTR("MPC Public"), CFSTR("MPC Port 1")
};
static volatile sig_atomic_t running = 1;
static MIDIClientRef midi_client;
static MIDIEndpointRef midi_sources[PORT_COUNT];
static MIDIEndpointRef midi_destinations[PORT_COUNT];
static libusb_context *usb_context;
static libusb_device_handle *usb_device;
static int usb_claimed;
static int destination_indexes[PORT_COUNT] = {0, 1, 2};

static void stop_signal(int sig) { (void)sig; running = 0; }

static void output_observer(const MIDIPacketList *list, void *context,
                            void *source_context) {
    (void)source_context;
    int index = *(const int *)context;
    const MIDIPacket *packet = &list->packet[0];
    for (UInt32 p = 0; p < list->numPackets; ++p) {
        fprintf(stderr, "CoreMIDI OUT port=%d bytes=", index);
        for (UInt16 i = 0; i < packet->length; ++i)
            fprintf(stderr, "%02x%s", packet->data[i], i + 1 == packet->length ? "" : " ");
        fputc('\n', stderr);
        packet = MIDIPacketNext(packet);
    }
    /* Deliberately no USB OUT path in this read-only build. */
}

static int create_midi_ports(void) {
    OSStatus rc = MIDIClientCreate(CFSTR("MPC Studio Silver Bridge"), NULL, NULL, &midi_client);
    if (rc) { fprintf(stderr, "MIDIClientCreate: %d\n", (int)rc); return 1; }
    for (int i = 0; i < PORT_COUNT; ++i) {
        rc = MIDISourceCreate(midi_client, port_names[i], &midi_sources[i]);
        if (rc) { fprintf(stderr, "MIDISourceCreate %d: %d\n", i, (int)rc); return 1; }
        rc = MIDIDestinationCreate(midi_client, port_names[i], output_observer,
                                   &destination_indexes[i], &midi_destinations[i]);
        if (rc) { fprintf(stderr, "MIDIDestinationCreate %d: %d\n", i, (int)rc); return 1; }
        MIDIObjectSetStringProperty(midi_sources[i], kMIDIPropertyManufacturer, CFSTR("Akai"));
        MIDIObjectSetStringProperty(midi_sources[i], kMIDIPropertyModel, CFSTR("MPC Studio"));
        MIDIObjectSetStringProperty(midi_destinations[i], kMIDIPropertyManufacturer, CFSTR("Akai"));
        MIDIObjectSetStringProperty(midi_destinations[i], kMIDIPropertyModel, CFSTR("MPC Studio"));
    }
    return 0;
}

static void dispose_midi_ports(void) {
    for (int i = 0; i < PORT_COUNT; ++i) {
        if (midi_sources[i]) MIDIEndpointDispose(midi_sources[i]);
        if (midi_destinations[i]) MIDIEndpointDispose(midi_destinations[i]);
    }
    if (midi_client) MIDIClientDispose(midi_client);
}

static int open_usb(void) {
    int rc = libusb_init(&usb_context);
    if (rc) { fprintf(stderr, "libusb_init: %s\n", libusb_error_name(rc)); return 1; }
    usb_device = libusb_open_device_with_vid_pid(usb_context, 0x09e8, 0x0013);
    if (!usb_device) { fprintf(stderr, "Silver not accessible\n"); return 1; }
    rc = libusb_claim_interface(usb_device, 1);
    if (rc) { fprintf(stderr, "claim interface 1: %s\n", libusb_error_name(rc)); return 1; }
    usb_claimed = 1;
    return 0;
}

static void close_usb(void) {
    if (usb_claimed) libusb_release_interface(usb_device, 1);
    if (usb_device) libusb_close(usb_device);
    if (usb_context) libusb_exit(usb_context);
    usb_claimed = 0; usb_device = NULL; usb_context = NULL;
}

static void midi_from_usb(void *context, uint8_t cable, const uint8_t *midi, size_t n) {
    (void)context;
    if (cable >= PORT_COUNT) return;
    Byte storage[256];
    MIDIPacketList *list = (MIDIPacketList *)storage;
    MIDIPacket *packet = MIDIPacketListInit(list);
    packet = MIDIPacketListAdd(list, sizeof(storage), packet, 0, (UInt16)n, midi);
    if (!packet) return;
    OSStatus rc = MIDIReceived(midi_sources[cable], list);
    if (rc) fprintf(stderr, "MIDIReceived cable=%u status=%d\n", cable, (int)rc);
}

int main(int argc, char **argv) {
    double duration = argc > 1 ? atof(argv[1]) : 10.0;
    if (duration <= 0 || duration > 60) { fprintf(stderr, "duration must be 0..60 seconds\n"); return 2; }
    signal(SIGINT, stop_signal);
    signal(SIGTERM, stop_signal);
    if (create_midi_ports()) { dispose_midi_ports(); return 1; }
    if (open_usb()) { close_usb(); dispose_midi_ports(); return 1; }
    fprintf(stderr, "SilverBridge: USB IN 0x82 -> CoreMIDI, USB OUT disabled\n");
    struct timespec begin;
    clock_gettime(CLOCK_MONOTONIC, &begin);
    unsigned events = 0;
    while (running) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        double elapsed = (double)(now.tv_sec - begin.tv_sec) +
                         (double)(now.tv_nsec - begin.tv_nsec) / 1e9;
        if (elapsed >= duration) break;
        unsigned char buffer[64];
        int received = 0;
        int rc = libusb_bulk_transfer(usb_device, 0x82, buffer, sizeof(buffer), &received, 250);
        if (rc == LIBUSB_ERROR_TIMEOUT) continue;
        if (rc) { fprintf(stderr, "USB IN: %s\n", libusb_error_name(rc)); break; }
        if (received > 0) {
            int count = usb_midi_decode(buffer, (size_t)received, midi_from_usb, NULL);
            if (count < 0) { fprintf(stderr, "Malformed USB-MIDI length %d\n", received); break; }
            events += (unsigned)count;
        }
    }
    fprintf(stderr, "SilverBridge: decoded events=%u\n", events);
    close_usb();
    dispose_midi_ports();
    return 0;
}
