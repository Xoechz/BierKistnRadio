# System Interface Contract

This document defines the contract between the **BierKistn Radio UI** (this repo) and the **NixOS system repository** that consumes it as a flake input. The system repo is responsible for providing the runtime environment described here; the app assumes all of it is in place and does not configure any of it itself.

For architectural rationale, see [ADR 0001](./docs/adr/0001-mpris2-mopidy-as-playback-abstraction.md) and [ADR 0002](./docs/adr/0002-tech-stack.md). For domain terminology, see [CONTEXT.md](./CONTEXT.md).

---

## 1. Flake output

| Attribute | Type | Systems |
| --- | --- | --- |
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
| --- | --- | --- |
| `SpotifyClient` | MPRIS2 (`org.mpris.MediaPlayer2.spotifyd.instance$PID`) | Track metadata, transport (play/pause/next/previous/seek), position. Present once spotifyd is connected to Spotify; whether a track is loaded indicates active playback (see [ADR 0005](./docs/adr/0005-drop-rs-spotifyd-controls.md)). |
| `ArtCache` | — | Reads `mpris:artUrl` values (remote `https://` URLs from Spotify CDN) |

**Note on the `$PID` suffix:** spotifyd's well-known names include its PID, which changes on restart. The app discovers the MPRIS2 name dynamically by listing bus names and matching `org.mpris.MediaPlayer2.spotifyd.*`, or uses `QDBusServiceWatcher` with a name match. The system repo must not hardcode the PID.

### System bus

| Controller | Service | Role |
| --- | --- | --- |
| `WifiController` | `org.freedesktop.NetworkManager` | Scan, connect, disconnect, connection state |
| `BluetoothClient` | `org.bluez` | `Device1` state (connected device, takeover kick), `MediaPlayer1` for best-effort AVRCP transport/metadata, `Adapter1.Set` for the `Discoverable` re-assertion |
| `PlaybackController` | — | Facade only; no D-Bus of its own. Routes transport to `SpotifyClient` / `BluetoothClient`. |

### Bluetooth connection model

The Bluetooth sink is **phone-driven** — see [ADR 0004](./docs/adr/0004-phone-driven-bluetooth-connection-model.md). The app's `BluetoothClient` handles **connection state only** and never initiates pairing, discovery, or connection. The system repo owns:

- **Always discoverable + pairable (base policy).** Adapter set to `Discoverable=true`, `Pairable=true`, `DiscoverableTimeout=0` (persist), `AutoEnable=true`, `Powered=true`. BlueZ automatically drops `Discoverable` once a device connects, so the base policy is NOT re-asserted by an ongoing system service. Instead, the **app re-asserts** `Discoverable=true` on the adapter (`org.bluez.Adapter1.Set`) whenever the user switches to the Bluetooth source while no device is connected (`BluetoothWaiting`). The app has no Discoverable *toggle*, but it does issue this one-shot assertion on entering `BluetoothWaiting`. The D-Bus policy must authorize the kiosk user for `org.bluez.Adapter1.Set`.
- **A2DP-sink-only role** in WirePlumber: `bluez5.roles = [ a2dp_sink ]`, `device.profile = "a2dp-sink"`, `bluez5.auto-connect = []`, and `bluez5.enable-sbc-xq = true` for high-quality SBC codec.

The app observes `org.bluez.Device1` objects (`PropertiesChanged` for `Connected`/`Name`) and calls `Device1.Disconnect()` to kick a device. **Takeover**: when a second phone connects while one is active, the app shows a modal dialog ("Keep <current> or switch to <new>?", default keep after 10 s) and disconnects accordingly — see [ADR 0004](./docs/adr/0004-phone-driven-bluetooth-connection-model.md).

**Best-effort AVRCP controls (ADR 0006):** when a device is connected, the app offers best-effort transport/metadata via `org.bluez.MediaPlayer1` on the device's `playerN` path. This is a playback layer; the *connection* model is unchanged and remains phone-driven. No new daemon or polkit surface is required beyond what the phone-driven model already grants the kiosk user, but the polkit rule must authorize the kiosk user for `org.bluez.MediaPlayer1` method calls (Play/Pause/Next/Previous) on connected devices.

