#!/bin/sh
# Removes the Launchpad bridge (admin password required). Does not touch the SilverBridge MPC driver.
L=org.silverbridge.launchpadmk3
launchctl bootout "gui/$(id -u)/$L" 2>/dev/null
sudo rm -f "/Library/LaunchAgents/$L.plist"
sudo rm -rf "/Library/Application Support/SilverBridgeLaunchpadMK3"
rm -rf "$HOME/Library/Application Support/SilverBridgeLaunchpadMK3"
sudo pkgutil --forget org.silverbridge.launchpadmk3.pkg >/dev/null 2>&1
echo "Done: Launchpad bridge removed."
read -r _
