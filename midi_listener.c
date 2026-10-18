#include <CoreMIDI/CoreMIDI.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static void received(const MIDIPacketList *list, void *context, void *source_context) {
    (void)context; (void)source_context;
    const MIDIPacket *packet = &list->packet[0];
    for (UInt32 i = 0; i < list->numPackets; ++i) {
        printf("packet %u", packet->length);
        for (UInt16 j = 0; j < packet->length; ++j) printf(" %02x", packet->data[j]);
        putchar('\n'); fflush(stdout);
        packet = MIDIPacketNext(packet);
    }
}

int main(int argc, char **argv) {
    double duration = argc > 1 ? atof(argv[1]) : 15.0;
    MIDIClientRef client = 0;
    MIDIPortRef port = 0;
    OSStatus rc = MIDIClientCreate(CFSTR("SilverBridge test listener"), NULL, NULL, &client);
    if (rc) { fprintf(stderr, "client: %d\n", (int)rc); return 1; }
    rc = MIDIInputPortCreate(client, CFSTR("Listener"), received, NULL, &port);
    if (rc) { fprintf(stderr, "input port: %d\n", (int)rc); return 1; }
    int found = 0;
    for (ItemCount i = 0; i < MIDIGetNumberOfSources(); ++i) {
        MIDIEndpointRef source = MIDIGetSource(i);
        CFStringRef name = NULL;
        if (MIDIObjectGetStringProperty(source, kMIDIPropertyName, &name) != noErr || !name) continue;
        if (CFStringCompare(name, CFSTR("MPC Public"), 0) == kCFCompareEqualTo) {
            rc = MIDIPortConnectSource(port, source, NULL);
            if (!rc) found = 1;
        }
        CFRelease(name);
    }
    if (!found) { fprintf(stderr, "MPC Public source not found\n"); return 2; }
    fprintf(stderr, "Listening to MPC Public for %.0f seconds\n", duration);
    struct timespec start, now;
    clock_gettime(CLOCK_MONOTONIC, &start);
    do {
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1, false);
        clock_gettime(CLOCK_MONOTONIC, &now);
    } while ((double)(now.tv_sec - start.tv_sec) +
             (double)(now.tv_nsec - start.tv_nsec) / 1e9 < duration);
    MIDIPortDispose(port);
    MIDIClientDispose(client);
    return 0;
}