**Audio exclusivity mute (ADR 0006):** to guarantee no audio overlap when switching sources, the app mutes the A2DP stream by finding the connected device's bluez audio PipeWire node — **matched by `api.bluez5.address` (the connected MAC), not by a hardcoded node name**. The node's name shape is device/profile-dependent (e.g. `bluez_input.<MAC>.2` or `bluez_output.<MAC>.a2dp-sink`; a Samsung phone surfaced as `bluez_input.<MAC>.2` on mainline). It then runs `wpctl set-mute <node id>`. It does **not** mute `@DEFAULT_AUDIO_SINK@` (spotifyd shares that path). The mute is re-asserted whenever a BT stream appears while the app is in a Spotify state, and cleared on switching back to Bluetooth. The system repo must ensure `pw-cli` and `pw-dump` are on PATH (they ship with `pipewire`, already required). The phone's A2DP transport is intentionally **not** disconnected by this — the stream is kept "captured" but inaudible.

**logind active-session requirement:** BlueZ/PipeWire only expose Bluetooth device/nodes to the **active logind session**. The kiosk user must hold the active seat (cage creates the session) or Bluetooth devices will not appear. If seat-monitoring interferes, set `monitor.bluez.seat-monitoring = disabled` in WirePlumber.

### Polkit

The app assumes the kiosk user has been granted the D-Bus actions required by NetworkManager and BlueZ. The system repo owns the **polkit rule** that grants these. If the calls fail with a D-Bus auth error, the app surfaces a "Permission denied — check system config" state — it never silently fails.

### spotifyd session bus sharing

spotifyd exposes MPRIS2 on the **session bus** by default. For the app to see it, **spotifyd and the app must share the same D-Bus session bus**. The system repo achieves this by running spotifyd as a **systemd user service** (`systemctl --user`), not a system service:

- cage creates the logind session → `systemd --user` starts → spotifyd starts
- The app, also launched by cage in the same session, inherits the same `DBUS_SESSION_BUS_ADDRESS`
- Both processes share the session bus automatically — no bus address injection, no UID guessing at build time, no ordering dependencies on the cage service

The app uses `QDBusConnection::sessionBus()` for all spotifyd communication. No `DBUS_SESSION_BUS_ADDRESS` configuration is needed in the app or the system repo — the user service environment provides it.

---

## 4. Runtime dependencies (on PATH or in environment)

| Dependency | Used by | Notes |
| --- | --- | --- |
| `wpctl` | `VolumeController`, `BluetoothClient` | Shells out: `wpctl set-volume @DEFAULT_AUDIO_SINK@ <pct>%` (slider); `wpctl set-mute <node id>` (BT stream mute, [ADR 0006](./docs/adr/0006-source-clients-and-best-effort-avrcp-controls.md)). Comes from `wireplumber`. Must be on `PATH`. |
| `pw-cli` / `pw-dump` | `BluetoothClient` | BlueZ sink node discovery for the A2DP mute. Ship with `pipewire`. Must be on `PATH`. |
| `wireplumber` (daemon) | `VolumeController`, `BluetoothClient` | `wpctl` requires a running `wireplumber` daemon. |
| `pipewire` (daemon) | audio routing | Required by wireplumber and for Bluetooth A2DP sink routing. |
| `spotifyd` | `PlaybackController` | The Spotify backend. Exposes MPRIS2 on the session bus. Must be running with `use_mpris = true`. The system repo runs it as a **systemd user service** so it shares the kiosk user's session bus automatically. |
| D-Bus session bus | `PlaybackController` | spotifyd's MPRIS2 interface lives on the session bus. Provided automatically by `systemd --user` when cage creates the logind session — no manual `DBUS_SESSION_BUS_ADDRESS` configuration needed. |
| D-Bus system bus | `WifiController`, `BluetoothClient` | Standard system bus — always available under systemd. |
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
- **Singletons exposed to QML:** `Theme` (QML singleton), `PlaybackController` (facade; source data via `PlaybackController.spotify.*` / `PlaybackController.bluetooth.*`, see [ADR 0006](./docs/adr/0006-source-clients-and-best-effort-avrcp-controls.md)), `WifiController`, `VolumeController`, `ArtCache` (C++ `QML_SINGLETON`s)

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
- Does **not** coordinate audio exclusivity between spotifyd and Bluetooth A2DP **except for the toggle mute** — the explicit source toggle mutes the outgoing BT stream (ADR 0006) to guarantee no overlap; passive BT connects and automatic events never trigger pause/mute. Audio overlap is otherwise the user's responsibility.
- Does **not** route Bluetooth audio — PipeWire/wireplumber handles that. The app only observes an A2DP source connect (state) and, when muted per the ADR 0006 invariant, silences the bluez sink node via `wpctl set-mute`.
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

---

## 13. Implementation status — confirmed present

Verified against `modules/bierkistn.nix` and `modules/hosts/piKistn.nix` in the system repo (as of this writing). The following contract items are correctly implemented:

