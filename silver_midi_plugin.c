#include "usb_midi.h"
#include <CoreFoundation/CFPlugIn.h>
#include <CoreFoundation/CFPlugInCOM.h>
#include <CoreMIDI/MIDIDriver.h>
#include <CoreMIDI/MIDISetup.h>
#include <libusb.h>
#include <IOKit/IOKitLib.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* USB OUT: the stock Akai driver (PID 0x0013 entry, HandleInput/PrepareOutput)
   uses the same 4-byte cable/CIN framing as the IN path; bulk OUT is 0x01. */
static pthread_mutex_t out_lock = PTHREAD_MUTEX_INITIALIZER;

static const CFStringRef entity_names[3] = {
    CFSTR("MPC Private"), CFSTR("MPC Public"), CFSTR("MPC Port 1")
};

typedef struct {
    MIDIDriverInterface *vtable;
    atomic_uint refs;
    MIDIDeviceRef device;
    MIDIEndpointRef sources[3];
    libusb_context *usb_context;
    libusb_device_handle *usb_device;
    pthread_t reader;
    atomic_bool reading;
    int reader_started;
    int claimed;
} SilverDriver;

static SilverDriver *instance(MIDIDriverRef self) { return (SilverDriver *)self; }

static void stop_usb(SilverDriver *driver) {
    atomic_store(&driver->reading, false);
    if (driver->reader_started) {
        pthread_join(driver->reader, NULL);
        driver->reader_started = 0;
    }
    if (driver->claimed) libusb_release_interface(driver->usb_device, 1);
    if (driver->usb_device) libusb_close(driver->usb_device);
    if (driver->usb_context) libusb_exit(driver->usb_context);
    driver->claimed = 0;
    driver->usb_device = NULL;
    driver->usb_context = NULL;
}

static int trace_enabled(void) {
    static int enabled = -1;
    if (enabled < 0) enabled = getenv("SILVERBRIDGE_TRACE") != NULL ||
                  access("/private/tmp/silverbridge-trace", F_OK) == 0;
    return enabled;
}

static void trace(const char *tag, const uint8_t *d, size_t n, int rc) {
    if (!trace_enabled()) return;
    int fd = open("/private/tmp/mpc-silver-midi-out.log", O_WRONLY | O_CREAT | O_APPEND, 0600);
    if (fd < 0) return;
    char line[512];
    int k = snprintf(line, sizeof(line), "%s rc=%d len=%zu", tag, rc, n);
    for (size_t j = 0; j < n && k < (int)sizeof(line) - 5; ++j)
        k += snprintf(line + k, sizeof(line) - (size_t)k, " %02x", d[j]);
    line[k++] = '\n';
    (void)write(fd, line, (size_t)k);
    close(fd);
}

static void forward_midi(void *context, uint8_t cable, const uint8_t *midi, size_t length) {
    SilverDriver *driver = context;
    if (cable >= 3 || !driver->sources[cable]) return;
    Byte storage[256];
    MIDIPacketList *list = (MIDIPacketList *)storage;
    MIDIPacket *packet = MIDIPacketListInit(list);
    if (MIDIPacketListAdd(list, sizeof(storage), packet, 0, (UInt16)length, midi))
        MIDIReceived(driver->sources[cable], list);
}

static void set_offline(SilverDriver *driver, int offline) {
    if (driver->device) MIDIObjectSetIntegerProperty(driver->device, kMIDIPropertyOffline, offline);
}

/* (Re)open and claim the Silver. The USB handle goes stale when the Mac sleeps or the cable is replugged. */
static int connect_usb(SilverDriver *driver) {
    int ok;
    pthread_mutex_lock(&out_lock);
    ok = driver->usb_device != NULL;
    if (!ok && driver->usb_context) {
        libusb_device_handle *h = libusb_open_device_with_vid_pid(driver->usb_context, 0x09e8, 0x0013);
        if (h && libusb_claim_interface(h, 1) == 0) { driver->usb_device = h; driver->claimed = 1; ok = 1; }
        else if (h) libusb_close(h);
    }
    pthread_mutex_unlock(&out_lock);
    return ok;
}

