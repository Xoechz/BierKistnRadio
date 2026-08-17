# BierKistn Radio UI

The touch-screen interface for a portable, Pi 4B-based radio and Spotify speaker running NixOS in kiosk mode. This repository produces only the Qt6/QML application; the NixOS system configuration (cage, NetworkManager, PipeWire, polkit) lives in a separate system repository that consumes this repo as a flake input.

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
The persistent top strip showing the clock (left), the Source toggle (center), and Reboot/Shutdown icons (right). Wi-Fi and Bluetooth status are shown in the Right Sidebar, not the Status Bar.

**Now-Playing**:
The default and only view. A three-column layout: Left Column (metadata), Center Column (album art + progress + transport), Right Sidebar (sliders + statuses + Wi-Fi settings). Has a Bluetooth Mode variant for Bluetooth-sink playback.

**Bluetooth Mode**:
The Now-Playing variant shown when the speaker is acting as a Bluetooth sink. No scrubber (a passive non-interactive progress bar may show `Position` while the phone publishes it and `duration > 0`). Best-effort AVRCP transport (Play/Pause/Next/Previous) is shown in the Center Column **only while the phone publishes `Status`**; metadata (title/artist/album) **only while `Track` is published**, with title suffixed "via \<device name\>"; when `Track` is not published → "Controlled by \<Paired Device\>" as title, "No metadata available" as subtitle. Paired Device still owns playback initiation; the speaker only mutes outgoing audio during a Spotify state via the mute invariant (see [ADR 0006](./adr/0006-source-clients-and-best-effort-avrcp-controls.md)). Bluetooth Mode is **opt-in** — a phone connecting via BT does NOT automatically enter Bluetooth Mode. The user must explicitly tap the Source toggle to switch. Has two sub-states: `BluetoothWaiting` (no device connected yet, show "Discoverable — connect your phone") and `BluetoothActive` (device connected, show metadata or "Controlled by \<Paired Device\>").
_Avoid_: bluetooth screen, passthrough, sink mode (renamed).

**Source**:
The user-selectable playback input: Spotify or Bluetooth sink. Toggled from the Status Bar — tapping the Source toggle switches between spotifyd (MPRIS2) and BT A2DP sink (PipeWire-routed audio). Not a full view. The current Source is derivable from `PlaybackController.playbackState`.
_Avoid_: input, mode, Source Selection (obsolete — the view was removed).

**Right Sidebar**:
The ~230px panel on the right side of the Now-Playing view. Always visible. Contains: volume slider (left) and brightness slider (right) side by side, dark mode toggle, Bluetooth status text, Wi-Fi status text, and a "Wifi Settings" button that opens the Wi-Fi Dialog. Source-independent — shows the same content regardless of playback state.

**Left Column**:
The ~200px panel on the left side of the Now-Playing view. Shows metadata depending on playback state: track title, artist, album (SpotifyActive); error/hint text (SpotifyUnavailable/SpotifyWaiting/BluetoothWaiting); or device-connected metadata (BluetoothActive).

**Center Column**:
The central area of the Now-Playing view. Contains: 400×400 album art (with fallback SVG), a progress scrubber or passive bar below the art, and transport buttons below the progress. Content adapts to playback state.

**Wi-Fi Dialog**:
A popup overlay opened from the Right Sidebar's "Wifi Settings" button. Shows a scrollable SSID list with signal strength bars, a password field with on-screen keyboard, and a Connect button. Auto-scans on open.

**Takeover Dialog**:
A centered modal popup shown when a second Bluetooth device connects while one is already playing. Asks "Keep current or switch to new?" with a 10-second auto-select countdown defaulting to Keep Current.

**Confirm Dialog**:
A centered modal popup for reboot/shutdown confirmation. Two buttons: Cancel and Confirm (red). 10-second auto-dismiss.

**Settings**:
The previous full-screen view for on-device management. **Replaced by the Right Sidebar** (sliders, statuses) and the Wi-Fi Dialog (SSID list + password). No longer a separate view.

### Playback

**Track**:
A playable item with a title, artist, album, and a fixed duration. Progress is meaningful and scrubbable.
_Avoid_: song (a Track may be a podcast episode).

**Transport**:
The set of playback actions: Play/Pause, Skip Forward, Skip Back. Exposed to the UI through spotifyd's MPRIS2 interface when in Spotify Mode (`SpotifyActive` state). In Bluetooth Mode, transport is best-effort AVRCP — shown only while the Paired Device publishes `Status` (see [ADR 0006](./adr/0006-source-clients-and-best-effort-avrcp-controls.md)). The Paired Device owns initiation; the speaker's controls are a degraded best-effort layer.

**Scrub**:
Dragging the progress slider on a Track to seek to a position. Only available when in `SpotifyActive` state with a valid track duration. Inactive in Bluetooth Mode (AVRCP has no seek-absolute; an option only displays a passive progress bar, no thumb).

### Connectivity

**Paired Device**:
A Bluetooth device that has completed pairing with the speaker. May be connected or disconnected independently of pairing. The phone always initiates pairing and connection — the speaker only advertises itself.
_Avoid_: connected device (a paired device may be disconnected).

**Discoverable**:
The speaker's Bluetooth radio state that allows new phones to find and pair with it. The NixOS system config sets it always on as the base policy; BlueZ drops it on connect, so the **app re-asserts `Discoverable=true` when the user switches to Bluetooth with no device connected**. There is no toggle in the UI.
_Avoid_: pairing mode, discoverable toggle (there is none).

**Takeover**:
The event where a second phone connects to the speaker while one is already streaming. Mediated by a modal dialog: "Keep <current> or switch to <new>?", defaulting to keep current after 10 seconds. Choosing keep disconnects the new phone; switching disconnects the old one.

**Sink**:
The role the speaker plays over Bluetooth: it _receives_ an audio stream from a phone. The phone is the source; the speaker is the sink. This is distinct from spotifyd playback and is routed by PipeWire, not by MPRIS. A phone connecting as a BT source does NOT automatically make the speaker enter Sink Mode — the user must toggle to Bluetooth explicitly.
_Avoid_: Bluetooth source (the speaker is never the Bluetooth source).
