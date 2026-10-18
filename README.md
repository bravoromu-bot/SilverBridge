# SilverBridge — MPC Studio driver for Apple Silicon Macs

*[Français plus bas](#français)*

An unofficial, open-source CoreMIDI driver that lets the **Akai MPC Studio** (USB `09e8:0013`)
work with the MPC software on Apple Silicon Macs, where the original Intel-only driver can no longer load.

It is a user-space CoreMIDI plug-in (no kernel extension, no DriverKit). It claims the device with
libusb and exposes the three ports the MPC software expects (`MPC Private`, `MPC Public`, `MPC Port 1`),
forwarding USB-MIDI packets in both directions. The pads, LEDs and screen data travel as MIDI/SysEx
and are passed through unchanged.

## Status

Tested on one machine: MPC Studio (`09e8:0013`), macOS 15, Apple Silicon, MPC software build of Aug 2024.
The device is recognised, the Hardware preferences are active, and pads/LEDs/display work in basic use.
Other hardware revisions, MPC versions and macOS versions are untested. Use at your own risk.

## Install

1. **Quit MPC.**
2. Download `SilverBridge.pkg` from the Releases page.
3. Right-click the pkg → **Open** (it is not signed with an Apple Developer ID), or allow it in
   *System Settings → Privacy & Security*.
4. Plug in the MPC Studio, then launch MPC. If MPC was already running, quit it and relaunch.

If macOS blocks the plug-in, run:

```sh
sudo xattr -rd com.apple.quarantine "/Library/Audio/MIDI Drivers/SilverMIDIDriver.plugin"
killall MIDIServer
```

Always quit MPC before restarting MIDIServer, otherwise the screen shows "Computer stopped communicating".

## Uninstall

Run `Uninstall SilverBridge.command` from `/Library/Application Support/SilverBridge/`, or delete
`/Library/Audio/MIDI Drivers/SilverMIDIDriver.plugin` and run `killall MIDIServer`.

## Problems?

Quit MPC, run `Diagnose SilverBridge.command` (same folder), follow the prompts and attach the
`SilverBridge-diagnostic.txt` it puts on your Desktop to an issue. The report contains system and USB
info plus raw MIDI/SysEx traffic between MPC and the device; read it before sharing.

## Build from source

Requires Xcode command line tools and `brew install libusb pkg-config`.

```sh
./make_pkg.sh 0.3.0     # arm64, libusb linked statically, produces SilverBridge.pkg
```

Tracing: create `/private/tmp/silverbridge-trace` (or set `SILVERBRIDGE_TRACE`) and restart MIDIServer;
traffic is written to `/private/tmp/mpc-silver-midi-out.log`.

## Legal

Written from scratch for hardware interoperability. It contains no code from Akai Professional or
Ploytec. The USB framing, port layout and device properties were determined by observing the behaviour of the
vendor's driver and software. Not affiliated with Akai, inMusic or Ploytec; trademarks belong to their owners.
MIT-licensed; see `LICENSE` and `NOTICE.md` (libusb, LGPL-2.1).

---

# Français

Pilote CoreMIDI **non officiel et open source** qui permet à la **MPC Studio** d'Akai (USB `09e8:0013`)
de fonctionner avec le logiciel MPC sur les Mac Apple Silicon, où l'ancien pilote (Intel uniquement) ne se charge plus.

C'est un plug-in CoreMIDI en espace utilisateur (ni kext, ni DriverKit). Il prend l'appareil avec libusb et expose
les trois ports attendus par MPC (`MPC Private`, `MPC Public`, `MPC Port 1`) en relayant les paquets USB-MIDI dans les deux sens.
Pads, LEDs et écran passent en MIDI/SysEx, sans modification.

## État

Testé sur une seule machine : MPC Studio (`09e8:0013`), macOS 15, Apple Silicon, logiciel MPC d'août 2024.
L'appareil est reconnu, les préférences Hardware sont actives, pads/LEDs/écran fonctionnent en usage simple.
Les autres révisions matérielles, versions de MPC et de macOS ne sont pas testées. À vos risques.

## Installation

1. **Quitter MPC.**
2. Télécharger `SilverBridge.pkg` dans l'onglet Releases.
3. Clic droit sur le pkg → **Ouvrir** (il n'est pas signé avec un compte développeur Apple), ou l'autoriser dans
   *Réglages Système → Confidentialité et sécurité*.
4. Brancher la MPC Studio puis lancer MPC. Si MPC était déjà ouvert, le quitter et le relancer.

Si macOS bloque le plug-in :

```sh
sudo xattr -rd com.apple.quarantine "/Library/Audio/MIDI Drivers/SilverMIDIDriver.plugin"
killall MIDIServer
```

Quittez toujours MPC avant de relancer MIDIServer, sinon l'écran affiche « Computer stopped communicating ».

## Désinstallation

Lancer `Uninstall SilverBridge.command` dans `/Library/Application Support/SilverBridge/`, ou supprimer
`/Library/Audio/MIDI Drivers/SilverMIDIDriver.plugin` puis `killall MIDIServer`.

## Un problème ?

Quittez MPC, lancez `Diagnose SilverBridge.command` (même dossier), suivez les instructions, puis joignez le fichier
`SilverBridge-diagnostic.txt` créé sur le Bureau à une issue. Il contient des infos système et USB et le trafic MIDI/SysEx brut
entre MPC et l'appareil : relisez-le avant de le partager.

## Compilation

Xcode Command Line Tools et `brew install libusb pkg-config` requis. Puis `./make_pkg.sh 0.3.0`.

## Mentions légales

Écrit de zéro pour l'interopérabilité matérielle. Aucun code d'Akai Professional ni de Ploytec. Le format USB, les ports et
les propriétés ont été déterminés en observant le comportement du pilote et du logiciel du constructeur.
Sans lien avec Akai, inMusic ou Ploytec. Licence MIT ; voir `LICENSE` et `NOTICE.md` (libusb, LGPL-2.1).
