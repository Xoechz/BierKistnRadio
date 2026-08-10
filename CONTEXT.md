# BierKistn Radio UI

The touch-screen interface for a portable, Pi 4B-based radio and Spotify speaker running NixOS in kiosk mode. This repository produces only the Qt6/QML application; the NixOS system configuration (cage, Mopidy, NetworkManager, PipeWire, polkit) lives in a separate system repository that consumes this repo as a flake input.

## Language

### Product & runtime

**BierKistn Radio**:
The portable device itself — a touch-driven radio, Bluetooth speaker, and Spotify player.
_Avoid_: radio (overloaded with the source), the speaker.

**Kiosk**:
The runtime mode of the app: full-screen, single application, no desktop environment, launched under the `cage` Wayland compositor.
_Avoid_: fullscreen mode, kiosk app (the kiosk is the device's runtime, not a setting).

### Views

**Status Bar**:
The persistent top strip showing the clock, Wi-Fi state, Bluetooth state, and the active Source badge. Tapping Wi-Fi or Bluetooth opens the relevant Settings module.

**Now-Playing**:
The default view. Shows current Track or Station art, metadata, a scrubbable progress bar, transport controls, and a volume slider. Has a Sink Mode variant for Bluetooth-sink playback.

**Sink Mode**:
The Now-Playing variant shown when the speaker is acting as a Bluetooth sink. Hides the scrubber and transport controls and displays "Controlled by <Paired Device>". The Paired Device owns playback; the speaker only renders audio routed by PipeWire.
_Avoid_: bluetooth screen, passthrough.

**Source**:
The user-selectable playback input shown in the UI: Spotify, Internet Radio, or Bluetooth sink. A UI/presentation concept derived from current playback state, not a distinct MPRIS player. Mopidy serves Spotify and Internet Radio behind one MPRIS2 interface; the Bluetooth sink is PipeWire-routed audio, not an MPRIS source.
_Avoid_: input, mode.

**Source Selection**:
The view for choosing a Source.

**Settings**:
The view for on-device management: Wi-Fi connection, Bluetooth pairing, screen brightness, and power controls.

### Playback

**Track**:
A playable item with a title, artist, album, and a fixed duration. Progress is meaningful and scrubbable.
_Avoid_: song (a Track may be a podcast episode).

**Station**:
An internet radio stream. Has a name and logo but no fixed duration; progress is a running clock, not a scrubber.

**Transport**:
The set of playback actions: Play/Pause, Skip Forward, Skip Back. Exposed to the UI through a single MPRIS2 interface regardless of which Mopidy-backed Source is active. Inactive in Sink Mode — the Paired Device owns transport, not the speaker.

**Scrub**:
Dragging the progress slider on a Track to seek to a position. Stations are not scrubbable.

### Connectivity

**Paired Device**:
A Bluetooth device that has completed pairing with the speaker. May be connected or disconnected independently of pairing.

**Discoverable**:
The speaker's Bluetooth radio state that allows new phones to find and pair with it. Toggled from the Bluetooth Settings module.

**Sink**:
The role the speaker plays over Bluetooth: it _receives_ an audio stream from a phone. The phone is the source; the speaker is the sink. This is distinct from Mopidy playback and is routed by PipeWire, not by MPRIS.
_Avoid_: Bluetooth source (the speaker is never the Bluetooth source).
