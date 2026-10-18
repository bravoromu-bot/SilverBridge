#!/bin/sh
# Builds SilverBridge.pkg (arm64, libusb linked statically, installs system-wide).
set -eu
cd "$(dirname "$0")"
version=${1:-0.2.0}
work=$(mktemp -d /private/tmp/silverbridge-pkg.XXXXXX)
trap 'rm -rf "$work"' EXIT
bundle="$work/root/Library/Audio/MIDI Drivers/SilverMIDIDriver.plugin"
mkdir -p "$bundle/Contents/MacOS" "$work/scripts" "$work/root/Library/Application Support/SilverBridge"
cp pkg-extras/*.command "$work/root/Library/Application Support/SilverBridge/"
factory_uuid=$(cat build-factory-uuid.txt 2>/dev/null || uuidgen)
sed -e "s/64D5B604-D8CC-4BAB-A579-10DAA184EA89/$factory_uuid/g" \
    -e "s|<string>0.1.0</string>|<string>$version</string>|" Info.plist > "$bundle/Contents/Info.plist"
plutil -lint "$bundle/Contents/Info.plist"
libdir=$(pkg-config --variable=libdir libusb-1.0)
clang -std=c11 -Wall -Wextra -Werror -O2 -arch arm64 -mmacosx-version-min=12.0 \
  $(pkg-config --cflags libusb-1.0) -bundle -fvisibility=hidden \
  -Wl,-exported_symbol,_SilverDriverFactory \
  silver_midi_plugin.c usb_midi.c "$libdir/libusb-1.0.a" \
  -o "$bundle/Contents/MacOS/SilverMIDIDriver" \
  -framework CoreMIDI -framework CoreFoundation -framework IOKit -framework Security
otool -L "$bundle/Contents/MacOS/SilverMIDIDriver" | grep -q homebrew && { echo "libusb still dynamic" >&2; exit 1; }
codesign --force --sign - "$bundle"
dot_clean -m "$work/root"; find "$work/root" -name "._*" -delete
cat > "$work/scripts/postinstall" <<'S'
#!/bin/sh
# Remove the per-user copy from the earlier manual install (it would duplicate the driver).
u=$(stat -f%Su /dev/console)
[ -n "$u" ] && [ "$u" != root ] && rm -rf "/Users/$u/Library/Audio/MIDI Drivers/SilverMIDIDriver.plugin"
killall MIDIServer 2>/dev/null || true
exit 0
S
chmod +x "$work/scripts/postinstall"
COPYFILE_DISABLE=1 pkgbuild --root "$work/root" --scripts "$work/scripts" \
  --identifier org.silverbridge.midi.pkg --version "$version" \
  --install-location / SilverBridge.pkg
pkgutil --check-signature SilverBridge.pkg | head -3 || true
echo "Built $(pwd)/SilverBridge.pkg"
