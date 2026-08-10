# System Interface Contract

This document defines the contract between the **BierKistn Radio UI** (this repo) and the **NixOS system repository** that consumes it as a flake input. The system repo is responsible for providing the runtime environment described here; the app assumes all of it is in place and does not configure any of it itself.

For architectural rationale, see [ADR 0001](./docs/adr/0001-mpris2-mopidy-as-playback-abstraction.md) and [ADR 0002](./docs/adr/0002-tech-stack.md). For domain terminology, see [CONTEXT.md](./CONTEXT.md).

---

## 1. Flake output

| Attribute | Type | Systems |
|---|---|---|
| `packages.<system>.bierkistnRadio` | derivation | `x86_64-linux`, `aarch64-linux` |
| `packages.<system>.default` | alias → `bierkistnRadio` | same |

The derivation produces a single executable at `bin/bierkistnRadio`. It is wrapped via `wrapQtAppsHook`, so the wrapper sets `QT_PLUGIN_PATH`, `QML_IMPORT_PATH`, and Qt platform plugin paths at runtime. There is no desktop file, no systemd service, and no D-Bus service file in this package — the system repo is responsible for launching the app and wiring it into the kiosk session.

Consumption in the system repo:

```nix
inputs.bierkistn-radio.url = "github:<owner>/BierKistnRadio";

# ...
environment.systemPackages = [
  inputs.bierkistn-radio.packages.${system}.bierkistnRadio
];
```

---

## 2. Compositor & launch

The app is a Wayland client designed to run full-screen under **cage**. It does not manage its own window decoration or compositor.

- **Compositor:** cage (or any wlroots-based kiosk compositor). Must provide `WAYLAND_DISPLAY`.
- **Launch:** the system repo starts cage with `bierkistnRadio` as the single child process. The app sets `QT_QPA_PLATFORM=wayland` itself (only if not already set), so no env var is required — but it can be overridden for debugging (`QT_QPA_PLATFORM=xcb` for X11 development).
- **Window:** `Qt.FramelessWindowHint` + `Window.FullScreen`, target resolution 1024×600. The app does not call `wlr-randr` or configure output — cage handles that.

---

## 3. D-Bus

The app is a thin D-Bus client. It connects to **both** the session bus and the system bus. It never calls D-Bus directly from QML — all calls go through the C++ controllers.

### Session bus

| Controller | Service | Role |
|---|---|---|
| `PlaybackController` | `rs.spotifyd.Controls` (`rs.spotifyd.instance$PID`) | Custom controls: `TransferPlayback`, `VolumeUp`, `VolumeDown`. Available once spotifyd connects to Spotify, even before it's the active device. |
| `PlaybackController` | MPRIS2 (`org.mpris.MediaPlayer2.spotifyd.instance$PID`) | Track metadata, transport (play/pause/next/previous/seek), position. Available **only when spotifyd is the active playback device** (after `TransferPlayback` or Spotify Connect selection). |
| `ArtCache` | — | Reads `mpris:artUrl` values (remote `https://` URLs from Spotify CDN) |

**Note on the `$PID` suffix:** spotifyd's well-known names include its PID, which changes on restart. The app discovers the name dynamically by listing bus names and matching `rs.spotifyd.*` / `org.mpris.MediaPlayer2.spotifyd.*`, or uses `QDBusServiceWatcher` with a name match. The system repo must not hardcode the PID.

### System bus

| Controller | Service | Role |
|---|---|---|
| `WifiController` | `org.freedesktop.NetworkManager` | Scan, connect, disconnect, connection state |
| `BluetoothController` | `org.bluez` | Device discovery, pairing, connect/disconnect, discoverable toggle |
| `PlaybackController` | `org.bluez` (MediaTransport) | Sink Mode detection (A2DP source connected) |

### Polkit

The app assumes the kiosk user has been granted the D-Bus actions required by NetworkManager and BlueZ. The system repo owns the **polkit rule** that grants these. If the calls fail with a D-Bus auth error, the app surfaces a "Permission denied — check system config" state — it never silently fails.

### spotifyd session bus sharing

spotifyd exposes both its custom `rs.spotifyd.Controls` interface and MPRIS2 on the **session bus** by default. For the app to see spotifyd's interfaces, **spotifyd and the app must share the same D-Bus session bus**. The system repo achieves this by running spotifyd as a **systemd user service** (`systemctl --user`), not a system service:

- cage creates the logind session → `systemd --user` starts → spotifyd starts
- The app, also launched by cage in the same session, inherits the same `DBUS_SESSION_BUS_ADDRESS`
- Both processes share the session bus automatically — no bus address injection, no UID guessing at build time, no ordering dependencies on the cage service

The app uses `QDBusConnection::sessionBus()` for all spotifyd communication. No `DBUS_SESSION_BUS_ADDRESS` configuration is needed in the app or the system repo — the user service environment provides it.

Alternatively, spotifyd can use the system bus (`dbus_type = "system"` in config), but this requires a D-Bus policy file granting the kiosk user ownership of `rs.spotifyd.*` and `org.mpris.MediaPlayer2.spotifyd.*`. The systemd user service approach is preferred — it follows the MPRIS2 convention, avoids extra policy configuration, and handles the session bus lifecycle correctly.

---

## 4. Runtime dependencies (on PATH or in environment)