static void drop_usb(SilverDriver *driver) {
    pthread_mutex_lock(&out_lock);
    if (driver->usb_device) {
        if (driver->claimed) libusb_release_interface(driver->usb_device, 1);
        libusb_close(driver->usb_device);
    }
    driver->usb_device = NULL;
    driver->claimed = 0;
    pthread_mutex_unlock(&out_lock);
}

static void *read_usb(void *context) {
    SilverDriver *driver = context;
    while (atomic_load(&driver->reading)) {
        if (!driver->usb_device) {
            if (!connect_usb(driver)) {
                for (int i = 0; i < 4 && atomic_load(&driver->reading); ++i) usleep(250000);
                continue;
            }
            set_offline(driver, 0);
        }
        unsigned char bytes[64];
        int received = 0;
        int rc = libusb_bulk_transfer(driver->usb_device, 0x82, bytes, sizeof(bytes),
                                      &received, 250);
        if (rc == LIBUSB_ERROR_TIMEOUT) continue;
        if (rc) {                       /* device gone (sleep, unplug): drop the handle and retry */
            drop_usb(driver);
            set_offline(driver, 1);
            continue;
        }
        if (received > 0) trace("usb-in", bytes, (size_t)received, 0);
        if (received > 0) usb_midi_decode(bytes, (size_t)received, forward_midi, driver);
    }
    return NULL;
}

static int usb_present(void) {
    libusb_context *context = NULL;
    if (libusb_init(&context)) return 0;
    libusb_device_handle *handle = libusb_open_device_with_vid_pid(context, 0x09e8, 0x0013);
    int found = handle != NULL;
    if (handle) libusb_close(handle);
    libusb_exit(context);
    return found;
}

static int usb_location_id(SInt32 *result) {
    CFMutableDictionaryRef match = IOServiceMatching("IOUSBHostDevice");
    io_iterator_t iterator = IO_OBJECT_NULL;
    if (!match || IOServiceGetMatchingServices(kIOMainPortDefault, match, &iterator) != KERN_SUCCESS)
        return 0;
    int found = 0;
    io_service_t service;
    while ((service = IOIteratorNext(iterator))) {
        CFTypeRef vendor = IORegistryEntryCreateCFProperty(service, CFSTR("idVendor"), NULL, 0);
        CFTypeRef product = IORegistryEntryCreateCFProperty(service, CFSTR("idProduct"), NULL, 0);
        int vid = 0, pid = 0;
        if (vendor && CFGetTypeID(vendor) == CFNumberGetTypeID()) CFNumberGetValue(vendor, kCFNumberIntType, &vid);
        if (product && CFGetTypeID(product) == CFNumberGetTypeID()) CFNumberGetValue(product, kCFNumberIntType, &pid);
        if (vid == 0x09e8 && pid == 0x0013) {
            CFTypeRef location = IORegistryEntryCreateCFProperty(service, CFSTR("locationID"), NULL, 0);
            if (location && CFGetTypeID(location) == CFNumberGetTypeID())
                found = CFNumberGetValue(location, kCFNumberSInt32Type, result);
            if (location) CFRelease(location);
        }
        if (vendor) CFRelease(vendor);
        if (product) CFRelease(product);
        IOObjectRelease(service);
        if (found) break;
    }
    IOObjectRelease(iterator);
    return found;
}

static OSStatus make_device(SilverDriver *driver, MIDIDeviceListRef list) {
    MIDIDeviceRef device = 0;
    OSStatus rc = MIDIDeviceCreate((MIDIDriverRef)&driver->vtable,
                                  CFSTR("MPC Studio"), CFSTR("Akai"),
                                  CFSTR("MPC Studio"), &device);
    if (rc) return rc;
    for (int i = 0; i < 3; ++i) {
        MIDIEntityRef entity = 0;
        rc = MIDIDeviceAddEntity(device, entity_names[i], true, 1, 1, &entity);
        if (rc) { MIDIDeviceDispose(device); return rc; }
        if (i == 0)
            MIDIObjectSetIntegerProperty(entity, kMIDIPropertyPrivate, 1);
    }
    rc = MIDIDeviceListAddDevice(list, device);
    if (rc) MIDIDeviceDispose(device);
    return rc;
}

static OSStatus driver_find_devices(MIDIDriverRef self, MIDIDeviceListRef list) {
    if (!usb_present()) return noErr;
    return make_device(instance(self), list);
}

