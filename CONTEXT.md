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

**Bluetooth Mode**:
The Now-Playing variant shown when the speaker is acting as a Bluetooth sink. Hides the scrubber and transport controls and displays "Controlled by <Paired Device>". The Paired Device owns playback; the speaker only renders audio routed by PipeWire. Bluetooth Mode is **opt-in** — a phone connecting via BT does NOT automatically enter Bluetooth Mode. The user must explicitly tap the Source toggle to switch. Has two sub-states: `BluetoothWaiting` (no device connected yet, show "Discoverable — connect your phone") and `BluetoothActive` (device connected, show "Controlled by <Paired Device>").
_Avoid_: bluetooth screen, passthrough, sink mode (renamed).

**Source**:
The user-selectable playback input: Spotify or Bluetooth sink. Toggled from the Status Bar — tapping the Source badge switches between spotifyd (MPRIS2) and BT A2DP sink (PipeWire-routed audio). Not a full view. The current Source is derivable from `PlaybackController.playbackState`.
_Avoid_: input, mode, Source Selection (obsolete — the view was removed).

**Source Selection**:
The view for choosing a Source.

**Settings**:
The view for on-device management: Wi-Fi connection, Bluetooth state, screen brightness, and power controls. Bluetooth is state-only (shows the connected device and takeover dialog) — pairing and connection happen on the phone, never on this screen.

### Playback

**Track**:
A playable item with a title, artist, album, and a fixed duration. Progress is meaningful and scrubbable.
_Avoid_: song (a Track may be a podcast episode).

**Transport**:
The set of playback actions: Play/Pause, Skip Forward, Skip Back. Exposed to the UI through spotifyd's MPRIS2 interface when in Spotify Mode (`SpotifyActive` state). Inactive in Bluetooth Mode — the Paired Device owns transport, not the speaker.

**Scrub**:
Dragging the progress slider on a Track to seek to a position. Only available when in `SpotifyActive` state with a valid track duration. Inactive in Bluetooth Mode.

### Connectivity

**Paired Device**:
A Bluetooth device that has completed pairing with the speaker. May be connected or disconnected independently of pairing. The phone always initiates pairing and connection — the speaker only advertises itself.
_Avoid_: connected device (a paired device may be disconnected).

**Discoverable**:
The speaker's Bluetooth radio state that allows new phones to find and pair with it. Owned by the NixOS system config — always on, not toggled from the UI. A phone pairing compares over JustWorks; the user never drives pairing from the app.
_Avoid_: pairing mode, discoverable toggle (there is none).

**Takeover**:
The event where a second phone connects to the speaker while one is already streaming. Mediated by a modal dialog: "Keep <current> or switch to <new>?", defaulting to keep current after 10 seconds. Choosing keep disconnects the new phone; switching disconnects the old one.

**Sink**:
The role the speaker plays over Bluetooth: it _receives_ an audio stream from a phone. The phone is the source; the speaker is the sink. This is distinct from spotifyd playback and is routed by PipeWire, not by MPRIS. A phone connecting as a BT source does NOT automatically make the speaker enter Sink Mode — the user must toggle to Bluetooth explicitly.
_Avoid_: Bluetooth source (the speaker is never the Bluetooth source).
