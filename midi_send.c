#include <CoreMIDI/CoreMIDI.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* usage: midi-send "<entity name>" hexbyte... */
int main(int argc, char **argv) {
    if (argc < 3) return 2;
    MIDIClientRef c; MIDIPortRef p;
    MIDIClientCreate(CFSTR("send"), NULL, NULL, &c);
    MIDIOutputPortCreate(c, CFSTR("o"), &p);
    unsigned char data[256]; int n = 0;
    for (int i = 2; i < argc; ++i) data[n++] = (unsigned char)strtoul(argv[i], NULL, 16);
    for (ItemCount i = 0; i < MIDIGetNumberOfDestinations(); ++i) {
        MIDIEndpointRef d = MIDIGetDestination(i);
        CFStringRef name = NULL; char buf[128] = "";
        MIDIObjectGetStringProperty(d, kMIDIPropertyName, &name);
        if (name) { CFStringGetCString(name, buf, sizeof buf, kCFStringEncodingUTF8); CFRelease(name); }
        if (strcmp(buf, argv[1])) continue;
        Byte st[512]; MIDIPacketList *l = (MIDIPacketList *)st;
        MIDIPacket *k = MIDIPacketListInit(l);
        MIDIPacketListAdd(l, sizeof st, k, 0, n, data);
        printf("send to %s rc=%d\n", buf, (int)MIDISend(p, d, l));
    }
    return 0;
}
