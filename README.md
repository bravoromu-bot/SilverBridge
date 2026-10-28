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

## Optional: Launchpad Pro MK3 colour feedback

`Launchpad Bridge.command` (source: `launchpad_bridge.c`) lights a Novation Launchpad Pro MK3 in Programmer layout:

- **Silver pads** — hitting a pad on the MPC Studio lights the matching Launchpad pad.
- **MPC pad lights** — whatever MPC lights on the Silver (notes you play or the sequence plays) is mirrored: the driver
  sends a local copy of MPC's pad-LED messages to the bridge (loopback UDP, never leaves the machine).
  Colours are mirrored exactly (RGB). Default MPC drum pads map to a 4x4 block at the bottom-left of the Launchpad.
- **MPC output** — notes MPC sends to the virtual port `SilverBridge Launchpad` (choose it in MPC's MIDI settings
  or as a track's MIDI output) light the grid.

Colour follows velocity (green, yellow, red); notes map onto the 8x8 grid in blocks of 64.
Each source can be switched off: `./launchpad-bridge --no-silver`, `--no-mpc` or `--no-leds`.
`--native-note` keeps the Launchpad in its own Note mode and lights it from the notes MPC plays
(channel-16 note injection on the DAW port); use it for plug-in tracks. In MPC, set the track's Out to
`SilverBridge Launchpad` (not `None`): the pad-light mirror is not used in this mode because it reports pad notes, not pitches. Close the window to stop; the Launchpad returns to
its normal layout. Only tested with the Pro MK3.

## Separate package: Launchpad bridge (`SilverBridgeLaunchpadMK3.pkg`)

The Launchpad bridge ships as its own installer, independent of the MPC driver pkg (build: `./make_launchpad_pkg.sh`).
It installs a login agent and a control script in `/Library/Application Support/SilverBridgeLaunchpadMK3/`.
Without the SilverBridge driver it runs in the Launchpad's native Note mode (no pad mirror, no auto-switch).
The driver does not need the bridge either.

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

## Optionnel : retour de couleur sur Launchpad Pro MK3

`Launchpad Bridge.command` (source : `launchpad_bridge.c`) allume un Novation Launchpad Pro MK3 en mode Programmer :

- **Pads de la Silver** : frapper un pad de la MPC Studio allume le pad correspondant du Launchpad.
- **Lumières des pads de MPC** : ce que MPC allume sur la Silver (notes jouées ou lues par la séquence) est reproduit : le pilote
  envoie une copie locale de ces messages au pont (UDP sur la boucle locale, rien ne sort de la machine).
  Les couleurs sont reproduites à l'identique (RVB). Les pads de batterie MPC par défaut occupent un bloc 4x4 en bas à gauche du Launchpad.
- **Sortie MPC** : les notes envoyées par MPC au port virtuel `SilverBridge Launchpad` (à choisir dans les réglages MIDI de MPC
  ou en sortie MIDI d'une piste) allument la grille.

La couleur dépend de la vélocité (vert, jaune, rouge) ; les notes sont réparties sur la grille 8x8 par blocs de 64.
Chaque source peut être désactivée : `./launchpad-bridge --no-silver`, `--no-mpc` ou `--no-leds`.
`--native-note` laisse le Launchpad dans son propre mode Note et l'allume avec les notes jouées par MPC (injection de notes
canal 16 sur le port DAW) ; à utiliser pour les pistes plug-in. Dans MPC, régler la sortie (Out) de la piste sur
`SilverBridge Launchpad` (pas `None`) : le miroir des lumières de pads n'est pas utilisé dans ce mode, car il indique des notes de pads et non des hauteurs. Fermer la fenêtre pour arrêter ; le Launchpad revient à son
mode normal. Testé uniquement avec le Pro MK3.

## Paquet séparé : pont Launchpad (`SilverBridgeLaunchpadMK3.pkg`)

Le pont Launchpad est livré dans son propre installeur, indépendant du pkg du pilote MPC (construction : `./make_launchpad_pkg.sh`).
Il installe un agent de démarrage et un script de contrôle dans `/Library/Application Support/SilverBridgeLaunchpadMK3/`.
Sans le pilote SilverBridge, il reste dans le mode Note natif du Launchpad (ni miroir des pads, ni bascule automatique).
Le pilote n'a pas besoin du pont non plus.

## Mentions légales

Écrit de zéro pour l'interopérabilité matérielle. Aucun code d'Akai Professional ni de Ploytec. Le format USB, les ports et
les propriétés ont été déterminés en observant le comportement du pilote et du logiciel du constructeur.
Sans lien avec Akai, inMusic ou Ploytec. Licence MIT ; voir `LICENSE` et `NOTICE.md` (libusb, LGPL-2.1).