static OSStatus driver_start(MIDIDriverRef self, MIDIDeviceListRef list) {
    SilverDriver *driver = instance(self);
    if (!MIDIDeviceListGetNumberOfDevices(list)) return noErr;
    driver->device = MIDIDeviceListGetDevice(list, 0);
    /* Set properties only after MIDIServer has added the device to its setup.
       Publishing them inside FindDevices crashes SetupManager::PropertyDirty. */
    MIDIObjectSetStringProperty(driver->device, CFSTR("PloytecVersion"), CFSTR("v3.3.11"));
    MIDIObjectSetIntegerProperty(driver->device, CFSTR("USBVendorProduct"), (SInt32)0x09e80013);
    SInt32 location = 0;
    if (usb_location_id(&location))
        MIDIObjectSetIntegerProperty(driver->device, CFSTR("USBLocationID"), location);
    /* Stock driver sets SerialNumber only when the USB string exists; the Silver has none. */
    MIDIObjectSetIntegerProperty(driver->device, kMIDIPropertyOffline, 0);
    for (int i = 0; i < 3; ++i) {
        MIDIEntityRef entity = MIDIDeviceGetEntity(driver->device, (ItemCount)i);
        driver->sources[i] = MIDIEntityGetSource(entity, 0);
        MIDIEndpointRef destination = MIDIEntityGetDestination(entity, 0);
        if (destination)
            MIDIEndpointSetRefCons(destination, (void *)(uintptr_t)i, NULL);
    }
    if (libusb_init(&driver->usb_context)) return noErr;
    connect_usb(driver);
    atomic_store(&driver->reading, true);
    if (!pthread_create(&driver->reader, NULL, read_usb, driver)) driver->reader_started = 1;
    else stop_usb(driver);
    return noErr;
}

static OSStatus driver_stop(MIDIDriverRef self) {
    SilverDriver *driver = instance(self);
    stop_usb(driver);
    driver->device = 0;
    for (int i = 0; i < 3; ++i) driver->sources[i] = 0;
    return noErr;
}

static OSStatus driver_configure(MIDIDriverRef self, MIDIDeviceRef device) {
    (void)self; (void)device; return noErr;
}

/* LED tap: MPC lights the Silver pads with channel-10 CC messages (0xB9, controller = pad note,
   value = colour). A copy of each is sent over loopback UDP so companion tools (launchpad-bridge)
   can mirror them. Nothing leaves the machine. */
#define LED_TAP_PORT 47531
static void led_tap(const uint8_t *data, size_t length) {
    static int sock = -2;
    static struct sockaddr_in dest;
    if (sock == -2) {
        sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock >= 0) fcntl(sock, F_SETFL, O_NONBLOCK);
        memset(&dest, 0, sizeof dest);
        dest.sin_family = AF_INET;
        dest.sin_port = htons(LED_TAP_PORT);
        inet_pton(AF_INET, "127.0.0.1", &dest.sin_addr);
    }
    if (sock < 0) return;
    uint8_t out[3 * 32];
    size_t n = 0;
    for (size_t i = 0; i + 2 < length && n + 3 <= sizeof out; ) {
        if (data[i] == 0xb9) { memcpy(out + n, data + i, 3); n += 3; i += 3; }
        else if (data[i] == 0xf0) { while (i < length && data[i] != 0xf7) ++i; ++i; }
        else ++i;
    }
    if (n) sendto(sock, out, n, 0, (struct sockaddr *)&dest, sizeof dest);
}

