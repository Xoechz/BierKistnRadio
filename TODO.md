# TODO

Ordered work items for the BierKistn Radio UI. Each item is scoped to be one focused session.

## Phase 1: D-Bus controller wiring

- [ ] **T1: PlaybackController — MPRIS2 D-Bus connection.** Connect to `org.mpris.MediaPlayer2.mopidy` on the session bus. Subscribe to `PropertiesChanged` on `org.mpris.MediaPlayer2.Player`. Wire `title`, `artist`, `album`, `artUrl`, `position`, `duration`, `isPlaying` from D-Bus property changes. Implement `play()`, `pause()`, `next()`, `previous()`, `seek()` as D-Bus method calls. Add a `QDBusServiceWatcher` to detect when Mopidy appears/vanishes.
  - Learn: `QDBusConnection::sessionBus()`, `QDBusInterface`, `QDBusAbstractInterface::call()`, `QDBusConnection::connect()` for signals, `QDBusArgument` for extracting metadata from `Metadata` a{sv} map.

- [ ] **T2: PlaybackController — Station detection.** Mopidy radio tracks have a `mpris:artUrl` but no `xesam:album`. Derive `isStation` from the track URI scheme (e.g. `tunein:`, `somafm:`, `radio:`) or the absence of a duration. Stations show a running clock instead of a scrubber — add a `QTimer` that increments `position` every second when `isStation && isPlaying`.
  - Learn: `QVariantMap` iteration, URI scheme parsing, `QTimer::timeout` signal.

- [ ] **T3: PlaybackController — Sink Mode detection.** When a Bluetooth A2DP source connects (BlueZ emits `org.bluez.MediaTransport1`), set `isSinkMode = true` and `pairedDeviceName` from the BlueZ device name. When it disconnects, `isSinkMode = false`. This is a BlueZ signal subscription, not MPRIS.
  - Learn: BlueZ D-Bus tree (`org.bluez.Media1`, `org.bluez.MediaTransport1`), `QDBusObjectManager` for tracking object additions/removals.

- [ ] **T4: VolumeController — read real volume.** Currently `setVolume` shells out to `wpctl set-volume` but `volume` is a hardcoded 50. Run `wpctl get-volume @DEFAULT_AUDIO_SINK@` at startup and parse the output. Subscribe to volume changes via `wpctl status` polling (1s timer) or `pw-cli` events. Round-trip: `setVolume` should not trigger a read-back race.
  - Learn: `QProcess::start()` vs `QProcess::execute()`, parsing stdout, `QTimer` polling, avoiding feedback loops.

- [ ] **T5: WifiController — NetworkManager scan + connect.** Call `GetDevices` on `org.freedesktop.NetworkManager` to find the Wi-Fi device. Call `RequestScan` on the Wi-Fi device's `Wireless` interface. Subscribe to `AccessPointAdded`/`AccessPointRemoved` signals. For connect: call `AddAndConnectConnection` on `Settings` interface with a connection dict (SSID, password, security). Populate `networks` as a list of SSIDs + signal strengths.
  - Learn: NetworkManager D-Bus API, `QDBusArgument` deserialization of `a{sv}` and `ao` arrays, connection profile dicts.

- [ ] **T6: WifiController — connection state tracking.** Subscribe to `PropertiesChanged` on `org.freedesktop.NetworkManager` for `ActiveConnections` and `PrimaryConnection`. Update `connected`, `ssid`, `signalStrength` from the active connection's access point. Handle auth failure (D-Bus error) → surface as a visible state, not a silent failure.
  - Learn: `QDBusConnection::connect()` for `org.freedesktop.DBus.Properties.PropertiesChanged`, `QDBusReply<T>` error handling.

- [ ] **T7: BluetoothController — BlueZ device discovery + pairing.** Use `QDBusObjectManager` on `org.bluez` to track device objects. Call `StartDiscovery`/`StopDiscovery` on `org.bluez.Adapter1`. For pairing: call `Pair` on `org.bluez.Device1`. For connect: call `Connect` on `org.bluez.Device1` (or `ConnectAudio` for A2DP). Populate `devices` list with name, address, paired/connected state.
  - Learn: `QDBusObjectManagerClient`, `InterfacesAdded`/`InterfacesRemoved` signals, `org.bluez.Device1` properties.

- [ ] **T8: BluetoothController — Discoverable toggle.** Call `Set` on `org.freedesktop.DBus.Properties` for `Discoverable` on `org.bluez.Adapter1`. Subscribe to `PropertiesChanged` to keep the toggle in sync. Handle the case where the adapter is powered off (power it on first).
  - Learn: `org.freedesktop.DBus.Properties.Set`, adapter power state, `Powered` property.

## Phase 2: QML views — real data + interactions

- [ ] **T9: NowPlaying — progress slider + time labels.** Bind the slider to `PlaybackController.position`/`duration`. Show `mm:ss` format for current and total. For Stations, replace the slider with a running clock label. Add `onMoved` → `PlaybackController.seek(value)`.
  - Learn: `Slider` bindings, `Qt.formatTime` / custom `mm:ss` formatter, `enabled` binding.

