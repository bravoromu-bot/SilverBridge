#!/bin/sh
set -eu
cd "$(dirname "$0")"
target_dir="$HOME/Library/Audio/MIDI Drivers"
apply=0
while [ "$#" -gt 0 ]; do
  case "$1" in
    --target-dir) target_dir=$2; shift 2 ;;
    --apply) apply=1; shift ;;
    *) echo "Unknown argument: $1" >&2; exit 2 ;;
  esac
done
source_bundle=$(pwd)/SilverMIDIDriver.plugin
target_bundle=$target_dir/SilverMIDIDriver.plugin
test -d "$source_bundle" || { echo "Build the plug-in first" >&2; exit 1; }
codesign --verify "$source_bundle"
if [ -e "$target_bundle" ]; then
  echo "Refusing to replace existing plug-in: $target_bundle" >&2
  exit 1
fi
echo "Install $source_bundle -> $target_bundle"
if [ "$apply" -eq 0 ]; then echo "Dry run; pass --apply to install"; exit 0; fi
mkdir -p "$target_dir"
ditto "$source_bundle" "$target_bundle"
codesign --verify "$target_bundle"
echo "Installed. Remove with uninstall-user.sh --apply."
