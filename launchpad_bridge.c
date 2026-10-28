/* Launchpad Pro MK3 <-> MPC bridge.

   The Launchpad is kept in Programmer layout (pad row r, col c = note 10*r+c) and the bridge draws the layout:

   DRUM mode   (Launchpad "Session" button): 4x4 block bottom-left mirrors the MPC pads. MPC's own pad lights
               (tapped from the driver over loopback UDP) and Silver pad hits are reproduced; Launchpad presses
               play the MPC drum pads (channel 10).
   NOTE mode   (Launchpad "Note" button): chromatic isomorphic layout (+1 semitone per column, +5 per row),
               roots blue, scale grey. Presses play real notes (channel 1) into MPC; notes MPC sends to the
               virtual port "SilverBridge Launchpad" light up green. Up/Down arrows shift the octave.

   MPC receives the Launchpad through the virtual source "SilverBridge Launchpad In" (enable it as a MIDI input). */
#include <CoreMIDI/CoreMIDI.h>
#include <CoreFoundation/CoreFoundation.h>
#include <arpa/inet.h>
#include <dispatch/dispatch.h>
#include <netinet/in.h>
#include <libproc.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>

enum { MODE_DRUM = 0, MODE_NOTE = 1 };

static MIDIClientRef client;
static MIDIPortRef in_port, lp_in_port, out_port;
static MIDIEndpointRef lp_dest, lp_daw, virtual_dest, virtual_src;
static int trace = 0;
static volatile int native = 0;
static int auto_mode = 1;
static int watch_mpc = 1;     /* release the Launchpad when MPC quits */
static int fixed_drum = 0;    /* started with --no-auto in drum layout: restore it when MPC comes back */   /* switch drum <-> native Note mode from the pattern of MPC pad lights */   /* --native-note: keep the Launchpad in its own Note mode, light it by channel-16 note injection */
static volatile sig_atomic_t running = 1;
static volatile int mode = MODE_DRUM;
static volatile int octave = 0;           /* note mode octave shift */
static int use_mpc_port = 1, use_input = 1;
static volatile unsigned generation[100];
static volatile unsigned char held[128];
static volatile time_t held_since[128];  /* notes MPC is currently playing (note mode) */

static const unsigned char mpc_pad_notes[16] = { 37, 36, 42, 82, 40, 38, 46, 44, 48, 47, 45, 43, 49, 55, 51, 53 };
static const unsigned char major[12] = { 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1 };

static void name_of(MIDIObjectRef o, char *b, size_t n) {
    CFStringRef s = NULL; b[0] = 0;
    if (MIDIObjectGetStringProperty(o, kMIDIPropertyDisplayName, &s) == noErr && s) {
        CFStringGetCString(s, b, (CFIndex)n, kCFStringEncodingUTF8); CFRelease(s);
    }
}

static void lp_send(const unsigned char *d, size_t n) {
    Byte buf[256]; MIDIPacketList *l = (MIDIPacketList *)buf;
    MIDIPacket *p = MIDIPacketListInit(l);
    MIDIPacketListAdd(l, sizeof buf, p, 0, n, d);
    MIDISend(out_port, lp_dest, l);
}

/* Native Note mode feedback: notes sent on channel 16 to the DAW port light the matching pads. */
static void inject(int on, unsigned note, const char *src) {
    if (trace) fprintf(stderr, "inject %s note=%u from %s\n", on ? "ON " : "off", note, src);
    if (!lp_daw || note > 127) return;
    if (note < 128) { held[note] = on ? 1 : 0; held_since[note] = time(NULL); }
    unsigned char m[3] = { on ? 0x9f : 0x8f, (unsigned char)note, on ? 100 : 0 };
    Byte buf[64]; MIDIPacketList *l = (MIDIPacketList *)buf;
    MIDIPacket *p = MIDIPacketListInit(l);
    MIDIPacketListAdd(l, sizeof buf, p, 0, 3, m);
    MIDISend(out_port, lp_daw, l);
}

static void to_mpc(unsigned char st, unsigned char a, unsigned char b) {
    if (!use_input || !virtual_src) return;
    Byte buf[64]; MIDIPacketList *l = (MIDIPacketList *)buf;
    MIDIPacket *p = MIDIPacketListInit(l);
    unsigned char d[3] = { st, a, b };
    MIDIPacketListAdd(l, sizeof buf, p, 0, 3, d);
    MIDIReceived(virtual_src, l);
}

static int valid_pad(int pad) { int r = pad / 10, c = pad % 10; return r >= 1 && r <= 8 && c >= 1 && c <= 8; }

