#!/bin/sh
# Builds SilverBridgeLaunchpadMK3.pkg (universal binary, independent of the MPC driver pkg).
set -eu
cd "$(dirname "$0")"
version=${1:-1.0.0}
work=$(mktemp -d /private/tmp/sblaunchpad-pkg.XXXXXX); trap 'rm -rf "$work"' EXIT
app="$work/root/Library/Application Support/SilverBridgeLaunchpadMK3"
mkdir -p "$app" "$work/root/Library/LaunchAgents" "$work/scripts"
clang -std=c11 -Wall -Wextra -Werror -O2 -fblocks -arch arm64 -arch x86_64 -mmacosx-version-min=12.0 \
  launchpad_bridge.c -o "$app/launchpad-bridge" -framework CoreMIDI -framework CoreFoundation
cp pkg-launchpad/run.sh "$app/"; cp pkg-launchpad/*.command "$app/"
cp pkg-launchpad/org.silverbridge.launchpadmk3.plist "$work/root/Library/LaunchAgents/"
chmod 755 "$app/run.sh" "$app/launchpad-bridge"; chmod 644 "$work/root/Library/LaunchAgents/org.silverbridge.launchpadmk3.plist"
codesign --force --sign - "$app/launchpad-bridge"
cat > "$work/scripts/postinstall" <<'S'
#!/bin/sh
# Start the bridge for the logged-in user; drop the earlier hand-made agent if present.
u=$(stat -f%Su /dev/console); [ -n "$u" ] && [ "$u" != root ] || exit 0
uid=$(id -u "$u"); L=org.silverbridge.launchpadmk3
launchctl asuser "$uid" launchctl bootout "gui/$uid/local.silverbridge.launchpad" 2>/dev/null
rm -f "/Users/$u/Library/LaunchAgents/local.silverbridge.launchpad.plist"
launchctl asuser "$uid" launchctl bootout "gui/$uid/$L" 2>/dev/null
launchctl asuser "$uid" launchctl bootstrap "gui/$uid" "/Library/LaunchAgents/$L.plist" 2>/dev/null
exit 0
S
chmod +x "$work/scripts/postinstall"
dot_clean -m "$work/root"; find "$work/root" -name "._*" -delete
COPYFILE_DISABLE=1 pkgbuild --root "$work/root" --scripts "$work/scripts" --ownership recommended \
  --identifier org.silverbridge.launchpadmk3.pkg --version "$version" --install-location / SilverBridgeLaunchpadMK3.pkg
echo "Built $(pwd)/SilverBridgeLaunchpadMK3.pkg"
