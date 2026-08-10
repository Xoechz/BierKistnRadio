# AGENTS.md: BierKistn Radio UI

Working instructions for any agent (human or AI) touching this repository. For domain terminology, see [CONTEXT.md](./CONTEXT.md). For architectural decisions and their rationale, see [docs/adr/](./docs/adr/).

---

## 1. Scope & Repository Boundary

This repository produces **only the Qt6/QML touch-screen application**. It does **not** contain the NixOS system configuration.

The separate **NixOS system repository** (consumes this repo as a flake input) owns:

- The `cage` Wayland compositor kiosk setup that launches this app
- `mopidy` (+ `mopidy-spotify`, `mopidy-mpris`, internet-radio extensions) as the media backend
- `NetworkManager`, `BlueZ`, `PipeWire`/`wireplumber` daemons
- `polkit` + a `soteria` agent + a polkit rule granting the kiosk user the D-Bus actions this app calls (Wi-Fi connect, Bluetooth pair, etc.)

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
- **Playback abstraction:** MPRIS2 over D-Bus, backed by Mopidy. See [ADR 0001](./docs/adr/0001-mpris2-mopidy-as-playback-abstraction.md).
- **Volume:** `wpctl set-volume @DEFAULT_AUDIO_SINK@ <pct%>` (wireplumber CLI), not a linked C library.
- **System D-Bus interfaces used:** MPRIS2 (`org.mpris.MediaPlayer2.mopidy`), NetworkManager, BlueZ.
- **On-screen keyboard:** `QtQuick.VirtualKeyboard` — **GPLv3/commercial in Qt6**. This app is GPLv3 as a result. See [ADR 0002](./docs/adr/0002-tech-stack.md).

Qt6 modules in nixpkgs live under `kdePackages.*` (e.g. `kdePackages.qtbase`, `kdePackages.qtdeclarative`). `qtquickcontrols2` is bundled into `qtdeclarative` in Qt6 — CMake still uses `find_package(Qt6 COMPONENTS QuickControls2)`.

## 4. UI Layout & Views

Three full-screen views + a persistent status bar. Navigation is a flat `Loader` driven by a `currentView` enum (`nowplaying` | `source` | `settings`); the Wi-Fi password sub-flow uses a `StackView` inside the Settings view.

### A. Status Bar (persistent top strip)

- Height: 48px. Shows clock, Wi-Fi state, Bluetooth state, active Source badge.
- Tapping Wi-Fi or Bluetooth opens the Settings view at the relevant section.

### B. Now-Playing (default view)

- Album/Station art, Track metadata (title bold large, artist, album — elide/marquee on overflow).
- Scrubbable progress slider (current timestamp + duration) — **disabled for Stations** (running clock, not a scrubber) and **hidden in Sink Mode**.
- Transport: Play/Pause, Skip Forward, Skip Back — **inactive in Sink Mode** (the Paired Device owns transport).
- Volume slider (0–150%).
- **Sink Mode variant:** shows "Controlled by <Paired Device>", hides scrubber + transport. The speaker only renders PipeWire-routed audio; it does not own playback.

### C. Source Selection

- Tile grid of three Sources: Spotify (Mopidy context), Internet Radio (Mopidy context), Bluetooth sink (PipeWire routing — not an MPRIS source).

### D. Settings

- **Wi-Fi module:** SSID list with signal strength; tap to open OSK for password; calls NetworkManager over D-Bus.
- **Bluetooth module:** paired/discoverable device list; Connect/Disconnect/Pair actions; Discoverable toggle.
- **System controls:** brightness slider; Reboot / Power Off triggers.

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
│   └── controllers/         # C++ QObject singletons owning D-Bus state
│       ├── PlaybackController.{h,cpp}   # MPRIS2
│       ├── WifiController.{h,cpp}       # NetworkManager
│       ├── BluetoothController.{h,cpp}  # BlueZ
│       ├── VolumeController.{h,cpp}    # wpctl
│       └── ArtCache.{h,cpp}             # album/station art cache + cleanup
├── qml/
│   ├── Main.qml             # root window, frameless fullscreen, nav
│   ├── Theme.qml            # singleton: colors, sizes, fonts, Material theme
│   ├── StatusBar.qml        # persistent top bar
│   └── views/
│       ├── NowPlaying.qml
│       ├── SourceSelection.qml
│       └── Settings.qml
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

Run tests: `scripts/test.sh` (or `ctest --test-dir build --output-on-failure` directly).

## 9. Controller Conventions

Every C++ controller is a `QML_SINGLETON` + `QML_NAMED_ELEMENT`, exposed to QML by its class name. It owns one D-Bus concern, subscribes to signals, and republishes state via `Q_PROPERTY` (with `NOTIFY`) and `Q_INVOKABLE` methods. QML never calls D-Bus directly. Keep controllers unit-testable in C++ — no QML dependency in the controller layer.