static int note_of_pad(int pad) { return 36 + 12 * octave + (pad % 10 - 1) + 5 * (pad / 10 - 1); }

static unsigned char layout_color(int note) {
    if (note < 0 || note > 127) return 0;
    if (note % 12 == 0) return 45;       /* root: blue */
    return major[note % 12] ? 2 : 0;     /* scale: grey, others off */
}

static void pad_color(int pad, unsigned char color) { unsigned char m[3] = { 0x90, (unsigned char)pad, color }; ++generation[pad]; lp_send(m, 3); }

static void pad_rgb(unsigned char pad, unsigned v) { /* Silver LED value = 0bRRGGBB */
    unsigned char m[] = { 0xf0, 0x00, 0x20, 0x29, 0x02, 0x0e, 0x03, 0x03, pad,
                          (unsigned char)(((v >> 4) & 3) * 42), (unsigned char)(((v >> 2) & 3) * 42),
                          (unsigned char)((v & 3) * 42), 0xf7 };
    ++generation[pad];
    lp_send(m, sizeof m);
}

static void button(unsigned char cc, unsigned char color) { unsigned char m[3] = { 0xb0, cc, color }; lp_send(m, 3); }

static void redraw(void) {
    for (int r = 1; r <= 8; ++r) for (int c = 1; c <= 8; ++c) {
        int pad = 10 * r + c;
        unsigned char col = 0;
        if (mode == MODE_NOTE) {
            int n = note_of_pad(pad);
            col = (n >= 0 && n < 128 && held[n]) ? 21 : layout_color(n);
        }
        pad_color(pad, col);
    }
    button(95, mode == MODE_DRUM ? 21 : 1);   /* Session */
    button(96, mode == MODE_NOTE ? 21 : 1);   /* Note */
    button(91, mode == MODE_NOTE ? 13 : 0);   /* octave up */
    button(92, mode == MODE_NOTE ? 13 : 0);   /* octave down */
    fprintf(stderr, "Mode: %s%s\n", mode == MODE_NOTE ? "NOTE" : "DRUM", mode == MODE_NOTE && octave ? " (octave shifted)" : "");
}

static int led_pad(unsigned note) { /* drum mode: default MPC pads -> 4x4 block bottom-left */
    for (int i = 0; i < 16; ++i)
        if (mpc_pad_notes[i] == note) return 10 * (i / 4 + 1) + i % 4 + 1;
    unsigned i = note % 64;
    return (int)(10 * (i / 8 + 1) + i % 8 + 1);
}

/* In note mode, light every pad that plays this note (isomorphic layout repeats notes). */
static void note_mode_light(int note, int on) {
    held[note] = on ? 1 : 0;
    held_since[note] = time(NULL);
    for (int r = 1; r <= 8; ++r) for (int c = 1; c <= 8; ++c) {
        int pad = 10 * r + c;
        if (note_of_pad(pad) == note) pad_color(pad, on ? 21 : layout_color(note));
    }
}

static void release_all(void) {
    for (int n = 0; n < 128; ++n) {
        if (native) inject(0, (unsigned)n, "release-all");
        else if (held[n]) note_mode_light(n, 0);
    }
}

static int mpc_running(void) {
    int bytes = proc_listpids(PROC_ALL_PIDS, 0, NULL, 0);
    if (bytes <= 0) return 1;
    pid_t *pids = malloc((size_t)bytes);
    if (!pids) return 1;
    bytes = proc_listpids(PROC_ALL_PIDS, 0, pids, bytes);
    int found = 0;
    for (int i = 0; i < bytes / (int)sizeof(pid_t) && !found; ++i) {
        char nm[64];
        if (pids[i] > 0 && proc_name(pids[i], nm, sizeof nm) > 0 && (!strcmp(nm, "MPC") || !strcmp(nm, "MPC Beats"))) found = 1;
    }
    free(pids);
    return found;
}

static void set_native(int want);

/* Safety net: MPC does not always send note-offs (e.g. when stopping a sequence). */
#define MAX_HELD_S 12
static void *watchdog(void *arg) {
    (void)arg;
    int was_running = 1;
    while (running) {
        sleep(1);
        if (watch_mpc) {
            int now_running = mpc_running();
            if (!now_running && was_running) {                 /* MPC quit: give the Launchpad back */
                fprintf(stderr, "MPC quit: releasing the Launchpad\n");
                if (native) release_all(); else set_native(1);
            } else if (now_running && !was_running && fixed_drum) {
                set_native(0);                                 /* MPC is back: restore the fixed drum layout */
            }
            was_running = now_running;
        }
        time_t now = time(NULL);
        for (int n = 0; n < 128; ++n)
            if (held[n] && now - held_since[n] > MAX_HELD_S) {
                if (native) inject(0, (unsigned)n, "watchdog"); else note_mode_light(n, 0);
            }
    }
    return NULL;
}