| Dependency | Used by | Notes |
|---|---|---|
| `wpctl` | `VolumeController` | Shells out: `wpctl set-volume @DEFAULT_AUDIO_SINK@ <pct>%`. Comes from `wireplumber`. Must be on `PATH`. |
| `wireplumber` (daemon) | `VolumeController` | `wpctl` requires a running `wireplumber` daemon. |
| `pipewire` (daemon) | audio routing | Required by wireplumber and for Bluetooth A2DP sink routing. |
| `spotifyd` | `PlaybackController` | The Spotify backend. Exposes `rs.spotifyd.Controls` and MPRIS2 on the session bus. Must be running with `use_mpris = true`. The system repo runs it as a **systemd user service** so it shares the kiosk user's session bus automatically. |
| D-Bus session bus | `PlaybackController` | spotifyd's MPRIS2 and Controls interfaces live on the session bus. Provided automatically by `systemd --user` when cage creates the logind session — no manual `DBUS_SESSION_BUS_ADDRESS` configuration needed. |
| D-Bus system bus | `WifiController`, `BluetoothController` | Standard system bus — always available under systemd. |
| Qt Wayland platform plugin | rendering | Bundled via `wrapQtAppsHook`. Needs `wayland` client libs (provided by `qtwayland` build input). |

The app does **not** bundle or require:

- `mopidy` — dropped. Spotify is served by spotifyd; radio via Bluetooth sink.
- `network-manager` / `bluez` daemons — system services.
- `polkit` / `soteria` — system services.

---

## 5. Qt modules used

Linked at build time via `find_package(Qt6 ...)`:

- `Core`, `Gui`, `Qml`, `Quick`, `QuickControls2`, `Svg`, `VirtualKeyboard`, `Test`, `QuickTest`
- `qtwayland` (platform plugin, runtime)
- `qtsvg`, `qtimageformats` (additional image format support for album art)

All Qt6 packages in nixpkgs live under `kdePackages.*`.

`QtQuick.VirtualKeyboard` is GPLv3 in Qt6 — this app is GPLv3 as a result (see [ADR 0002](./docs/adr/0002-tech-stack.md)).

---

## 6. QML module

- **URI:** `BierKistnRadio`
- **Version:** 1.0
- **Entry point:** `engine.loadFromModule("BierKistnRadio", "Main")` (see `src/main.cpp`)
- **Singletons exposed to QML:** `Theme` (QML singleton), `PlaybackController`, `WifiController`, `BluetoothController`, `VolumeController`, `ArtCache` (C++ `QML_SINGLETON`s)

The QML module is compiled into the binary via `qt_add_qml_module` (AOT-cached, resources embedded). No external QML files are loaded at runtime — everything is in the Qt resource system (`qrc:/qt/qml/BierKistnRadio/`).

---

## 7. Filesystem

| Path | Purpose | Notes |
|---|---|---|
| `~/.cache/BierKistnRadio/BierKistnRadio/art/` | ArtCache | Album/station art cache. `QStandardPaths::CacheLocation` with orgName/appName both `BierKistnRadio`. The app creates this dir on startup. Needs a writable home directory or `XDG_CACHE_HOME`. |

The app does **not** write to `/sys`, `/etc`, or any system path. It does not modify mopidy config, network config, or bluetooth config files — all system changes go through D-Bus.

---

## 8. Organization & application name

Set in `src/main.cpp`:

- `app.setOrganizationName("BierKistnRadio")`
- `app.setApplicationName("BierKistnRadio")`

These affect `QStandardPaths` locations (cache, config, data). The system repo should not override them.

---

## 9. Brightness control (open)

Brightness control is not yet implemented (TODO T16). The system repo should provide one of:

- A writable `/sys/class/backlight/<panel>/brightness` path, or
- A D-Bus backlight interface (e.g. `org.freedesktop.login1` or a custom helper).

The app will read/write brightness from whichever interface the system provides. This decision should be coordinated when T16 is implemented.

---

## 10. Power controls

The app will offer Reboot / Power Off buttons (TODO T16). The intended mechanism is `org.freedesktop.login1` D-Bus calls (`PowerOff`, `Reboot`). The system repo must grant the kiosk user permission to call these (polkit rule for `org.freedesktop.login1.power-off` / `org.freedesktop.login1.reboot`).

---

## 11. What the app does NOT do

This list is as important as what it does — it defines the boundary.

- Does **not** configure or start spotifyd, NetworkManager, BlueZ, PipeWire, or wireplumber.
- Does **not** write spotifyd config, network config, or bluetooth config files.
- Does **not** manage the cage compositor or Wayland output configuration.
- Does **not** set up or manage the polkit / soteria agent.
- Does **not** create or manage the kiosk user account.
- Does **not** coordinate audio exclusivity between spotifyd and Bluetooth A2DP — both may play simultaneously; the user manages this manually.
- Does **not** route Bluetooth audio — PipeWire/wireplumber handles that. The app only detects sink mode (an A2DP source is connected) and displays the state.
- Does **not** provide a desktop file, systemd service, or D-Bus service file.

---

## 12. Testing on the target

The app binary can be tested on the Pi without the full system config:

```sh
# On the Pi, with a Wayland compositor running:
bierkistnRadio  # launches full-screen

# Or in a window for debugging:
QT_QPA_PLATFORM=xcb bierkistnRadio
```

For the full kiosk experience, the system repo's cage configuration should launch `bierkistnRadio` as the only child process.
