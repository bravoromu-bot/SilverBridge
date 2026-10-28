#!/bin/sh
# SilverBridge Launchpad MK3: on/off and mode.
L=org.silverbridge.launchpadmk3; P="/Library/LaunchAgents/$L.plist"; U=$(id -u)
D="$HOME/Library/Application Support/SilverBridgeLaunchpadMK3"; mkdir -p "$D"
is_on() { launchctl print "gui/$U/$L" >/dev/null 2>&1; }
echo "Launchpad bridge: $(is_on && echo ON || echo OFF)   mode: $(cat "$D/mode" 2>/dev/null || echo auto)"
echo "  1) Turn ON/OFF"
echo "  2) Auto  (drum mirror <-> Note mode follows the MPC track type)"
echo "  3) Drum  (always mirror MPC pads; Session/Note buttons switch layout)"
echo "  4) Note  (always the Launchpad's own Note mode)"
printf "Choice: "; read -r c
restart() { is_on && launchctl bootout "gui/$U/$L"; sleep 1; launchctl bootstrap "gui/$U" "$P" && echo "ON, mode: $(cat "$D/mode")"; }
case "$c" in
  1) if is_on; then launchctl bootout "gui/$U/$L" && echo OFF; else launchctl bootstrap "gui/$U" "$P" && echo ON; fi ;;
  2) echo auto > "$D/mode"; restart ;;
  3) echo drum > "$D/mode"; restart ;;
  4) echo note > "$D/mode"; restart ;;
esac
