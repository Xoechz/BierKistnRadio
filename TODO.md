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

- [ ] **T5: WifiController — NetworkManager scan + connect.** Call `GetDevices` on `org.freedesktop.NetworkManager` to find the Wi-Fi device. Call `RequestScan` on the Wi-Fi device's `Wireless` interface. Subscribe to `AccessPointAdded`/`AccessPointRemoved` signals. For connect: call `AddAndConnectConnection` on `Settings` interface with a connection dict (SSID, password, security). Populate `networks` as a list of SSIDs + signal strengths.
  - Learn: NetworkManager D-Bus API, `QDBusArgument` deserialization of `a{sv}` and `ao` arrays, connection profile dicts.

- [ ] **T6: WifiController — connection state tracking.** Subscribe to `PropertiesChanged` on `org.freedesktop.NetworkManager` for `ActiveConnections` and `PrimaryConnection`. Update `connected`, `ssid`, `signalStrength` from the active connection's access point. Handle auth failure (D-Bus error) → surface as a visible state, not a silent failure.
  - Learn: `QDBusConnection::connect()` for `org.freedesktop.DBus.Properties.PropertiesChanged`, `QDBusReply<T>` error handling.

- [ ] **T7: BluetoothClient — BlueZ tracking + MediaPlayer1 AVRCP** (renamed from `BluetoothController`; part of the [ADR 0006](./docs/adr/0006-source-clients-and-best-effort-avrcp-controls.md) facade split). Bluetooth is phone-driven (see [ADR 0004](./docs/adr/0004-phone-driven-bluetooth-connection-model.md)): the app does NOT initiate pairing, discovery, or connection. Use `QDBusObjectManager` on `org.bluez` to watch `Device1` objects; subscribe to `PropertiesChanged` for `Connected`/`Name`/`Alias`. Expose the current connected device and support kicking it. Implement `ensureDiscoverable()` → `Adapter1.Set(Discoverable, true)` (re-asserted on entering `BluetoothWaiting` because BlueZ drops it on connect). Implement **takeover**: when a second device connects while one is active, signal QML to show the takeover dialog. Send `Device1.Disconnect()` to kick. Track `MediaPlayer1` objects (per-device `playerN`) for best-effort AVRCP: expose `Status`/`Track`/`Position` properties and `play()/pause()/next()/previous()` — controls target the active device. Implement `setMuted(bool)`: discover the connected device's bluez audio PipeWire node via `pw-dump`/`pw-cli` **by matching `api.bluez5.address` == the connected MAC** (the node name shape is device/profile-dependent — e.g. `bluez_input.<MAC>.2` or `bluez_output.<MAC>.a2dp-sink` — never hardcode `bluez_output.*.a2dp-sink`), then `wpctl set-mute <id>` (never `@DEFAULT_AUDIO_SINK@`).
  - Learn: `QDBusObjectManagerClient`, `InterfacesAdded`/`InterfacesRemoved` signals, `org.bluez.Device1` properties, `org.bluez.Adapter1` adapter properties, `Device1.Disconnect()`, `MediaPlayer1` PropertiesChanged, `wpctl set-mute` + `pw-dump` node discovery.

- [ ] **T8: BluetoothClient — takeover confirmation flow + adapter state.** The takeover dialog is state-only: "Keep <current> or switch to <new>?", default keep after 10 s (timer in QML). Implementing "keep current" → disconnect the new `Device1`; "switch" → disconnect the old. The adapter's base `Powered`/`Discoverable`/`Pairable` state is owned by the NixOS config — the app re-asserts `Discoverable=true` only when entering `BluetoothWaiting` (see [ADR 0004](./docs/adr/0004-phone-driven-bluetooth-connection-model.md)), and otherwise observes the adapter, not building a discoverable toggle.
  - Learn: `QDBusPendingCallWatcher` for async `Disconnect`, `QTimer`/`QMetaObject` for the dialog timeout, adapter property observation.

## Phase 2: QML views — real data + interactions

- [ ] **T9: StatusBar — source toggle.** The Source badge in the Status Bar is a tap target. Tapping it switches between Spotify and Bluetooth:
  - To Bluetooth: calls `PlaybackController.switchToBluetooth()` — pauses spotifyd, unmutes BT stream, enters `BluetoothWaiting` (no device) or `BluetoothActive` (device connected). Shows "…" until `playbackState` changes.
  - To Spotify: calls `PlaybackController.switchToSpotify()` — mutes BT stream (if active, AVRCP `Pause` best-effort), then re-queries for the Spotify state. Shows "…" until `playbackState` changes.
  - Note: a phone connecting via BT does NOT automatically switch to Bluetooth Mode — only the user tapping the toggle does. (While in a Spotify state, a connecting BT stream IS muted — the ADR 0006 mute invariant.) Show a subtle "muted" badge on the BT indicator while the BT stream is muted.
  - Learn: `MouseArea.onClick`, state bindings, `Q_INVOKABLE` calls from QML, `Connections` for async state transitions.

