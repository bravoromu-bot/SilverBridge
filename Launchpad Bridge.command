#!/bin/sh
# Launchpad Pro MK3 <-> MPC bridge. Close this window or press Ctrl+C to stop.
cd "$(dirname "$0")"
[ -x ./launchpad-bridge ] || clang -O2 -fblocks launchpad_bridge.c -o launchpad-bridge -framework CoreMIDI -framework CoreFoundation || exit 1
echo "Launchpad Bridge"
echo "  1) Drum mode  - mirror MPC pads on the Launchpad (Session / Note buttons switch layouts)"
echo "  2) Note mode  - Launchpad keeps its own Note mode, lit by the notes MPC plays (plug-in tracks)"
printf "Choice [1/2]: "; read -r m
args=""
[ "$m" = "2" ] && args="--native-note"
echo "Running. Close this window or press Ctrl+C to stop."
./launchpad-bridge $args
