# AGENTS.md: BierKistn Radio UI

Working instructions for any agent (human or AI) touching this repository. For domain terminology, see [CONTEXT.md](./CONTEXT.md). For architectural decisions and their rationale, see [docs/adr/](./docs/adr/).

---

## 1. Scope & Repository Boundary

This repository produces **only the Qt6/QML touch-screen application**. It does **not** contain the NixOS system configuration.

The separate **NixOS system repository** (consumes this repo as a flake input) owns:

- The `cage` Wayland compositor kiosk setup that launches this app
- `spotifyd` (with `use_mpris = true`) as the Spotify backend — exposes MPRIS2 on the session bus
- `NetworkManager`, `BlueZ`, `PipeWire`/`wireplumber` daemons
- `polkit` + a `soteria` agent + a polkit rule granting the kiosk user the D-Bus actions this app calls (Wi-Fi connect, Bluetooth disconnect, etc.)

This app is a **thin D-Bus client**: it assumes the system environment grants those calls. Controllers must surface D-Bus auth errors as a visible "Permission denied — check system config" state, never a silent failure.

## 2. Device & Runtime Context

- **Hardware:** Raspberry Pi 4B, 7" 1024×600 IPS capacitive touchscreen.
- **Runtime:** Kiosk mode under `cage` (Wayland), no desktop environment, single full-screen application.
- **Target arch:** `aarch64-linux` (the Pi). Dev arch: `x86_64-linux`.

## 3. Technology Stack

- **UI:** Qt 6 / QML (Qt Quick), Material style, dark-mode-first with light-mode toggle.
- **Backend:** C++20 (`QObject` controllers owning D-Bus subscriptions, republishing state to QML via `Q_PROPERTY`/`Q_INVOKABLE`).
- **Build:** CMake with `qt_add_qml_module` (compiled QML resources, AOT-cached).
- **Packaging:** Nix flake — `packages.<system>.bierkistnRadio` for `x86_64-linux` (dev/test) and `aarch64-linux` (deploy).
- **Playback abstraction:** MPRIS2 over D-Bus via spotifyd (Spotify), plus Bluetooth A2DP sink + best-effort AVRCP controls via BlueZ (radio/audio from a paired phone). Playback source state lives in `SpotifyClient` / `BluetoothClient`, coordinated by the `PlaybackController` facade — see [ADR 0003](./docs/adr/0003-spotifyd-and-bluetooth-sink-mopidy-dropped.md) and [ADR 0006](./docs/adr/0006-source-clients-and-best-effort-avrcp-controls.md).
- **Volume:** `wpctl set-volume @DEFAULT_AUDIO_SINK@ <pct%>` (wireplumber CLI), not a linked C library. Source-independent — shown in every state.
- **Bluetooth audio mute (overlap guarantee):** muting the A2DP stream = find the connected device's bluez audio node via `pw-dump` (match by `api.bluez5.address` == the connected MAC, e.g. `bluez_input.<MAC>.2` or `bluez_output.<MAC>.a2dp-sink` — the node name shape is **device/profile-dependent**, never hardcode it), then `wpctl set-mute <node id>`. Never mute `@DEFAULT_AUDIO_SINK@` (that's spotifyd's path too). See [ADR 0006](./docs/adr/0006-source-clients-and-best-effort-avrcp-controls.md).
- **System D-Bus interfaces used:** MPRIS2 (`org.mpris.MediaPlayer2.spotifyd.instance$PID`), `org.bluez` (`Device1`, `Adapter1`, `MediaPlayer1` for best-effort AVRCP), NetworkManager, BlueZ.
- **On-screen keyboard:** `QtQuick.VirtualKeyboard` — **GPLv3/commercial in Qt6**. This app is GPLv3 as a result. See [ADR 0002](./docs/adr/0002-tech-stack.md).

Qt6 modules in nixpkgs live under `kdePackages.*` (e.g. `kdePackages.qtbase`, `kdePackages.qtdeclarative`). `qtquickcontrols2` is bundled into `qtdeclarative` in Qt6 — CMake still uses `find_package(Qt6 COMPONENTS QuickControls2)`.

## 4. UI Layout & Views

Single full-screen three-column layout with a persistent status bar. No view switching — the Now-Playing view is always visible. Wi-Fi setup uses a dialog overlay; takeover and power actions use modal dialogs. Source switching is a toggle in the Status Bar.

### A. Status Bar (persistent top strip)

