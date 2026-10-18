#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFPlugIn.h>
#include <CoreMIDI/MIDIDriver.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char **argv) {
    char path[4096];
    if (!getcwd(path, sizeof(path)) ||
        strlen(path) + strlen("/SilverMIDIDriver.plugin") + 1 > sizeof(path)) return 4;
    if (argc > 1) {
        if (strlen(argv[1]) + 1 > sizeof(path)) return 4;
        strcpy(path, argv[1]);
    } else strcat(path, "/SilverMIDIDriver.plugin");
    fprintf(stderr, "path=%s\n", path);
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(NULL,
        (const UInt8 *)path, (CFIndex)strlen(path), true);
    char url_text[4096] = "";
    CFStringGetCString(CFURLGetString(url), url_text, sizeof(url_text), kCFStringEncodingUTF8);
    fprintf(stderr, "url=%s\n", url_text);
    CFBundleRef bundle = CFBundleCreate(NULL, url);
    fprintf(stderr, "CFBundleCreate=%s\n", bundle ? "ok" : "null");
    if (bundle) {
        fprintf(stderr, "CFBundleLoadExecutable=%s factory=%p\n",
                CFBundleLoadExecutable(bundle) ? "ok" : "failed",
                CFBundleGetFunctionPointerForName(bundle, CFSTR("SilverDriverFactory")));
    }
    if (!bundle) { CFRelease(url); return 1; }
    CFDictionaryRef info = CFBundleGetInfoDictionary(bundle);
    CFDictionaryRef factories = CFDictionaryGetValue(info, CFSTR("CFPlugInFactories"));
    if (!factories || CFDictionaryGetCount(factories) != 1) { CFRelease(bundle); CFRelease(url); return 5; }
    const void *key = NULL;
    CFDictionaryGetKeysAndValues(factories, &key, NULL);
    CFUUIDRef factory = CFUUIDCreateFromString(NULL, (CFStringRef)key);
    if (!factory) { CFRelease(bundle); CFRelease(url); return 6; }
    CFPlugInRef plugin = CFPlugInCreate(NULL, url);
    CFRelease(url);
    CFRelease(bundle);
    if (!plugin) { fprintf(stderr, "CFPlugInCreate failed\n"); return 1; }
    MIDIDriverRef driver = CFPlugInInstanceCreate(NULL, factory, kMIDIDriverTypeID);
    CFRelease(factory);
    if (!driver) { fprintf(stderr, "CFPlugInInstanceCreate failed\n"); CFRelease(plugin); return 2; }
    void *queried = NULL;
    HRESULT rc = (*driver)->QueryInterface(driver,
        CFUUIDGetUUIDBytes(kMIDIDriverInterfaceID), &queried);
    printf("factory=ok query=%d interface=%s\n", (int)rc, queried ? "ok" : "null");
    if (queried) (*driver)->Release(queried);
    (*driver)->Release(driver);
    CFRelease(plugin);
    return rc == S_OK && queried ? 0 : 3;
}