#define MIN_LIT_MS 180
static void pad_off_later(unsigned char pad) {
    unsigned g = generation[pad];
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, MIN_LIT_MS * NSEC_PER_MSEC),
                   dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
        if (generation[pad] == g && mode == MODE_DRUM) { unsigned char m[3] = { 0x90, pad, 0 }; lp_send(m, 3); }
    });
}

static unsigned char vel_color(unsigned vel) { return vel < 43 ? 21 : vel < 86 ? 13 : 5; }

/* Notes arriving from MPC (virtual port) or from the Silver pads. */
static void handle_notes(const MIDIPacketList *list, void *ctx, void *src) {
    (void)src;
    const MIDIPacket *p = &list->packet[0];
    for (UInt32 k = 0; k < list->numPackets; ++k) {
        if (trace) { fprintf(stderr, "raw in:"); for (UInt16 j = 0; j < p->length && j < 16; ++j) fprintf(stderr, " %02x", p->data[j]); fputc(10, stderr); }
        for (UInt16 i = 0; i + 2 < p->length + 0u; ) {
            unsigned char st = p->data[i];
            if ((st & 0xf0) == 0x90 || (st & 0xf0) == 0x80) {
                unsigned n = p->data[i + 1], vel = p->data[i + 2];
                int on = (st & 0xf0) == 0x90 && vel > 0;
                if (native) { inject(on, n, ctx ? "mpc-track-out" : "silver-hit"); if (ctx) to_mpc(st, (unsigned char)n, (unsigned char)vel); }
                else if (mode == MODE_NOTE) note_mode_light((int)n, on);
                else {
                    unsigned char pad = (unsigned char)led_pad(n);
                    if (on) pad_color(pad, vel_color(vel)); else pad_off_later(pad);
                }
                i += 3;
            } else if ((st & 0xf0) == 0xb0 && (p->data[i + 1] == 123 || p->data[i + 1] == 120)) {
                release_all(); i += 3;
            } else if (st >= 0x80) {
                i += ((st & 0xf0) == 0xc0 || (st & 0xf0) == 0xd0) ? 2 : 3;
            } else ++i;
        }
        p = MIDIPacketNext(p);
    }
}

/* Presses on the Launchpad itself. */
static void handle_launchpad(const MIDIPacketList *list, void *ctx, void *src) {
    (void)ctx; (void)src;
    if (native) return;   /* native Note mode: the Launchpad talks to MPC directly */
    const MIDIPacket *p = &list->packet[0];
    for (UInt32 k = 0; k < list->numPackets; ++k) {
        for (UInt16 i = 0; i + 2 < p->length + 0u; ) {
            unsigned char st = p->data[i];
            unsigned a = p->data[i + 1], b = p->data[i + 2];
            if ((st & 0xf0) == 0xb0) {
                if (b > 0) {
                    if (a == 96) { mode = MODE_NOTE; redraw(); }
                    else if (a == 95) { mode = MODE_DRUM; redraw(); }
                    else if (a == 91 && mode == MODE_NOTE) { if (octave < 4) ++octave; redraw(); }
                    else if (a == 92 && mode == MODE_NOTE) { if (octave > -2) --octave; redraw(); }
                }
                i += 3;
            } else if ((st & 0xf0) == 0x90 || (st & 0xf0) == 0x80) {
                int on = (st & 0xf0) == 0x90 && b > 0;
                if (valid_pad((int)a)) {
                    if (mode == MODE_NOTE) {
                        int n = note_of_pad((int)a);
                        if (n >= 0 && n < 128) {
                            to_mpc(on ? 0x90 : 0x80, (unsigned char)n, on ? (unsigned char)b : 0);
                            note_mode_light(n, on);
                        }
                    } else {
                        int r = (int)a / 10, c = (int)a % 10;
                        if (r <= 4 && c <= 4)
                            to_mpc(on ? 0x99 : 0x89, (unsigned char)(36 + (r - 1) * 4 + (c - 1)), on ? (unsigned char)b : 0); /* MPC software drum programs: pad A01 = note 36 ... A16 = 51 */
                    }
                }
                i += 3;
            } else if (st >= 0x80) {
                i += ((st & 0xf0) == 0xc0 || (st & 0xf0) == 0xd0) ? 2 : 3;
            } else ++i;
        }
        p = MIDIPacketNext(p);
    }
}