static OSStatus driver_send(MIDIDriverRef self, const MIDIPacketList *list,
                            void *ref1, void *ref2) {
    (void)self; (void)ref2;
    int fd = trace_enabled() ? open("/private/tmp/mpc-silver-midi-out.log", O_WRONLY | O_CREAT | O_APPEND, 0600) : -1;
    if (fd >= 0 && list) {
        char line[1024];
        const MIDIPacket *packet = &list->packet[0];
        for (UInt32 i = 0; i < list->numPackets; ++i) {
            int n = snprintf(line, sizeof(line), "port=%ld len=%u", (long)(uintptr_t)ref1, packet->length);
            for (UInt16 j = 0; j < packet->length && n < (int)sizeof(line) - 4; ++j)
                n += snprintf(line + n, sizeof(line) - (size_t)n, " %02x", packet->data[j]);
            if (n < (int)sizeof(line) - 1) line[n++] = '\n';
            (void)write(fd, line, (size_t)n);
            packet = MIDIPacketNext(packet);
        }
        close(fd);
    }
    SilverDriver *driver = instance(self);
    OSStatus status = noErr;
    if (!list) return -4;
    uintptr_t cable = (uintptr_t)ref1;
    if (cable >= 3) return -4;
    pthread_mutex_lock(&out_lock);
    if (!driver->usb_device || !driver->claimed) { pthread_mutex_unlock(&out_lock); return -4; }
    const MIDIPacket *packet = &list->packet[0];
    for (UInt32 i = 0; i < list->numPackets; ++i) {
        led_tap(packet->data, packet->length);
        uint8_t buffer[1024];
        int n = usb_midi_encode((uint8_t)cable, packet->data, packet->length, buffer, sizeof(buffer));
        /* Stock PrepareOutput zero-pads every transfer to the full 64-byte packet. */
        for (int off = 0; n > 0 && off < n; off += 64) {
            uint8_t frame[64] = {0};
            int chunk = n - off > 64 ? 64 : n - off, sent = 0;
            memcpy(frame, buffer + off, (size_t)chunk);
            int urc = libusb_bulk_transfer(driver->usb_device, 0x01, frame, 64, &sent, 200);
            trace("usb-out", frame, 64, urc);
            if (urc) {
                status = -4;
                break;
            }
        }
        packet = MIDIPacketNext(packet);
    }
    pthread_mutex_unlock(&out_lock);
    return status;
}

static OSStatus driver_enable_source(MIDIDriverRef self, MIDIEndpointRef source,
                                     Boolean enabled) {
    (void)self; (void)source; (void)enabled; return noErr;
}

static OSStatus driver_flush(MIDIDriverRef self, MIDIEndpointRef destination,
                             void *ref1, void *ref2) {
    (void)self; (void)destination; (void)ref1; (void)ref2; return noErr;
}

static OSStatus driver_monitor(MIDIDriverRef self, MIDIEndpointRef destination,
                               const MIDIPacketList *list) {
    (void)self; (void)destination; (void)list; return noErr;
}

static OSStatus driver_send_packets(MIDIDriverRef self, const MIDIEventList *list,
                                    void *ref1, void *ref2) {
    (void)self; (void)list; (void)ref1; (void)ref2; return -4;
}

static OSStatus driver_monitor_events(MIDIDriverRef self, MIDIEndpointRef destination,
                                      const MIDIEventList *list) {
    (void)self; (void)destination; (void)list; return noErr;
}

static ULONG driver_add_ref(void *self) {
    return atomic_fetch_add(&((SilverDriver *)self)->refs, 1) + 1;
}

static ULONG driver_release(void *self) {
    SilverDriver *driver = self;
    ULONG count = atomic_fetch_sub(&driver->refs, 1) - 1;
    if (!count) {
        stop_usb(driver);
        free(driver);
    }
    return count;
}

static HRESULT driver_query_interface(void *self, REFIID iid, LPVOID *out) {
    if (!out) return E_POINTER;
    *out = NULL;
    CFUUIDRef uuid = CFUUIDCreateFromUUIDBytes(NULL, iid);
    int supported = CFEqual(uuid, IUnknownUUID) ||
                    CFEqual(uuid, kMIDIDriverInterfaceID);
    CFRelease(uuid);
    if (!supported) return E_NOINTERFACE;
    *out = self;
    driver_add_ref(self);
    return S_OK;
}

static MIDIDriverInterface driver_vtable = {
    NULL,
    driver_query_interface, driver_add_ref, driver_release,
    driver_find_devices, driver_start, driver_stop, driver_configure,
    driver_send, driver_enable_source, driver_flush, driver_monitor,
    driver_send_packets, driver_monitor_events
};

__attribute__((visibility("default")))
void *SilverDriverFactory(CFAllocatorRef allocator, CFUUIDRef type) {
    (void)allocator;
    if (!CFEqual(type, kMIDIDriverTypeID)) return NULL;
    SilverDriver *driver = calloc(1, sizeof(*driver));
    if (!driver) return NULL;
    driver->vtable = &driver_vtable;
    atomic_init(&driver->refs, 1);
    atomic_init(&driver->reading, false);
    return &driver->vtable;
}
