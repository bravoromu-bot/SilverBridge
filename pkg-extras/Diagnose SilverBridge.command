#!/bin/sh
# Collects a diagnostic report on the Desktop. Quit MPC first; plug in the MPC Studio.
out="$HOME/Desktop/SilverBridge-diagnostic.txt"
flag=/private/tmp/silverbridge-trace
log=/private/tmp/mpc-silver-midi-out.log
if pgrep -x MPC >/dev/null; then echo "Please quit MPC first, then run this again."; read -r _; exit 1; fi
rm -f "$log"; touch "$flag"
killall MIDIServer 2>/dev/null; sleep 5
echo "Now launch MPC with the MPC Studio connected."
echo "Wait about 30 seconds, hit a few pads, then press Return here."
read -r _
rm -f "$flag"
{
  echo "== SilverBridge diagnostic $(date)"
  sw_vers
  echo; echo "== Architecture: $(uname -m)"
  echo; echo "== MPC version"
  defaults read /Applications/MPC.app/Contents/Info CFBundleShortVersionString 2>&1
  echo; echo "== Installed MIDI drivers"
  ls "/Library/Audio/MIDI Drivers" "$HOME/Library/Audio/MIDI Drivers" 2>&1
  defaults read "/Library/Audio/MIDI Drivers/SilverMIDIDriver.plugin/Contents/Info" CFBundleVersion 2>&1
  echo; echo "== USB (Akai)"
  system_profiler SPUSBDataType 2>/dev/null | grep -i -B3 -A12 akai
  echo; echo "== Recent MIDIServer crash reports"
  ls -t "$HOME/Library/Logs/DiagnosticReports" 2>/dev/null | grep -i midiserver | head -5
  echo; echo "== Driver trace (MIDI traffic only, first 400 lines + last 100)"
  head -400 "$log" 2>&1; echo "[...]"; tail -100 "$log" 2>&1
} > "$out" 2>&1
echo "Report saved to: $out  (send this file back)."
read -r _