- [ ] **T10: NowPlaying — five-state view switching.** Drive the Now-Playing view from `PlaybackController.playbackState`:
  - `SpotifyUnavailable`: error message "Spotify service not running — check system config". Volume slider shown (Q3/ADR 0006 — volume is universal).
  - `SpotifyWaiting`: hint text "Open Spotify on your phone — choose this speaker". Hide volume slider, scrubber, transport.
  - `SpotifyActive`: album art, metadata, scrubber, transport (driven by `isSpotifyPlaying`), volume slider.
  - `BluetoothWaiting`: "Discoverable — connect your phone". Hide scrubber, transport, volume.
  - `BluetoothActive`: best-effort AVRCP state (see [ADR 0006](./docs/adr/0006-source-clients-and-best-effort-avrcp-controls.md)) — metadata sub-layout if `Track` published ("via <Device>"), else "No Metadata available"; transport shown while `Status` published; passive no-thumb progress bar while `Position` publishes; volume slider shown.
  - Learn: conditional layouts in QML, `states` and `transitions` for view mode switching, binding to `Q_ENUM` from QML.

- [ ] **T11: NowPlaying — progress slider + time labels.** Bind the slider to `PlaybackController.position`/`duration`. Show `mm:ss` format for current and total. Slider (with thumb) is only shown in `SpotifyActive` state. In `BluetoothActive`, show a passive non-interactive progress bar (no thumb) only when `Position` is publishing; no seek. Volume slider shows in all states.
  - Learn: `Slider` bindings, `Qt.formatTime` / custom `mm:ss` formatter, `visible` binding to enum state.

- [ ] **T12: NowPlaying — marquee for long text.** If `title` or `artist` overflows the label width, scroll it horizontally. Use a `NumberAnimation` on `x` triggered by `elide: Text.ElideNone` + width check.
  - Learn: `TextMetrics` for measuring text width, `NumberAnimation`, `PauseAnimation`, state-driven animations.

- [ ] **T13: NowPlaying — album art with ArtCache.** When `artUrl` changes, call `ArtCache.cacheArt(url, key)`. Show a placeholder while loading. spotifyd's `mpris:artUrl` is typically a remote `https://` URL (Spotify CDN). `asynchronous: true` on the `Image`.
  - Learn: `Image.status` (`Image.Loading`, `Image.Ready`, `Image.Error`), `QUrl` handling, placeholder states.

- [ ] **T14: Settings — Wi-Fi list with signal icons.** Show `WifiController.networks` as a list. Each row: SSID name, signal strength icon (4 bars based on strength), lock icon for secured networks. Tap → opens password `StackView` page. Connected SSID gets a checkmark.
  - Learn: `ListView` + `delegate`, `Repeater` with model, signal strength → icon mapping.

- [ ] **T15: Settings — Wi-Fi password entry with OSK.** `StackView` push with a `TextField` for password. `QtQuick.VirtualKeyboard` auto-pops up on focus. "Connect" button calls `WifiController.connect(ssid, password)`. Handle failure (wrong password) → red border + error text.
  - Learn: `StackView.push()`, `TextField.focus`, `QtQuick.VirtualKeyboard` behavior, `Qt.inputMethod`.

- [ ] **T16: Settings — Bluetooth state + takeover.** Show current connected device (state-only, per [ADR 0004](./docs/adr/0004-phone-driven-bluetooth-connection-model.md)) and the takeover confirmation dialog. No device list, no Pair/Connect/Disconnect rows, no Discoverable toggle.
  - Learn: `ListView`/`Label` for state display, `Dialog` / `Popup` + `Timer` for the takeover confirm with 10 s keep-default.

- [ ] **T17: Settings — brightness slider + power controls.** Brightness: write to `/sys/class/backlight/.../brightness` (or use a D-Bus backlight interface if available on the Pi). Power Off / Reboot: call `systemctl poweroff`/`reboot` via `QProcess` or `login1` D-Bus. Add a confirmation dialog before power actions.
  - Learn: file I/O for backlight (or `org.freedesktop.login1` D-Bus), `Dialog` / `Popup` for confirmation.

