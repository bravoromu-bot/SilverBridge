#!/bin/sh
set -eu
target_dir="$HOME/Library/Audio/MIDI Drivers"
apply=0
while [ "$#" -gt 0 ]; do
  case "$1" in
    --target-dir) target_dir=$2; shift 2 ;;
    --apply) apply=1; shift ;;
    *) echo "Unknown argument: $1" >&2; exit 2 ;;
  esac
done
target_bundle=$target_dir/SilverMIDIDriver.plugin
if [ ! -e "$target_bundle" ]; then echo "Nothing to remove"; exit 0; fi
identifier=$(plutil -extract CFBundleIdentifier raw -o - "$target_bundle/Contents/Info.plist")
if [ "$identifier" != 'org.silverbridge.midi' ]; then
  echo "Refusing to remove a different plug-in: $identifier" >&2
  exit 1
fi
echo "Remove $target_bundle"
if [ "$apply" -eq 0 ]; then echo "Dry run; pass --apply to remove"; exit 0; fi
rm -rf "$target_bundle"
echo "Removed. The historical system driver was untouched."
