# Qt6/CMake/C++ with D-Bus controller bridge

The UI is a **Qt 6 / QML** application built with **CMake** (`qt_add_qml_module`), written in **C++20** on the backend. C++ owns the system integration: thin `QObject` controllers (`PlaybackController`, `WifiController`, `BluetoothController`, `VolumeController`) each subscribe to their D-Bus interface (MPRIS2, NetworkManager, BlueZ) and republish state as QML-facing `Q_PROPERTY`/`Q_INVOKABLE` members. QML stays declarative — no D-Bus calls in QML. Volume goes through `wpctl` (wireplumber CLI) rather than a linked C library, to keep the build simple. The app is packaged as a flake `packages.<system>.bierkistnRadio` for both `x86_64-linux` (dev) and `aarch64-linux` (deploy), consumed as a flake input by the separate NixOS system repo.

## Considered Options

- **Backend language** — C++/Qt6 (chosen) vs Python/PySide6. Python is faster to prototype but carries a second toolchain, the GIL, and interpreter boot on a Pi; C++ with `QtDBus` is first-class and lighter on the target.
- **D-Bus bridge** — C++ `QObject` controllers (chosen) vs QML calling `QDBus` directly vs a separate sidecar process. The controller shape keeps D-Bus glue unit-testable in C++ and QML declarative.
- **Volume path** — `wpctl` shell-out (chosen) vs `libpipewire`/`libwireplumber` link vs PulseAudio-compat D-Bus. `wpctl` is one line, reliable under wireplumber, and avoids a C library dependency; revisit only if measurable cost appears (unlikely for user-initiated changes).
- **Build system** — CMake (chosen) vs Qbs (deprecated) vs Meson/xmake (fragile Qt6-QML support). CMake is Qt6's official system and the only path to `qt_add_qml_module`.

## Consequences

- **GPLv3 applies to this application.** `QtQuick.VirtualKeyboard` is GPLv3/commercial in Qt6 (not LGPL like Qt5). Using it without a commercial Qt license makes the whole app GPLv3. This is acceptable for a personal project, but the constraint is recorded here so no future contributor assumes otherwise.
- The UI is a **thin D-Bus client**: it assumes the system environment (separate NixOS repo) grants the kiosk user the NetworkManager/BlueZ/polkit actions it calls. Controllers must surface D-Bus auth errors as a visible "Permission denied — check system config" state, never a silent failure.
- **Bluetooth-sink playback is not an MPRIS source.** When the speaker is a Bluetooth sink, transport/scrub are inactive and the Now-Playing view shows "Controlled by <Paired Device>" — the phone owns playback; the speaker only renders PipeWire-routed audio.
- Adding a future Mopidy-backed source (e.g. podcasts) is a Mopidy extension, not a new UI state machine — the MPRIS2 abstraction absorbs it.
