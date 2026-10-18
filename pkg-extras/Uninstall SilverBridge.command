#!/bin/sh
# Removes the SilverBridge MIDI driver. Quit MPC first.
echo "Uninstalling SilverBridge (admin password required)..."
if pgrep -x MPC >/dev/null; then echo "Please quit MPC first, then run this again."; read -r _; exit 1; fi
sudo rm -rf "/Library/Audio/MIDI Drivers/SilverMIDIDriver.plugin" \
            "$HOME/Library/Audio/MIDI Drivers/SilverMIDIDriver.plugin" \
            "/Library/Application Support/SilverBridge"
sudo pkgutil --forget org.silverbridge.midi.pkg >/dev/null 2>&1; sudo pkgutil --forget org.codex.mpc-silver.midi.pkg >/dev/null 2>&1
killall MIDIServer 2>/dev/null
echo "Done. SilverBridge has been removed."
read -r _