## Phase 3: Polish + deployment

- [ ] **T18: View transitions.** Add `Transition`s between `currentView` states in `Main.qml` — slide/fade the `Loader` content. Keep it subtle (200ms) for a touch device.
  - Learn: `Transition`, `NumberAnimation`, `PropertyAnimation`, `State` changes.

- [ ] **T19: Theme — light mode pass.** Verify all views look good in light mode. Fix any hardcoded colors that bypass `Theme`. Add a settings toggle for dark/light mode.
  - Learn: `Theme.qml` singleton usage, `Material.theme` binding, catching color leaks.

- [ ] **T20: D-Bus error handling.** Every controller must catch D-Bus errors and expose a `Q_PROPERTY` error state (e.g. `permissionDenied`, `serviceUnavailable`). QML shows a "Permission denied — check system config" or "spotifyd not running" banner (the `SpotifyUnavailable` state in `PlaybackController` is the first instance of this). Never silent failure.
  - Learn: `QDBusError`, `QDBusReply<T>::isValid()`, error propagation to QML, `Q_ENUM` error states.

- [ ] **T21: Controller tests — real coverage.** Expand `tst_controllers.cpp` to test property changes after method calls, signal emissions (`QSignalSpy`), clamping edges, and error states. For D-Bus integration tests: start a private `dbus-daemon --session`, register mock spotifyd/MPRIS2/NM/BlueZ services, connect controllers to it.
  - Learn: `QSignalSpy`, `QVERIFY(signalSpy.count() == 1)`, `dbus-daemon --session --print-address`, `QDBusConnection::connectToBus()`.

- [ ] **T22: Cross-build for the Pi.** Run `scripts/nix-build-pi.sh`. Fix any aarch64-specific issues (e.g. missing cross-compiled Qt plugins). Verify the binary runs on the Pi under cage.
  - Learn: Nix cross-compilation, `pkgsCross.aarch64-multiplatform`, Qt platform plugins for Wayland on aarch64.

- [ ] **T23: System repo handoff.** Create the separate NixOS system repo that inputs this flake, runs cage, configures spotifyd (with `use_mpris = true`) + NetworkManager + BlueZ + PipeWire + polkit/soteria, and launches `bierkistnRadio` as the kiosk app. This is where the polkit rule granting the kiosk user D-Bus actions lives. See [SYSTEM_INTERFACE.md](./SYSTEM_INTERFACE.md) for the full contract.
  - Learn: NixOS modules, `services.cage`, `services.spotifyd` (or systemd user service), `security.polkit.extraConfig`, `security.soteria.enable`, D-Bus session bus setup for kiosk user.

## Phase 2 (continued): Now-Playing — release date metadata

- [ ] **T24: Release date — MusicBrainz lookup.** For an active Track, use the title + first artist to query the MusicBrainz Recording API for the track's **first release date**. Query `GET /ws/2/recording/?query=recording:"<title>"+AND+artist:"<artist>"&fmt=json` (see the [MusicBrainz API docs](https://musicbrainz.org/doc/MusicBrainz_API) and the example `https://musicbrainz.org/ws/2/recording/?query=recording:%22Exodus%22%20AND%20artist:%22Brand%20of%20Sacrifice%22&fmt=json`). Parse the first hit's earliest `first-release-date` (derived from the releases' `date` fields), respecting MB's 1 req/s rate limit (`User-Agent` + `polite pool` contract) and caching per track. Surface the resolved date as a `Q_PROPERTY releaseDate` on `PlaybackController` (or a dedicated non-singleton music-metadata client reached through the facade, mirroring the `SpotifyClient`/`BluetoothClient` pattern) so QML can show it in the Now-Playing metadata block.
  - **Failure/edge handling (must never crash or block):** API unreachable → show **"No Connection to Musicbrainz for release dates"**; track not found or ambiguous (0 or >1 hits) → show **"Release Date unknown"**. Any valid single hit → show its first release date. Keep the query async (`QNetworkAccessManager`, don't block the UI thread), and fire it whenever the current artist/title changes (or on `SpotifyWaiting`. Only trigger when a Track is actually loaded).
  - Learn: `QNetworkAccessManager`/`QNetworkReply`, MusicBrainz JSON `fmt=json` schema, polite-pool `User-Agent` conventions + `~1 req/s` rate limiting, `QUrlQuery` encoding, an LRU `release-date` cache keyed by `artist|title`, mapping the "unknown"/"no connection" states to distinct user-facing strings.