/* MPC pad lights, tapped from the driver: 0xB9 <pad note> <6-bit colour>. Drum mode only. */
static void set_native(int want) {
    if (want == native) return;
    static const unsigned char programmer[] = { 0xf0, 0x00, 0x20, 0x29, 0x02, 0x0e, 0x0e, 0x01, 0xf7 };
    static const unsigned char live[] = { 0xf0, 0x00, 0x20, 0x29, 0x02, 0x0e, 0x0e, 0x00, 0xf7 };
    if (want) {
        for (int n = 0; n < 128; ++n) held[n] = 0;
        for (unsigned r = 1; r <= 8; ++r) for (unsigned c = 1; c <= 8; ++c) pad_color((int)(10 * r + c), 0);
        lp_send(live, sizeof live);
        native = 1;
        release_all();
    } else {
        for (int n = 0; n < 128; ++n) held[n] = 0;
        native = 0;
        lp_send(programmer, sizeof programmer);
        mode = MODE_DRUM;
        redraw();
    }
    fprintf(stderr, "Auto: %s\n", want ? "plug-in/keyboard layout -> native Note mode" : "drum layout -> drum mirror");
}

/* MPC repaints all 16 pads at once when the track type changes; drum frames use dim colours (< 0x30),
   keyboard/plug-in frames use bright ones (0x38, 0x3f). */
static unsigned char wval[128], wseen[128];
static int wcount, wdone;
static double wlast;

static void classify(const unsigned char *buf, ssize_t n) {
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    double now = (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
    if (now - wlast > 40) { memset(wseen, 0, sizeof wseen); wcount = 0; wdone = 0; }
    wlast = now;
    for (ssize_t i = 0; i + 2 < n; i += 3) {
        if (buf[i] != 0xb9 || buf[i + 1] > 127) continue;
        if (!wseen[buf[i + 1]]) { wseen[buf[i + 1]] = 1; ++wcount; }
        wval[buf[i + 1]] = buf[i + 2];
    }
    if (wcount >= 14 && !wdone) {
        int hot = 0;
        for (int k = 0; k < 128; ++k) if (wseen[k] && wval[k] >= 0x30) ++hot;
        wdone = 1;
        set_native(hot >= 4);
    }
}

static void *led_listener(void *arg) {
    int sock = *(int *)arg;
    unsigned char buf[256];
    for (;;) {
        ssize_t n = recv(sock, buf, sizeof buf, 0);
        if (n < 0) break;
        if (trace) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); fprintf(stderr, "[%ld.%03ld] led:", (long)ts.tv_sec, ts.tv_nsec / 1000000); for (ssize_t j = 0; j < n; ++j) fprintf(stderr, " %02x", buf[j]); fputc(10, stderr); }
        if (auto_mode) classify(buf, n);
        if (native) {
            for (ssize_t i = 0; i + 2 < n; i += 3) if (buf[i] == 0xb9 && !auto_mode) inject(buf[i + 2] != 0, buf[i + 1], "mpc-led-cc");
            continue;
        }
        if (mode != MODE_DRUM) continue;
        for (ssize_t i = 0; i + 2 < n; i += 3)
            if (buf[i] == 0xb9) pad_rgb((unsigned char)led_pad(buf[i + 1]), buf[i + 2]);
    }
    return NULL;
}

static void stop(int s) { (void)s; running = 0; }

