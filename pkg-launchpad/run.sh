#!/bin/sh
# Starts the Launchpad bridge using the per-user mode file (auto | drum | note).
B="/Library/Application Support/SilverBridgeLaunchpadMK3/launchpad-bridge"
M="$HOME/Library/Application Support/SilverBridgeLaunchpadMK3/mode"
case "$(cat "$M" 2>/dev/null)" in
  drum) exec "$B" --no-auto ;;
  note) exec "$B" --native-note ;;
  *)    exec "$B" ;;
esac
