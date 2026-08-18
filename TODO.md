# TODO

Ordered work items for the BierKistn Radio UI. Each item is scoped to be one focused session.

## Phase 1: D-Bus controller wiring

- [x] **T0: Facade split (ADR 0006) — `PlaybackController` → facade + source clients.** `PlaybackController` becomes the only playback QML singleton and a **facade**: it owns `playbackState`, source switching (`switchToBluetooth`/`switchToSpotify`), and routes transport (`play`/`pause`/`next`/`previous`/`seek`) to the active source. MPRIS2 code moves out into a new **`SpotifyClient`** (title/artist/album/artUrl/position/duration/isSpotifyPlaying + transport + `hasTrack`/`isAvailable` signals). `BluetoothController` is renamed to **`BluetoothClient`**, becomes a parented non-singleton reached via `PlaybackController.bluetooth`, and gains the ADR 0006 surface — best-effort AVRCP (`Status`/`Track`/`Position`/`duration`, publish flags, `play`/`pause`/`next`/`previous`), the mute (`setMuted(bool)`), takeover, and `ensureDiscoverable()`. The facade exposes the clients as `Q_PROPERTY` children (`PlaybackController.spotify`, `PlaybackController.bluetooth`) so QML binds data on the owner and routes calls to the router. `WifiController`/`VolumeController`/`ArtCache` unchanged.
  - Learn: facade pattern, `Q_PROPERTY(QObject*)` child exposure from a singleton, moving responsibilities across controllers while keeping QML bindings stable.

- [x] **T1: PlaybackController — spotifyd discovery + MPRIS2 connection.** Discover spotifyd's D-Bus name dynamically (list names, match `org.mpris.MediaPlayer2.spotifyd.*` — the `$PID` suffix changes on restart). Add a `QDBusServiceWatcher` to detect when the name appears/vanishes. Once the MPRIS2 name is present, subscribe to `PropertiesChanged` on `org.mpris.MediaPlayer2.Player` and wire `title`, `artist`, `album`, `artUrl`, `position`, `duration`, `isSpotifyPlaying` from D-Bus property changes. Implement `play()`, `pause()`, `next()`, `previous()`, `seek()` as D-Bus method calls. Drive `playbackState` transitions from MPRIS2 presence and content: `SpotifyUnavailable` (no MPRIS2 name) → `SpotifyWaiting` (name present, no track loaded) → `SpotifyActive` (track loaded).
  - Learn: `QDBusConnection::sessionBus()`, `QDBusConnectionInterface::registeredServiceNames()`, `QDBusServiceWatcher`, `QDBusInterface`, `QDBusArgument` for extracting metadata from `Metadata` a{sv} map, `Q_ENUM` state machines.

