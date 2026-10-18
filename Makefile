CC = clang
CFLAGS = -std=c11 -Wall -Wextra -Werror -O2 $(shell pkg-config --cflags libusb-1.0)
LDLIBS = $(shell pkg-config --libs libusb-1.0) -framework CoreMIDI -framework CoreFoundation

all: silver-bridge

plugin: SilverMIDIDriver.plugin

plugin-smoke: plugin_smoke.c SilverMIDIDriver.plugin
	$(CC) $(CFLAGS) plugin_smoke.c -o $@ -framework CoreFoundation -framework CoreMIDI

test-plugin: plugin-smoke
	./plugin-smoke

test-plugin-rebuild:
	$(MAKE) -B test-plugin
	$(MAKE) -B test-plugin
	./plugin-smoke
	./plugin-smoke

SilverMIDIDriver.plugin: silver_midi_plugin.c usb_midi.c usb_midi.h Info.plist
	sh ./build_plugin.sh

midi-listener: midi_listener.c
	$(CC) $(CFLAGS) midi_listener.c -o $@ -framework CoreMIDI -framework CoreFoundation

silver-bridge: silver_bridge.c usb_midi.c usb_midi.h
	$(CC) $(CFLAGS) silver_bridge.c usb_midi.c -o $@ $(LDLIBS)

test: packet-test
	./packet-test ../usb_input_pad1.log

packet-test: packet_test.c usb_midi.c usb_midi.h
	$(CC) $(CFLAGS) packet_test.c usb_midi.c -o $@

clean:
	rm -f silver-bridge packet-test midi-listener plugin-smoke
	rm -rf SilverMIDIDriver.plugin