- Height: 48px. Three regions: clock (left), Source toggle (center), Reboot/Shutdown icons (right).
- **Clock:** current time, left-aligned.
- **Source toggle:** "Spotify ○══○ Bluetooth" — tapping it switches between Spotify and Bluetooth.
  - Spotify → calls `PlaybackController.switchToSpotify()`: mutes the Bluetooth stream (if active), then sends a best-effort AVRCP `Pause`, then queries the bus for the appropriate Spotify state. See the mute invariant in [ADR 0006](./docs/adr/0006-source-clients-and-best-effort-avrcp-controls.md).
  - Bluetooth → calls `PlaybackController.switchToBluetooth()`: pauses spotifyd, unmutes the Bluetooth stream, then enters `BluetoothWaiting` (no device connected) or `BluetoothActive` (device already connected).
  - Shows "…" while switching; resets when `playbackState` changes. A 3-second timeout resets the "…" if no state change arrives (prevents stuck limbo).
  - A phone connecting via BT does NOT automatically switch the app to Bluetooth mode — only the user tapping the toggle does. (It *does* get muted if the app is in a Spotify state — the mute invariant, not a state change.)
- **Reboot / Shutdown icons:** icon-only buttons in the top-right. Tapping opens a confirmation dialog (see §4.F).

### B. Now-Playing (default and only view)

Three-column layout driven by `PlaybackController.playbackState` — a five-state enum:

```
┌──────────┬──────────────────────────┬──────────────┐
│  Status Bar (clock | toggle | power)               │
├──────────┼──────────────────────────┼──────────────┤
│          │                          │  Right       │
│  Left    │  Center                  │  Sidebar     │
│  Column  │  Column                  │              │
│          │                          │  brightness  │
│  Track   │  Album Art (400×400)     │  volume      │
│  Artist  │                          │  dark mode   │
│  Album   │  Progress / Scrubber     │  BT status   │
│  (state) │  Transport               │  Wi-Fi status│
│          │                          │  Wifi Settings│
└──────────┴──────────────────────────┴──────────────┘
```

#### Left Column (metadata, ~230px wide)

Content depends on playback state:

- **`SpotifyUnavailable`:** Error text — "Spotify service not running — check system config".
- **`SpotifyWaiting`:** Hint text — "Open Spotify on your phone — choose this speaker".
- **`SpotifyActive`:** Track title (bold, large, word-wrap max 3 lines then elide), Artist, Album, Release Date (separate rows). Release Date is resolved via MusicBrainz lookup (see TODO T24).
- **`BluetoothWaiting`:** Hint text — "Discoverable — connect your phone".
- **`BluetoothActive`:**
  - If `trackPublished`: Track title, Artist, Album (word-wrap max 3 lines then elide). Title suffixed "via \<device name\>".
  - If `!trackPublished`: "Controlled by \<device name\>" as title, "No metadata available" as subtitle.

#### Center Column (album art + playback)

- **Album art:** 400×400 `Image` with `fillMode: Image.PreserveAspectFit`. Shows `spotify.artUrl` (SpotifyActive) or fallback SVG (`qrc:/qt/qml/BierKistnRadio/assets/fallback-album.svg`) when `artUrl` is empty. `asynchronous: true`. Always shown (fallback fills the space in non-active states).
- **Progress scrubber** (below album art):
  - `SpotifyActive`: interactive `Slider` with thumb. Time labels flanking: current timestamp (left, `m:ss`) and total duration (right, `m:ss`). `onMoved` calls `PlaybackController.seek(value * 1000)` (slider in seconds, API in milliseconds).
  - `BluetoothActive` + `positionPublished` + `duration > 0`: passive non-interactive progress bar (no thumb), time labels (`m:ss` current / total). No seek.
  - `BluetoothActive` + `positionPublished` + `duration <= 0`: passive bar, no time labels.
  - All other states: no progress element.
- **Transport** (below progress):
  - `SpotifyActive`: Play/Pause + Next + Previous buttons. Play/Pause glyph reflects `isSpotifyPlaying`.
  - `BluetoothActive` + `statusPublished`: Play/Pause + Next + Previous buttons (best-effort AVRCP). Play/Pause glyph reflects `isBluetoothPlaying`.
  - All other states: no transport.

#### Right Sidebar (~230px wide, always visible)

Top-to-bottom, source-independent:
1. **Brightness slider** (vertical, 0–100%, label shows current %)
2. **Volume slider** (vertical, 0–150%, label shows current %, tick mark at 100%)
3. **Dark mode toggle** (small switch, sun/moon icon)
4. **Bluetooth status** — two-line block: "Bluetooth" label + status text (`BluetoothWaiting`: "Discoverable", `BluetoothActive`: "Connected to \<name\>", unavailable: "Not available")
5. **Wi-Fi status** — two-line block: "Wi-Fi" label + status text ("Connected to \<SSID\>" or "Not connected")
6. **"Wifi Settings" button** — opens the Wi-Fi dialog (§4.D)

### C. Takeover Dialog (modal overlay)

