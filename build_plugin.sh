#!/bin/sh
set -eu
cd "$(dirname "$0")"
source_dir=$(pwd)
build_dir=$(mktemp -d /private/tmp/mpc-silver-build.XXXXXX)
trap 'rm -rf "$build_dir"' EXIT
bundle="$build_dir/SilverMIDIDriver.plugin"
mkdir -p "$bundle/Contents/MacOS"
sed 's/64D5B604-D8CC-4BAB-A579-10DAA184EA89/00000000-0000-4000-8000-000000000001/g' \
  Info.plist > "$build_dir/Info.plist"
cp "$build_dir/Info.plist" "$bundle/Contents/Info.plist"
cp silver_midi_plugin.c usb_midi.c usb_midi.h "$build_dir/"
plutil -lint "$bundle/Contents/Info.plist"
cd "$build_dir"
clang -std=c11 -Wall -Wextra -Werror -O2 \
  $(pkg-config --cflags libusb-1.0) -bundle -fvisibility=hidden \
  -Wl,-exported_symbol,_SilverDriverFactory \
  silver_midi_plugin.c usb_midi.c \
  -o "$bundle/Contents/MacOS/SilverMIDIDriver" \
  $(pkg-config --libs libusb-1.0) -framework CoreMIDI -framework CoreFoundation -framework IOKit
factory_uuid=$(uuidgen)
sed "s/64D5B604-D8CC-4BAB-A579-10DAA184EA89/$factory_uuid/g" \
  "$source_dir/Info.plist" > "$bundle/Contents/Info.plist"
codesign --force --sign - "$bundle"
codesign --verify --verbose=2 "$bundle"
rm -rf "$source_dir/SilverMIDIDriver.plugin"
ditto "$bundle" "$source_dir/SilverMIDIDriver.plugin"
printf '%s\n' "$factory_uuid" > "$source_dir/build-factory-uuid.txt"
codesign --verify --verbose=2 "$source_dir/SilverMIDIDriver.plugin"