- [ ] **T10: NowPlaying — marquee for long text.** If `title` or `artist` overflows the label width, scroll it horizontally. Use a `NumberAnimation` on `x` triggered by `elide: Text.ElideNone` + width check.
  - Learn: `TextMetrics` for measuring text width, `NumberAnimation`, `PauseAnimation`, state-driven animations.

- [ ] **T11: NowPlaying — album art with ArtCache.** When `artUrl` changes, call `ArtCache.cacheArt(url, key)`. Show a placeholder while loading. Handle `file://` (local Mopidy art) and `http://` (remote radio logos) differently. `asynchronous: true` on the `Image`.
  - Learn: `Image.status` (`Image.Loading`, `Image.Ready`, `Image.Error`), `QUrl` handling, placeholder states.

- [ ] **T12: SourceSelection — source switching.** Tapping a tile sets the active source. For Spotify/Radio: call `PlaybackController` to switch Mopidy tracklist (or just show the current playback — Mopidy handles the source internally). For Bluetooth: call `BluetoothController.setDiscoverable(true)` to accept incoming audio. Highlight the active tile.
  - Learn: `MouseArea.onClick`, state highlighting, `BluetoothController` invocation from QML.

- [ ] **T13: Settings — Wi-Fi list with signal icons.** Show `WifiController.networks` as a list. Each row: SSID name, signal strength icon (4 bars based on strength), lock icon for secured networks. Tap → opens password `StackView` page. Connected SSID gets a checkmark.
  - Learn: `ListView` + `delegate`, `Repeater` with model, signal strength → icon mapping.

- [ ] **T14: Settings — Wi-Fi password entry with OSK.** `StackView` push with a `TextField` for password. `QtQuick.VirtualKeyboard` auto-pops up on focus. "Connect" button calls `WifiController.connect(ssid, password)`. Handle failure (wrong password) → red border + error text.
  - Learn: `StackView.push()`, `TextField.focus`, `QtQuick.VirtualKeyboard` behavior, `Qt.inputMethod` .

- [ ] **T15: Settings — Bluetooth device list.** Show `BluetoothController.devices` as a list. Each row: device name, address, paired/connected badges. Buttons: Pair, Connect, Disconnect. Discoverable `Switch` at the top.
  - Learn: `ListView` delegate with conditional buttons, `Switch` binding, `Q_INVOKABLE` calls from QML.

- [ ] **T16: Settings — brightness slider + power controls.** Brightness: write to `/sys/class/backlight/.../brightness` (or use a D-Bus backlight interface if available on the Pi). Power Off / Reboot: call `systemctl poweroff`/`reboot` via `QProcess` or `login1` D-Bus. Add a confirmation dialog before power actions.
  - Learn: file I/O for backlight (or `org.freedesktop.login1` D-Bus), `Dialog` / `Popup` for confirmation.

## Phase 3: Polish + deployment

- [ ] **T17: View transitions.** Add `Transition`s between `currentView` states in `Main.qml` — slide/fade the `Loader` content. Keep it subtle (200ms) for a touch device.
  - Learn: `Transition`, `NumberAnimation`, `PropertyAnimation`, `State` changes.

- [ ] **T18: Theme — light mode pass.** Verify all views look good in light mode. Fix any hardcoded colors that bypass `Theme`. Add a settings toggle for dark/light mode.
  - Learn: `Theme.qml` singleton usage, `Material.theme` binding, catching color leaks.

- [ ] **T19: D-Bus error handling.** Every controller must catch D-Bus errors and expose a `Q_PROPERTY` error state (e.g. `permissionDenied`, `serviceUnavailable`). QML shows a "Permission denied — check system config" or "Mopidy not running" banner. Never silent failure.
  - Learn: `QDBusError`, `QDBusReply<T>::isValid()`, error propagation to QML.

- [ ] **T20: Controller tests — real coverage.** Expand `tst_controllers.cpp` to test property changes after method calls, signal emissions (`QSignalSpy`), clamping edges, and error states. For D-Bus integration tests: start a private `dbus-daemon --session`, register mock services, connect controllers to it.
  - Learn: `QSignalSpy`, `QVERIFY(signalSpy.count() == 1)`, `dbus-daemon --session --print-address`, `QDBusConnection::connectToBus()`.

- [ ] **T21: Cross-build for the Pi.** Run `scripts/nix-build-pi.sh`. Fix any aarch64-specific issues (e.g. missing cross-compiled Qt plugins). Verify the binary runs on the Pi under cage.
  - Learn: Nix cross-compilation, `pkgsCross.aarch64-multiplatform`, Qt platform plugins for Wayland on aarch64.

- [ ] **T22: System repo handoff.** Create the separate NixOS system repo that inputs this flake, runs cage, configures Mopidy + NetworkManager + BlueZ + PipeWire + polkit/soteria, and launches `bierkistnRadio` as the kiosk app. This is where the polkit rule granting the kiosk user D-Bus actions lives.
  - Learn: NixOS modules, `services.cage`, `services.mopidy`, `security.polkit.extraConfig`, `security.soteria.enable`.