int main(int argc, char **argv) {
    int use_silver = 1, use_leds = 1;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--no-silver")) use_silver = 0;
        else if (!strcmp(argv[i], "--no-mpc")) use_mpc_port = 0;
        else if (!strcmp(argv[i], "--no-leds")) use_leds = 0;
        else if (!strcmp(argv[i], "--no-input")) use_input = 0;
        else if (!strcmp(argv[i], "--note-mode")) { mode = MODE_NOTE; auto_mode = 0; fixed_drum = 0; }
        else if (!strcmp(argv[i], "--no-auto")) { auto_mode = 0; fixed_drum = 1; }
        else if (!strcmp(argv[i], "--no-watch")) watch_mpc = 0;
        else if (!strcmp(argv[i], "--trace")) trace = 1;
        else if (!strcmp(argv[i], "--native-note")) { native = 1; use_input = 1; use_leds = 0; auto_mode = 0; fixed_drum = 0; }
        else if (!strcmp(argv[i], "--with-leds")) use_leds = 1;
        else {
            fprintf(stderr, "usage: %s [--no-silver] [--no-mpc] [--no-leds] [--no-input] [--note-mode] [--native-note [--with-leds]]\n"
                    "  --no-silver  ignore MPC Studio pad hits\n"
                    "  --no-mpc     no \"SilverBridge Launchpad\" port (notes from MPC tracks)\n"
                    "  --no-leds    ignore MPC pad-light feedback\n"
                    "  --no-watch   do not release the Launchpad when MPC quits\n"
                    "  --no-auto    do not switch drum/Note mode automatically from MPC pad lights\n"
                    "  --no-input   Launchpad presses are not sent to MPC\n"
                    "  --note-mode  start in note mode (default: drum mode)\n"
                    "  --native-note  Launchpad stays in its own Note mode; MPC notes light it (channel-16 injection)\n", argv[0]);
            return 2;
        }
    }
    char nm[200];
    lp_dest = 0;
    MIDIEndpointRef lp_source = 0;
    for (ItemCount i = 0; i < MIDIGetNumberOfDestinations(); ++i) {
        name_of(MIDIGetDestination(i), nm, sizeof nm);
        if (strstr(nm, "LPProMK3 MIDI")) lp_dest = MIDIGetDestination(i);
        if (strstr(nm, "LPProMK3 DAW")) lp_daw = MIDIGetDestination(i);
    }
    for (ItemCount i = 0; i < MIDIGetNumberOfSources(); ++i) {
        name_of(MIDIGetSource(i), nm, sizeof nm);
        if (strstr(nm, "LPProMK3 MIDI")) lp_source = MIDIGetSource(i);
    }
    if (!lp_dest) { fprintf(stderr, "Launchpad Pro MK3 not found\n"); return 1; }
    MIDIClientCreate(CFSTR("SilverBridge Launchpad"), NULL, NULL, &client);
    MIDIOutputPortCreate(client, CFSTR("out"), &out_port);
    MIDIInputPortCreate(client, CFSTR("silver"), handle_notes, NULL, &in_port);
    MIDIInputPortCreate(client, CFSTR("launchpad"), handle_launchpad, NULL, &lp_in_port);
    if (use_input) MIDISourceCreate(client, CFSTR("SilverBridge Launchpad In"), &virtual_src);
    if (use_mpc_port) MIDIDestinationCreate(client, CFSTR("SilverBridge Launchpad"), handle_notes, (void *)1, &virtual_dest);
    if (lp_source) MIDIPortConnectSource(lp_in_port, lp_source, NULL);
    int linked = 0;
    for (ItemCount i = 0; use_silver && i < MIDIGetNumberOfSources(); ++i) {
        name_of(MIDIGetSource(i), nm, sizeof nm);
        if (strstr(nm, "MPC Public")) { MIDIPortConnectSource(in_port, MIDIGetSource(i), NULL); ++linked; }
    }
    if (auto_mode && use_silver && linked == 0) {   /* no SilverBridge MPC driver: leave the Launchpad alone */
        native = 1; auto_mode = 0; use_leds = 0;
        fprintf(stderr, "MPC Studio port not found: running in native Note mode (no pad mirror)\n");
    }
    if (use_leds) {
        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        struct sockaddr_in a; memset(&a, 0, sizeof a);
        a.sin_family = AF_INET; a.sin_port = htons(47531); inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
        if (sock >= 0 && bind(sock, (struct sockaddr *)&a, sizeof a) == 0) {
            static int s; s = sock; pthread_t t; pthread_create(&t, NULL, led_listener, &s); pthread_detach(t);
        } else fprintf(stderr, "LED tap port busy (another bridge running?)\n");
    }
    static const unsigned char programmer[] = { 0xf0, 0x00, 0x20, 0x29, 0x02, 0x0e, 0x0e, 0x01, 0xf7 };
    if (!native) lp_send(programmer, sizeof programmer);
    fprintf(stderr, "Bridge running: Silver pads %s (%d linked), MPC output port %s, MPC LED feedback %s, Launchpad -> MPC %s\n",
            use_silver ? "ON" : "OFF", linked, use_mpc_port ? "ON" : "OFF", use_leds ? "ON" : "OFF", use_input ? "ON" : "OFF");
    if (!native) redraw();
    if (native) release_all();
    { pthread_t w; pthread_create(&w, NULL, watchdog, NULL); pthread_detach(w); }
    signal(SIGINT, stop); signal(SIGTERM, stop);
    while (running) CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1, false);
    mode = MODE_DRUM;
    if (native) { release_all(); MIDIClientDispose(client); return 0; }
    for (unsigned r = 1; r <= 8; ++r) for (unsigned c = 1; c <= 8; ++c) pad_color((int)(10 * r + c), 0);
    static const unsigned char live[] = { 0xf0, 0x00, 0x20, 0x29, 0x02, 0x0e, 0x0e, 0x00, 0xf7 };
    lp_send(live, sizeof live);
    MIDIClientDispose(client);
    return 0;
}