| Contract requirement | Where |
| --- | --- |
| `bierkistn-radio` flake input (`github:Xoechz/BierKistnRadio`, nixpkgs follows) | `bierkistn.nix` `flake-file.inputs` |
| Launch `bierkistnRadio` as cage's single child | `piKistn.nix` `services.cage` (`program = inputs.bierkistn-radio.packages.${system}.bierkistnRadio`) |
| `QT_QPA_PLATFORM=wayland` | `services.cage.environment` |
| Writable home / `XDG_CACHE_HOME=/home/kistn/.cache` (ArtCache §7, §8) | `services.cage.environment` |
| spotifyd as a **systemd user service** so it shares the kiosk user's session bus (MPRIS2 on session bus, §3) | `bierkistn.nix` Home Module `bierkistn` (`systemd.user.services.spotifyd`, `--config-path /etc/spotifyd.conf`) |
| `use_mpris = true` + `device_name = hostname` + 320kbps | `environment.etc.spotifyd.conf` |
| Always discoverable + pairable base policy (`Discoverable`/`Pairable`/`DiscoverableTimeout=0`) with `AutoEnable=true` (`Powered` implicit via `AutoEnable`) | `bierkistn.nix` `hardware.bluetooth.settings.General` |
| Auto-accept pairing (NoInputNoOutput, kiosk has no display) | `systemd.services.bt-agent` |
| A2DP-sink-only + best-effort AVRCP (roles, `auto-connect = []`, `enable-sbc-xq`, `dummy-avrcp-player`, `device.profile`) | `services.pipewire.wireplumber.extraConfig."10-bierkistn"` |
| PipeWire/WirePlumber started under the kiosk session (no graphical-session dependency) | `systemd.user.services.{pipewire,wireplumber}.wantedBy = default.target` |
| **Power controls** polkit grant (`org.freedesktop.login1.*` for Reboot/Power Off, §10) | `security.polkit.extraConfig` |
| kiosk user in `bluetooth` / `networkmanager` / `audio` / `video` groups | `users.users.kistn.extraGroups` |

## 14. Missing / not-yet-implemented configurations

These contract items are **NOT** satisfied by the current system-module config:

### 14.1 BlueZ polkit grant (BluetoothClient D-Bus actions) — gap

The interface requires the kiosk user be authorized for the **BlueZ** actions `BluetoothClient`/`WifiController` call (see §3 *Polkit*):

- `org.bluez.Adapter1.Set` — the `Discoverable=true` one-shot re-assertion on entering `BluetoothWaiting` (§3 *Bluetooth connection model*).
- `org.bluez.MediaPlayer1` method calls (Play/Pause/Next/Previous best-effort AVRCP) — explicitly required by §3 *Best-effort AVRCP controls*.
- `org.bluez.Device1.Disconnect` — the takeover "kick" (§3).

The current `security.polkit.extraConfig` rule grants **only** `org.freedesktop.NetworkManager.*` and `org.freedesktop.login1.*`. **No BlueZ action is granted.**

This may not bite in practice because BlueZ's net-effect is often gated by the caller being the active session user and a member of the `bluetooth` group (the kiosk user holds the active seat under cage and is in `extraGroups.bluetooth`), rather than by polkit. **Action:** verify on-device whether `Adapter1.Set Property Discoverable`, `Device1.Disconnect`, and `MediaPlayer1.Play/Pause/Next/Previous` succeed for the `kistn` user; if they are policy-rejected, add a polkit rule granting `org.bluez.*` to user `"kistn"` (e.g. prefix-match on `"org.bluez."`).

### 14.2 Brightness control (still open, §9)

§9 is marked "open" pending T16, and nothing has been supplied:

- No writable `/sys/class/backlight/<panel>/brightness` is configured, and
- No D-Bus backlight interface is exposed.

The Pi's 7" panel is driven over **HDMI** (GPU output), which exposes no conventional `backlight` sysfs node — so a `wlr-brightness` (Wayland) or `ddcutil`/custom interface is the likely path, not `/sys/class/backlight`. **Action:** pick and expose one interface when the brightness Task is implemented; document which interface the app must target.

### 14.3 Room-note: `Powered` and seat-monitoring

- `Powered = true` is not set explicitly, but `AutoEnable = true` powers the adapter at boot — treated as satisfied, no action needed.
- `monitor.bluez.seat-monitoring` is unset (default). The logind active-session note in §3 is a *conditional* ("if seat-monitoring interferes") — only address if Bluetooth nodes fail to appear in practice.