When a second phone connects while one is already playing (`PlaybackController.bluetooth.takeoverPending`):
- Centered modal overlay (~300×200px).
- Title: "Takeover"
- Body: "Keep playing on \<current device\>, or switch to \<new device\>?"
- Two large touch buttons: "Keep Current" / "Switch to \<new\>"
- Countdown: visible "Auto-selecting Keep Current in \<n\>s" below buttons. After 10 seconds, auto-selects Keep Current.
- "Keep Current" → disconnects the new device. "Switch to \<new\>" → disconnects the old device.

### D. Wi-Fi Dialog (popup overlay)

Tapping "Wifi Settings" in the right sidebar opens a dialog:
- Auto-scans on open (calls `WifiController.scan()`). Manual refresh button at top.
- Scrollable SSID list from `WifiController.networks`. Each row: SSID name + signal strength bars (▂▄▆█ mapping: 0–25% → ▂, 25–50% → ▄, 50–75% → ▆, 75–100% → █). Connected SSID gets a checkmark.
- Tap an SSID → password `TextField` appears with OSK (`QtQuick.VirtualKeyboard` auto-pops on focus).
- "Connect" button calls `WifiController.connect(ssid, password)`.
- On success → dialog closes. On error → error message shown in dialog, stays open.

### E. Reboot / Shutdown Confirmation Dialog

Tapping reboot or shutdown in the status bar opens a centered modal:
- Title: "Reboot Radio?" or "Shutdown Radio?"
- Two large touch buttons: "Cancel" (neutral) / "Confirm" (red, `Theme.errorColor`)
- 10-second auto-dismiss (cancels) to prevent accidental activation.
- Confirm calls `systemctl poweroff` or `systemctl reboot` via `QProcess` or `org.freedesktop.login1` D-Bus.

### F. Mute Invariant (ADR 0006)

The app does NOT auto-pause spotifyd when a phone connects via BT — but the **mute invariant** in [ADR 0006](./docs/adr/0006-source-clients-and-best-effort-avrcp-controls.md) guarantees a BT stream is muted while in a Spotify state (re-asserted on every connect). Audio overlap is otherwise managed by the explicit toggle (mute/pause the outgoing source). A BT disconnect while in a Spotify state does nothing; a BT disconnect while in `BluetoothActive` triggers a Bluetooth subtree re-query: if another device is still connected, retarget the AVRCP controls at it and stay `BluetoothActive`; otherwise transition to `BluetoothWaiting`. Never re-query the Spotify bus on a BT disconnect, and never auto-reconnect a dropped connection — source switching (and reconnection) is explicit-only.

## 5. Design Guidelines

- **Touch targets:** minimum 48×48 dp (`Theme.touchTarget`); large transport controls 64×64 (`Theme.touchTargetLarge`).
- **Theme:** Material, dark by default (`Theme.darkMode` toggle). All colors/sizes/fonts flow through the `Theme.qml` singleton — no hex values scattered in views.
- **OSK:** `QtQuick.VirtualKeyboard` pops up automatically when text fields (Wi-Fi password) are focused.
- **View transitions:** QML `States` and `Transitions` for smooth switches.

## 6. Project Layout

```txt
.
├── AGENTS.md                # this file — working instructions
├── CONTEXT.md               # domain glossary (terms only, no implementation)
├── docs/adr/                # architectural decisions + rationale
├── flake.nix                # devShell (x86_64) + package (x86_64, aarch64)
├── CMakeLists.txt           # Qt6 + qt_add_qml_module, C++20
├── src/
│   ├── main.cpp             # QML engine, defaults QT_QPA_PLATFORM=wayland
│   └── controllers/         # C++ QObject controllers owning D-Bus state
│       ├── PlaybackController.{h,cpp}   # facade: playbackState, source switching, transport routing
│       ├── SpotifyClient.{h,cpp}        # MPRIS2 (spotifyd), source state + transport
│       ├── BluetoothClient.{h,cpp}      # BlueZ: Device1 + MediaPlayer1 AVRCP, takeover, mute
│       ├── WifiController.{h,cpp}       # NetworkManager
│       ├── VolumeController.{h,cpp}    # wpctl (source-independent sink volume)
│       └── ArtCache.{h,cpp}             # album art cache + cleanup
├── qml/
│   ├── Main.qml             # root window, frameless fullscreen, three-column layout
│   ├── Theme.qml            # singleton: colors, sizes, fonts, Material theme
│   ├── StatusBar.qml        # persistent top bar (clock, source toggle, reboot/shutdown)
│   ├── RightSidebar.qml     # brightness, volume, dark mode, BT/Wi-Fi status, Wifi Settings btn
│   ├── LeftColumn.qml       # metadata display (state-dependent: track/artist/album/error/hint)
│   ├── CenterColumn.qml     # album art + scrubber/progress + transport
│   ├── WifiDialog.qml       # popup: SSID list, signal bars, password OSK, connect
│   ├── TakeoverDialog.qml   # modal: keep current / switch to new, 10s auto-select
│   └── ConfirmDialog.qml    # modal: reboot/shutdown confirmation, 10s auto-dismiss
├── tests/
│   ├── tst_controllers.cpp  # C++ controller unit tests (Qt Test)
│   ├── tst_qml.cpp          # QML test harness (Qt Quick Test)
│   └── tst_Theme.qml        # QML singleton tests
└── scripts/                 # dev convenience (assume you're in `nix develop`)
```