- [x] **T2: Delete `rs.spotifyd.Controls` + TransferPlayback (phone-driven Spotify).** Spotify is initiated from the phone, like Bluetooth — see [ADR 0005](./docs/adr/0005-drop-rs-spotifyd-controls.md). Removed the `Controls` name tracking, `transferPlayback()`, and the `SpotifyReady` state. Renamed to `SpotifyWaiting`, detected by content (no track loaded) rather than name absence, because MPRIS2 is present as soon as spotifyd is connected. `switchToSpotify()` re-queries the bus instead of transfering playback.
  - Learn: custom D-Bus interfaces can be the wrong abstraction; verify name-presence assumptions against actual daemon behavior (the `SpotifyReady` window didn't exist in practice).

- [x] **T3: PlaybackController — Bluetooth Mode (opt-in) + A2DP detection.** Bluetooth Mode is NOT triggered automatically by a BT A2DP source connecting. It is only entered via `switchToBluetooth()`:
  - `switchToBluetooth()`: pauses spotifyd (MPRIS2 `Pause`), unmutes the BT stream (`BluetoothClient.setMuted(false)`), then checks if an A2DP transport exists. If yes → `BluetoothActive` with `pairedDeviceName` from BlueZ. If no → calls `BluetoothClient.ensureDiscoverable()`, sets `BluetoothWaiting`. `ensureDiscoverable()` re-asserts `Adapter1.Set(Discoverable, true)` because BlueZ drops it on connect — see [ADR 0004](./docs/adr/0004-phone-driven-bluetooth-connection-model.md).
  - When a phone connects during `BluetoothWaiting`: transition to `BluetoothActive`.
  - On BT disconnect while in `BluetoothActive`: query the **Bluetooth** state (not the Spotify bus) — if another device is still connected, stay `BluetoothActive` and retarget AVRCP at it; otherwise transition to `BluetoothWaiting`. Do NOT attempt to reconnect a dropped connection, and do NOT auto-switch to a Spotify state (source switching is explicit-only).
  - On BT disconnect while in a Spotify state: do nothing.
  - `switchToSpotify()`: if in `BluetoothActive`, **mute** the BT stream first (`BluetoothClient.setMuted(true)`), send a best-effort AVRCP `Pause`, then re-query the bus for the appropriate Spotify state (ADR 0006: mute-before-pause, phone is the truth).
  - Mute invariant (ADR 0006): any BT A2DP stream appearing while in a Spotify state is muted, re-asserted on every connect.
  - Learn: BlueZ D-Bus tree (`org.bluez.Media1`, `org.bluez.MediaTransport1`), `QDBusObjectManager` for tracking object additions/removals, opt-in state transitions, BT subtree re-query on disconnect to find remaining active connections, node-discovery via `pw-cli` before `wpctl set-mute`.

- [x] **T4: VolumeController — read real volume.** Currently `setVolume` shells out to `wpctl set-volume` but `volume` is a hardcoded 50. Run `wpctl get-volume @DEFAULT_AUDIO_SINK@` at startup and parse the output. Subscribe to volume changes via `wpctl status` polling (1s timer) or `pw-cli` events. Round-trip: `setVolume` should not trigger a read-back race.
  - Learn: `QProcess::start()` vs `QProcess::execute()`, parsing stdout, `QTimer` polling, avoiding feedback loops.

- [x] **T5: WifiController — NetworkManager scan + connect.** Call `GetDevices` on `org.freedesktop.NetworkManager` to find the Wi-Fi device. Call `RequestScan` on the Wi-Fi device's `Wireless` interface. Subscribe to `AccessPointAdded`/`AccessPointRemoved` signals. For connect: call `AddAndConnectConnection` on `Settings` interface with a connection dict (SSID, password, security). Populate `networks` as a list of SSIDs + signal strengths.
  - Learn: NetworkManager D-Bus API, `QDBusArgument` deserialization of `a{sv}` and `ao` arrays, connection profile dicts.

- [x] **T6: WifiController — connection state tracking.** Subscribe to `PropertiesChanged` on `org.freedesktop.NetworkManager` for `ActiveConnections` and `PrimaryConnection`. Update `connected`, `ssid`, `signalStrength` from the active connection's access point. Handle auth failure (D-Bus error) → surface as a visible state, not a silent failure.
  - Learn: `QDBusConnection::connect()` for `org.freedesktop.DBus.Properties.PropertiesChanged`, `QDBusReply<T>` error handling.

- [x] **T7: BluetoothClient — BlueZ tracking + MediaPlayer1 AVRCP** (renamed from `BluetoothController`; part of the [ADR 0006](./docs/adr/0006-source-clients-and-best-effort-avrcp-controls.md) facade split). Bluetooth is phone-driven (see [ADR 0004](./docs/adr/0004-phone-driven-bluetooth-connection-model.md)): the app does NOT initiate pairing, discovery, or connection. Use `QDBusObjectManager` on `org.bluez` to watch `Device1` objects; subscribe to `PropertiesChanged` for `Connected`/`Name`/`Alias`. Expose the current connected device and support kicking it. Implement `ensureDiscoverable()` → `Adapter1.Set(Discoverable, true)` (re-asserted on entering `BluetoothWaiting` because BlueZ drops it on connect). Implement **takeover**: when a second device connects while one is active, signal QML to show the takeover dialog. Send `Device1.Disconnect()` to kick. Track `MediaPlayer1` objects (per-device `playerN`) for best-effort AVRCP: expose `Status`/`Track`/`Position` properties and `play()/pause()/next()/previous()` — controls target the active device. Implement `setMuted(bool)`: discover the connected device's bluez audio PipeWire node via `pw-dump`/`pw-cli` **by matching `api.bluez5.address` == the connected MAC** (the node name shape is device/profile-dependent — e.g. `bluez_input.<MAC>.2` or `bluez_output.<MAC>.a2dp-sink` — never hardcode `bluez_output.*.a2dp-sink`), then `wpctl set-mute <id>` (never `@DEFAULT_AUDIO_SINK@`).
  - Learn: `QDBusObjectManagerClient`, `InterfacesAdded`/`InterfacesRemoved` signals, `org.bluez.Device1` properties, `org.bluez.Adapter1` adapter properties, `Device1.Disconnect()`, `MediaPlayer1` PropertiesChanged, `wpctl set-mute` + `pw-dump` node discovery.

- [x] **T8: BluetoothClient — takeover confirmation flow + adapter state.** The takeover dialog is state-only: "Keep <current> or switch to <new>?", default keep after 10 s (timer in QML). Implementing "keep current" → disconnect the new `Device1`; "switch" → disconnect the old. The adapter's base `Powered`/`Discoverable`/`Pairable` state is owned by the NixOS config — the app re-asserts `Discoverable=true` only when entering `BluetoothWaiting` (see [ADR 0004](./docs/adr/0004-phone-driven-bluetooth-connection-model.md)), and otherwise observes the adapter, not building a discoverable toggle.
  - Learn: `QDBusPendingCallWatcher` for async `Disconnect`, `QTimer`/`QMetaObject` for the dialog timeout, adapter property observation.

## Phase 2: QML views — three-column layout (mockup-driven)

- [x] **T9: Main.qml — three-column skeleton.** Rewrite `Main.qml` to the three-column layout: `StatusBar` on top, then a `RowLayout` with `LeftColumn` (~250px), `CenterColumn` (fill), `RightSidebar` (~250px). Remove the bottom nav bar, the `Loader`, and the `Settings.qml` reference. Remove the `appRouter` QtObject. The three columns are always visible — no view switching. Keep `Theme` singleton integration. Window remains `1024×600`, frameless fullscreen.
  - Learn: `RowLayout`, `Layout.fillWidth`, `Layout.preferredWidth`, removing a `Loader`-based navigation pattern.

- [x] **T10: StatusBar — clock + source toggle + reboot/shutdown.** Rewrite `StatusBar.qml`: clock (left-aligned), source toggle center ("Spotify ○══○ Bluetooth"), reboot + shutdown icon-only buttons (right). Remove the Wi-Fi emoji and Bluetooth emoji — status is now in the right sidebar. Source toggle: tap calls `switchToSpotify()`/`switchToBluetooth()`, shows "…" while switching, 3-second timeout resets stuck "…" if no `playbackState` change arrives. Reboot/shutdown buttons open the `ConfirmDialog`.
  - Learn: `MouseArea`, `Timer` for timeout, wiring to `PlaybackController.switchTo*`.

- [x] **T11: RightSidebar — brightness, volume, dark mode, statuses.** New `RightSidebar.qml`. Top-to-bottom: vertical brightness slider (0–100%, label shows %), vertical volume slider (0–150%, label shows %, tick mark at 100%), dark mode toggle (small switch), Bluetooth status (two-line: "Bluetooth" label + dynamic text), Wi-Fi status (two-line: "Wi-Fi" label + dynamic text), "Wifi Settings" button (opens `WifiDialog`). All always visible, source-independent. Bind `VolumeController.volume`/`setVolume`, `PlaybackController.bluetooth.*`, `WifiController.connected`/`ssid`.
  - Learn: `Slider` (vertical orientation), `Layout` in a `ColumnLayout`, binding to controller properties.

- [x] **T12: LeftColumn — metadata display.** New `LeftColumn.qml`. Content switches on `PlaybackController.playbackState`:
  - `SpotifyUnavailable`: error text "Spotify service not running — check system config"
  - `SpotifyWaiting`: hint text "Open Spotify on your phone — choose this speaker"
  - `SpotifyActive`: Track title (bold, large, word-wrap max 3 lines then elide), Artist, Album, Release Date
  - `BluetoothWaiting`: hint text "Discoverable — connect your phone"
  - `BluetoothActive` + `trackPublished`: Track title (suffixed "via \<device name\>"), Artist, Album
  - `BluetoothActive` + `!trackPublished`: "Controlled by \<device name\>" as title, "No metadata available" as subtitle
  - Learn: QML `states` with `PropertyChanges`, `Text { wrapMode: Text.WordWrap; maximumLineCount: 3; elide: Text.ElideRight }`.

- [x] **T13: CenterColumn — album art + progress + transport.** New `CenterColumn.qml`. Top: 400×400 `Image` with `fillMode: Image.PreserveAspectFit`, `asynchronous: true`. Source: `spotify.artUrl` (SpotifyActive) or fallback SVG (`qrc:/qt/qml/BierKistnRadio/assets/fallback-album.svg`) when empty. Below art: progress element (see T14). Below progress: transport buttons (see T15).
  - Learn: `Image` with `asynchronous`, `fillMode`, `source` binding with ternary on state.

- [x] **T14: CenterColumn — progress scrubber / passive bar.** Inside `CenterColumn`: `SpotifyActive` → interactive `Slider` (0–`duration/1000` seconds), thumb, time labels flanking (`m:ss` current left, `m:ss` total right), `onMoved: PlaybackController.seek(value * 1000)`. `BluetoothActive` + `positionPublished` + `duration > 0` → passive non-interactive bar (no thumb), time labels (`m:ss` current / total). `BluetoothActive` + `positionPublished` + `duration <= 0` → passive bar, no labels. All other states: no progress element.
  - Learn: `Slider` vs `ProgressBar`, `Qt.formatDateTime` or custom `m:ss` formatter, conditional visibility.

- [x] **T15: CenterColumn — transport buttons.** Inside `CenterColumn`, below progress: `SpotifyActive` → Play/Pause + Next + Previous buttons (64×64 touch targets). Play/Pause glyph reflects `isSpotifyPlaying`. `BluetoothActive` + `statusPublished` → same three buttons (best-effort AVRCP). Play/Pause glyph reflects `isBluetoothPlaying`. All other states: no transport. Buttons call `PlaybackController.play()`/`pause()`/`next()`/`previous()`.
  - Learn: `Icon` or unicode glyphs, `MouseArea` / `Button`, conditional `visible` on state.

- [x] **T16: Fallback album art SVG.** Create `qml/assets/fallback-album.svg` — a simple speaker/radio icon suitable for a 400×400 container. Embed as Qt resource in `CMakeLists.txt`.
  - Learn: SVG authoring, `qt_add_qml_module` resource embedding.

- [x] **T17: WifiDialog — SSID list + password + OSK.** New `WifiDialog.qml` (`Popup` or `Dialog`). Auto-scans on open (`WifiController.scan()`). Scrollable `ListView` of `WifiController.networks`: each row shows SSID name + signal strength bars (▂▄▆█: 0–25% → ▂, 25–50% → ▄, 50–75% → ▆, 75–100% → █). Connected SSID gets a checkmark. Tap an SSID → password `TextField` with OSK (`QtQuick.VirtualKeyboard` auto-pops on focus). "Connect" button calls `WifiController.connect(ssid, password)`. On success → dialog closes. On error → error text in dialog, stays open. Manual refresh button at top.
  - Learn: `Popup`/`Dialog`, `ListView` + delegate, `QtQuick.VirtualKeyboard`, binding to `WifiController.errorMessage`.

- [x] **T18: TakeoverDialog — keep/switch modal.** New `TakeoverDialog.qml` (`Popup` centered, modal). Visible when `PlaybackController.bluetooth.takeoverPending`. Title "Takeover", body "Keep playing on \<current\>, or switch to \<new\>?". Two large touch buttons: "Keep Current" / "Switch to \<new\>". Countdown text: "Auto-selecting Keep Current in \<n\>s". 10-second `Timer` → auto-selects KeepCurrent. Calls `PlaybackController.bluetooth.resolveTakeover(KeepCurrent|SwitchToNew)`.
  - Learn: `Popup` with `modal: true`, `Timer` countdown, `Q_ENUM` from `BluetoothClient.TakeoverChoice`.

- [x] **T19: ConfirmDialog — reboot/shutdown.** New `ConfirmDialog.qml` (`Popup` centered, modal). Title changes: "Reboot Radio?" / "Shutdown Radio?". Two large buttons: "Cancel" (neutral) / "Confirm" (red `Theme.errorColor`). 10-second auto-dismiss (cancels). Confirm calls `systemctl poweroff`/`reboot` via `QProcess`.
  - Learn: `Popup` + `Timer` auto-dismiss, `QProcess::start("systemctl", ...)`.

- [x] **T20: Dark mode toggle.** Wire `Theme.darkMode` toggle in the right sidebar (small switch between sliders and BT status). Toggle sets `Theme.darkMode = !Theme.darkMode`, which flips `Material.theme` between Dark and Light. Verify all views re-render correctly.
  - Learn: `Switch`/`Toggle` control, `Material.theme` binding, verifying no hardcoded color leaks.

## Phase 3: Polish + deployment

- [ ] **T21: D-Bus error handling.** Every controller must catch D-Bus errors and expose a `Q_PROPERTY` error state (e.g. `permissionDenied`, `serviceUnavailable`). QML shows a "Permission denied — check system config" or "spotifyd not running" banner (the `SpotifyUnavailable` state in `PlaybackController` is the first instance of this). Never silent failure.
  - Learn: `QDBusError`, `QDBusReply<T>::isValid()`, error propagation to QML, `Q_ENUM` error states.

- [ ] **T22: Controller tests — real coverage.** Expand `tst_controllers.cpp` to test property changes after method calls, signal emissions (`QSignalSpy`), clamping edges, and error states. For D-Bus integration tests: start a private `dbus-daemon --session`, register mock spotifyd/MPRIS2/NM/BlueZ services, connect controllers to it.
  - Learn: `QSignalSpy`, `QVERIFY(signalSpy.count() == 1)`, `dbus-daemon --session --print-address`, `QDBusConnection::connectToBus()`.

- [ ] **T23: Cross-build for the Pi.** Run `scripts/nix-build-pi.sh`. Fix any aarch64-specific issues (e.g. missing cross-compiled Qt plugins). Verify the binary runs on the Pi under cage.
  - Learn: Nix cross-compilation, `pkgsCross.aarch64-multiplatform`, Qt platform plugins for Wayland on aarch64.

- [ ] **T24: System repo handoff.** Create the separate NixOS system repo that inputs this flake, runs cage, configures spotifyd (with `use_mpris = true`) + NetworkManager + BlueZ + PipeWire + polkit/soteria, and launches `bierkistnRadio` as the kiosk app. This is where the polkit rule granting the kiosk user D-Bus actions lives. See [SYSTEM_INTERFACE.md](./SYSTEM_INTERFACE.md) for the full contract.
  - Learn: NixOS modules, `services.cage`, `services.spotifyd` (or systemd user service), `security.polkit.extraConfig`, `security.soteria.enable`, D-Bus session bus setup for kiosk user.

- [ ] **T26: Screen brightness controller.** Implement [ADR 0007](./docs/adr/0007-brightness-controller-ddcutil.md): new `BrightnessController` QML singleton that shells out to `ddcutil --display N getvcp 10` / `setvcp 10 <p>` via an injectable `CommandRunner` seam (mirroring `VolumeController`), driving the RightSidebar brightness slider. `N` defaults to `1` (env `BLK_BRIGHTNESS_DISPLAY` overloads). Percent-normalized scale (1–100, always; clamp 0→1, 100+→100; `% = round(value/max·100)` on read, `round(pct/100·max)` on write). Reads/Writes are per-call **3s QProcess-watchdog-guarded**, with a **write-gen counter** (no read-back race); probe at startup + **5 s poll**; on consecutive read failures stop the poll and retry on backoff 5 s→10 s→30 s (success resets), setting `available=false` and keeping the last-good value. Writes happen **only** from the QML `onMoved`, clamped [1,100], **never at boot**. Write failure reverts to last-good + `errorMessage` (never a lying slider). **No persistence** — default `70` until the first successful read. QML: slope binds `enabled: BrightnessController.available` + inline "brightness unavailable" hint. Extend `tst_controllers.cpp` (parse/clamps/write-gen/watchdog/backoff). Note: SYSTEM_INTERFACE.md §9/§14.2 flipped from "system repo owns a brightness interface" to "app shells ddcutil; system repo provides `ddcutil` + kiosk-user i2c access (udev `i2c` group) + a `ddcutil detect` sanity check".
  - Learn: DDC/CI over `i2c-dev`/`ddcutil` (Pi HDMI support caveats), `getvcp`/`setvcp` output formats (human + `--terse`), `QProcess` per-call watchdogs, write-gen read-back guards, exponential backoff, disable-but-honest availability UI.

## Phase 4: Now-Playing — release date metadata

- [ ] **T25: Release date — MusicBrainz lookup.** For an active Track, use the title + first artist to query the MusicBrainz Recording API for the track's **first release date**. Query `GET /ws/2/recording/?query=recording:"<title>"+AND+artist:"<artist>"&fmt=json` (see the [MusicBrainz API docs](https://musicbrainz.org/doc/MusicBrainz_API) and the example `https://musicbrainz.org/ws/2/recording/?query=recording:%22Exodus%22%20AND%20artist:%22Brand%20of%20Sacrifice%22&fmt=json`). Parse the first hit's earliest `first-release-date` (derived from the releases' `date` fields), respecting MB's 1 req/s rate limit (`User-Agent` + `polite pool` contract) and caching per track. Surface the resolved date as a `Q_PROPERTY releaseDate` on `PlaybackController` (or a dedicated non-singleton music-metadata client reached through the facade, mirroring the `SpotifyClient`/`BluetoothClient` pattern) so QML can show it in the Now-Playing metadata block.
  - **Failure/edge handling (must never crash or block):** API unreachable → show **"No Connection to Musicbrainz for release dates"**; track not found or ambiguous (0 or >1 hits) → show **"Release Date unknown"**. Any valid single hit → show its first release date. Keep the query async (`QNetworkAccessManager`, don't block the UI thread), and fire it whenever the current artist/title changes (or on `SpotifyWaiting`. Only trigger when a Track is actually loaded).
  - Learn: `QNetworkAccessManager`/`QNetworkReply`, MusicBrainz JSON `fmt=json` schema, polite-pool `User-Agent` conventions + `~1 req/s` rate limiting, `QUrlQuery` encoding, an LRU `release-date` cache keyed by `artist|title`, mapping the "unknown"/"no connection" states to distinct user-facing strings.

## Unspecified

- [ ] Fix wifi list
- [ ] Decide how to finalize spotifyd bluetooth split(shutting of the service? allowing overlapping audio and switch just switches through displays(no fixed states then?))
  - Then we would be just a mpris player => fine i guess?
  - Maybe we simplify to play whatever and show the first mpris(bt data we get?)
  - Check why my car shows bt metadata, but the pi does not
- [ ] Pin protected bluetooth
