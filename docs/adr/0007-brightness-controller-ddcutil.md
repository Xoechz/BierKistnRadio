# Brightness controller (in-app `ddcutil`)

The Right Sidebar's brightness slider becomes real: a dedicated `BrightnessController` QML singleton drives the HDMI panel's backlight via DDC/CI (`ddcutil`), declared as a thin in-app `QProcess` client — no new system-repo service.

## Context

The Right Sidebar brightness slider (T11) is currently decorative — pure QML local state with no backend. [SYSTEM_INTERFACE.md](../SYSTEM_INTERFACE.md) §9/§14.2 left the mechanism open: the Pi's 7" panel is believed to be **HDMI-driven**, and the docs assumed no `/sys/class/backlight` node would exist.

Facts that bounded the decision:

- `/sys/class/backlight/*` only exists for panels a kernel driver controls — RPi **DSI** panels (e.g. the official 7" touchscreen), not HDMI outputs.
- There is **no Wayland/wlroots brightness protocol**. Only **gamma ramps** exist (e.g. `wlr-gamma-control`), which change *perceived* brightness via color correction — no panel backlight saving, compositor-dependent, they don't dim the backlight.
- External monitors do backlight via **DDC/CI** over I2C: `ddcutil getvcp 10` / `setvcp 10 <0-100>`. Requires the `i2c-dev` module, `/dev/i2c-*`, and group permissions. ddcutil supports the Raspberry Pi but it is a known-fiddly path (kernel-DT/`i2c-dev` loading, monitor must actually implement MCCS).

## Decision

1. **New `BrightnessController` QML singleton**, one-concern-per controller, mirroring `VolumeController`'s shape: `Q_PROPERTY(int brightness)` (1–100, default `70`), `Q_PROPERTY(bool available)` (`false` until a first successful read), `Q_PROPERTY(QString errorMessage)`; `Q_INVOKABLE setBrightness(int)`. An injectable `CommandRunner` seam (`QStringList args` + `onFinished`, same shape as `VolumeController`) keeps it unit-testable without hardware.
2. **Transport**: `QProcess` shell-outs to `ddcutil --display N getvcp 10` / `setvcp 10 <p>`. `N` defaults to `1`; overridable via env `BLK_BRIGHTNESS_DISPLAY`. Single-panel kiosk; no `ddcutil detect` scan at startup.
3. **Scale normalization**: brightness is always exposed in percent (1–100, claim 0 = "dim but visible"; screen *off* is DPMS's job, not the slider). Read: `brightness% = round(value/max·100)`, max taken from the same `getvcp` response. Write: `round(pct/100·max)`. Parser accepts both the human output (`current value = X, max value = Y`) and `--terse`.
4. **Freshness**: probe on startup, then poll every 5 s. Each call is guarded by a **3 s `QProcess` watchdog** (kill on timeout — a hung panel must not leak a zombie or hold a stale state). A write-gen counter invalidates any in-flight read result issued before the write (no read-back race).
5. **Failure/backoff**: N consecutive read failures ⇒ `available=false`, keep the last-known `brightness`, stop the fast poll, retry on a backoff timer (5 s → 10 s → 30 s cap, success resets). Write failures (setvcp non-zero or timeout) **revert to the last-good value** and set `errorMessage` — the touchscreen must never display a lying value.
6. **Write policy**: writes happen **only** from the QML slider's `onMoved` (release), clamped to [1,100], and **never at boot** (startup only reads; nothing is written to the panel during initialization).
7. **No persistence**: no QSettings; the slider shows default `70` until the first successful poll replaces it with the panel's real value.
8. **UI**: the Right Sidebar slider binds `enabled: BrightnessController.available`; while unavailable it stays visible at the last-known/default value with an inline "brightness unavailable" hint (no per-poll error spam — one surfaced state).

## Considered Options

- **System-repo D-Bus brightness service** (`org.bierkistn.kiosk.Brightness`) — rejected. Introducing a custom IPC service for a one-command CLI call adds serialization and an ordering dependency for zero app-side benefit; the established repo pattern for CLI-driven hardware is exactly `QProcess` via a seam (`wpctl` volume).
- **`/sys/class/backlight` write** — rejected/dead end for HDMI (no node; DSI-only). Kept as a possible fallback target if the physical panel ever turns out to be DSI.
- **Software gamma scaling (wl-gammarelay/gammastep)** — rejected. Perceived brightness, not panel backlight; no power/stamina benefit; compositor-specific and it would distort colors in a dark-room radio.
- **Fold brightness into `VolumeController`** — rejected. Different hardware path (audio sink vs panel), different poll cost and cadence; one concern per controller.
- **`ddcutil detect` at startup + best-match selection** — rejected for cost (I2C bus scan, parsing identity output) and single-display kiosk reality; env override handles a test rig with two monitors.

## Consequences

- **SYSTEM_INTERFACE.md §9 / §14.2 are no longer "open"**: the app owns brightness; the system repo's only obligations are `ddcutil` on the kiosk `PATH`, `/dev/i2c-*` access for the kiosk user (udev `i2c` group membership), and a one-time `ddcutil detect` verification that the panel really answers DDC/CI — if not, the controller reports the visible "brightness unavailable" state by design (T21 policy) and the panel isn't dimmable, which is a hardware fact, not an app bug.
- **No new daemons** in the image; no polkit action needed (no D-Bus involved).
- **Testing** (extends `tst_controllers.cpp`): parse (value/max → %, `--terse` + human forms, max != 100), clamp edges (0→1, 100+→100), write-gen guard, watchdog-on-timeout, and backoff/availability transitions via a fake runner.
- Tracked in [TODO.md](../TODO.md) as **T26**, which implements ADR 0007.