## 7. Build & Run

All scripts assume you have first entered the Nix devShell: `nix develop` (or `direnv allow` if using the existing `.envrc`).

| Script | Purpose |
| --- | --- |
| `scripts/setup.sh` | One-time CMake configure into `build/` (Debug). Rerun after `CMakeLists.txt` changes. |
| `scripts/build.sh` | Incremental build via Ninja into `build/bin/bierkistnRadio`. Fast iteration. |
| `scripts/run.sh` | Run the local `build/bin/bierkistnRadio`, auto-picking `wayland` or `xcb` based on `WAYLAND_DISPLAY`. |
| `scripts/clean.sh` | Remove the `build/` directory. |
| `scripts/nix-build.sh` | Full reproducible Nix build → `result/bin/bierkistnRadio` (x86_64). Use for a clean verification. |
| `scripts/nix-build-pi.sh` | Cross-build the `aarch64-linux` package for the Pi. |
| `scripts/test.sh` | Build and run all tests via CTest (`tst_controllers` + `tst_qml`). |

For day-to-day iteration: `nix develop` → `scripts/setup.sh` (once) → `scripts/build.sh` → `scripts/run.sh`.

For a clean release check: `scripts/nix-build.sh` → `result/bin/bierkistnRadio`.

For deploying to the Pi: the **system repo** inputs this flake and references `packages.aarch64-linux.bierkistnRadio`; you do not deploy from here directly.

## 8. Testing

The project has two test layers, both wired into CTest:

- **C++ controller tests** (`tst_controllers`): Qt Test unit tests exercising controller defaults, property changes, and clamping logic. Pure C++ — no D-Bus, no QML.
- **QML view tests** (`tst_qml`): Qt Quick Test cases in `tests/tst_*.qml`. Run offscreen (`QT_QPA_PLATFORM=offscreen`). Import the `BierKistnRadio` module to test singletons and view behavior.

The core library (`bierkistn_core`) — controllers + QML module — is a static lib linked by both the app and the tests, so tests see the exact same types as the app.

For D-Bus integration tests (when controllers get D-Bus wiring): run a private `dbus-daemon --session` on an isolated address, register mock MPRIS2/NM/BlueZ services on it, and have controllers connect via `QDBusConnection::connectToBus()` rather than hardcoding `sessionBus()`.

Run tests via `nix develop --command bash -c 'scripts/test.sh'` — the script already runs the build itself, so **do not** run `scripts/build.sh` first. (Running `nix develop --command bash -c 'ctest --test-dir build --output-on-failure'` directly is fine for re-running without touching the build.)

## 9. Controller Conventions

`PlaybackController` is the **only QML singleton in the playback domain** and acts as a **facade**: it owns `playbackState`, source switching, and transport routing, and exposes its source clients as `Q_PROPERTY` children (`PlaybackController.spotify`, `PlaybackController.bluetooth`) for QML to bind data on. `SpotifyClient` (MPRIS2) and `BluetoothClient` (BlueZ) are plain parented `QObject`s — **not** QML singletons — and are reached only through the facade. `WifiController` and `VolumeController` remain independent QML singletons (Settings + volume slider; volume is source-independent, see [ADR 0006](./docs/adr/0006-source-clients-and-best-effort-avrcp-controls.md)).

Each controller/client owns one D-Bus concern, subscribes to signals, and republishes state via `Q_PROPERTY` (with `NOTIFY`) and `Q_INVOKABLE` methods. QML never calls D-Bus directly. Transport calls (play/pause/next/seek) route through `PlaybackController` to the active source; non-source concerns (takeover, Wi-Fi, volume) are reached directly but never D-Bus-called from QML. Keep controllers unit-testable in C++ — no QML dependency in the controller layer.

**C++ code style:** braces are **required** for `if`/`for`/`while`/`do` blocks — always use `{ }` even for single-statement bodies. Never write brace-less single-statement bodies (e.g. `if (x) return;`).

## 10. Online Resources

Spotifyd documentation: <https://docs.spotifyd.rs/Introduction.html>
