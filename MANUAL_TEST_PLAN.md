# BierKistn Radio — manual Pi acceptance test plan

Run this on the **physical Pi under cage at 1024×600** after deploying compatible app and NyxOS revisions. Allow about 45–60 minutes, including a four-minute discoverability check. Automated mock/offscreen tests and the AArch64 package build do not replace these checks.

Record **PASS / FAIL / BLOCKED**, observations, and evidence for each case. A phone feature that BlueZ does not expose should be recorded explicitly rather than reported as an app metadata defect.

**Automated baseline:** all five CTest targets pass, including Material-style QML geometry/input tests, private-bus lifecycle/pairing/NetworkManager tests, and a real hung-process timeout test. `scripts/nix-build-pi.sh` builds an AArch64 executable; its packaged QML starts in an isolated offscreen emulated run. The cases below remain pending until performed on the Pi.

## Preparation

- Pi, touchscreen, working audio output, and internet access.
- Phone A with Spotify; Phone B for takeover. Record phone models, OS versions, and music apps.
- A secured test Wi-Fi network or phone hotspot whose password you know. Keep local console access for connectivity/recovery tests.
- Deploy app code containing the loading overlay and Wi-Fi popup fix. NyxOS consumes a pinned `bierkistn-radio` input: publish/update that input or use your local-source deployment workflow so you do not accidentally test an older app. Installing only the app's build output does not apply the system configuration.
- The system changes are in `~/NyxOS/modules/bierkistn.nix` and `modules/hosts/piKistn.nix`. The system-side checklist is `~/NyxOS/resources/bierkistnVerification.md`.

On the development machine, record both revisions and any uncommitted changes:

```sh
git -C ~/Repos/BierKistnRadio rev-parse HEAD
git -C ~/Repos/BierKistnRadio status --short
git -C ~/NyxOS rev-parse HEAD
git -C ~/NyxOS status --short
```

On the Pi, run the commands below **as `kistn`**. For an SSH/console shell, connect to the existing kiosk user's bus, not an unrelated user's or a new private bus:

```sh
id -un
export XDG_RUNTIME_DIR="/run/user/$(id -u)"
export DBUS_SESSION_BUS_ADDRESS="unix:path=$XDG_RUNTIME_DIR/bus"
nixos-version
readlink -f /run/current-system
systemctl show cage-tty1.service -p ExecStart -p Environment
busctl --system tree org.bluez
```

Set `ADAPTER` to the adapter path shown by the last command. The example below is the usual Pi path; replace it if necessary:

```sh
ADAPTER=/org/bluez/hci0
```

For ordered source-transition evidence, open a second Pi terminal before tapping the source toggle:

```sh
busctl --user monitor org.freedesktop.systemd1
```

And another terminal if available:

```sh
busctl --system monitor org.bluez
```

Stop monitors with Ctrl+C. They should show shutdown state **before** the opposite backend's startup request. Do not invoke `Device1.Connect` or `MediaPlayer1.Play` merely to inspect state.

## M01: Package, kiosk, and boot

1. Reboot the Pi and leave both phones disconnected initially.
2. Confirm one full-screen UI appears at 1024×600, with the clock, three columns, and controls reachable by touch. No duplicate/stretched screen, QML load error, missing-plugin message, or desktop window decoration.
3. Confirm the startup loader disappears when Spotify's user service is ready. No phone or loaded track is needed; expect **“Open Spotify on your phone — choose this speaker”**, with fallback artwork.
4. Run:

   ```sh
   systemctl --user show spotifyd.service -p ActiveState -p Restart -p RestartUSec -p After -p TimeoutStartUSec -p TimeoutStopUSec
   systemctl --user status pipewire.service wireplumber.service --no-pager
   systemctl status cage-tty1.service bluetooth.service NetworkManager.service --no-pager
   busctl --system get-property org.bluez "$ADAPTER" org.bluez.Adapter1 Powered
   loginctl list-sessions
   ```

5. Expect spotifyd, PipeWire, WirePlumber, BlueZ, and NetworkManager active; the adapter property is `b false`. Spotify timeouts are three seconds, restart policy is `on-failure`, and `After` does not contain `default.target`. Check the cage `kistn` session holds the active seat using `loginctl show-session SESSION_ID -p Name -p Active -p Type`.
6. On Phone A, confirm the Pi is available in Spotify Connect without touching the source toggle.

**Pass:** the ARM package runs under Wayland/cage, Spotify is available at boot, and Bluetooth is off. Covers T23/T24/T29.

## M02: Wi-Fi settings

1. Tap **Wifi Settings**. Expect a visible, centered dialog above the dimmed background, not a grey screen with no dialog.
2. Check nearby SSIDs, signal bars, and the connected-network checkmark. Tap Refresh; no duplicated rows or repeated prompts.
3. Tap Cancel. Confirm dimming disappears and the underlying volume/theme/source controls work. Repeat open/close three times, including in light mode.
4. Select the secured test network. Expect the password field to receive focus and the on-screen keyboard to appear above the popup. The field, Connect, and Cancel remain visible/reachable. Try a long SSID and a longer error message if available.
5. Enter an incorrect password and tap Connect. The dialog remains open while connecting, then displays an error or bounded connection timeout. It must not close merely because the activation call was accepted.
6. Retry with the correct password. Only after the selected SSID actually connects should the popup and keyboard close; the sidebar updates to that SSID.
7. Reopen the dialog, select a secured network, then Cancel while the keyboard is open. Both keyboard and dimming disappear. If a hardware keyboard is available, verify Escape also dismisses the dialog.
8. Toggle the test hotspot off/on and refresh. Known and newly scanned networks should return. An open network, if available, connects without asking for a password.

Diagnostics if the list is empty or activation fails:

```sh
nmcli device status
nmcli -f IN-USE,SSID,SIGNAL,SECURITY device wifi list --rescan yes
nmcli connection show --active
journalctl -b -u NetworkManager.service --since '-5 minutes' --no-pager
```

**Pass:** visible dialog, accessible keyboard layout, real SSIDs, and a secured connection through the UI. Covers T27/T41/T43.

## M03: Spotify playback and artwork

1. In Spotify mode, select the Pi on Phone A and start a track.
2. Check title, artist, album, artwork, current/total time, and Play/Pause state.
3. Exercise Previous, Next, Play/Pause, and seeking. Tap the cover for Play/Pause; swipe left/right for Next/Previous. A swipe must not also toggle Play/Pause.
4. Skip several tracks quickly. No artwork or release date from a previous track should remain attached to the new track after its lookup resolves. Revisit a cover to check cache reuse.
5. Try a track with a long title, including **“Good Vibrations - Remastered”** if available, and a very long word/device name. Titles wrap or elide within three lines and do not cover Artist/Album.
6. Check release-date lookup: a known unambiguous result shows a date; unknown/ambiguous results show **“Release Date unknown”**. Loss of MusicBrainz connectivity shows **“No Connection to Musicbrainz for release dates”**, without freezing touch input. Network failure is not evidence of an unknown release date.
7. With an uncached artwork request and internet temporarily unavailable, expect fallback artwork and a responsive UI. Restore the network before continuing.

Diagnostics:

```sh
busctl --user list
du -sh "$HOME/.cache/BierKistnRadio/BierKistnRadio/art"
```

Find the dynamic `org.mpris.MediaPlayer2.spotifyd.*` name in the bus list; it is not fixed across restarts. Cache usage should remain bounded to about 100 MB of image data after sufficient distinct covers; do not require a new small cache to reach the cap. Cache pruning and HTTP error cases also have automated coverage.

**Pass:** phone-driven Spotify works, gestures do not double-fire, metadata is current, and artwork/lookup failures do not block the UI.

## M04: Exclusive source switching and loading overlay

1. While Spotify is playing, tap Bluetooth. Expect the entire touch UI to be covered by a translucent grey overlay and centered loading indicator. Taps on volume, power, Wi-Fi, and source controls must not act through it.
   A fast transition may show only a brief flash; use M10's missing-sink case to inspect a sustained loader.
2. Use the bus monitors to confirm spotifyd is observed `inactive`/`failed` **before** `Powered=true` is requested. Spotify Connect disappears; no Spotify/Bluetooth audio overlap.
3. When ready, expect the overlay to disappear without waiting for a phone connection. The screen shows Bluetooth waiting, not indefinite loading.
4. Run:

   ```sh
   systemctl --user show spotifyd.service -p ActiveState
   busctl --system get-property org.bluez "$ADAPTER" org.bluez.Adapter1 Powered
   busctl --system get-property org.bluez "$ADAPTER" org.bluez.Adapter1 UUIDs
   busctl --system get-property org.bluez "$ADAPTER" org.bluez.Adapter1 Discoverable
   ```

5. Expect spotifyd stopped, `Powered=true`, and local A2DP Sink UUID `0000110b-0000-1000-8000-00805f9b34fb`. With no connected phone, verify discoverability still works after **four minutes**.
6. Pair/connect as described in M05, then play Bluetooth audio. Tap Spotify.
7. Confirm `Powered=false` is observed and the phone disconnects **before** `StartUnit("spotifyd.service", "replace")`. Spotify Connect returns, and the overlay clears when the service runs. The app must not automatically issue Play or reconnect the phone.
8. Repeat both directions five times. Reconnection initiated by the phone is separate from the app initiating a connection; source selection must never change merely because metadata or a phone connection appears.

**Pass:** ordered disable-before-enable, correct readiness, bounded modal loading, no automatic Play, and no overlapping sources. Covers T29/T30/T31.

## M05: Protected Bluetooth pairing

Use a previously unpaired Phone A. Remove the existing bond from both phone and Pi if needed; remove only the test phone, not all saved devices. Discover the correct device path first with `busctl --system tree org.bluez`.

1. Select Bluetooth and initiate pairing **from the phone**.
2. Expect a device-named touchscreen prompt with a six-digit, zero-padded code matching the phone. Record whether BlueZ uses `RequestConfirmation`.
3. Tap **Reject**. The phone remains unpaired and the prompt closes.
4. Repeat, but cancel on the phone. The touchscreen prompt closes without accepting anything.
5. Repeat and leave the prompt unanswered. It rejects after **30 seconds**. A late tap cannot accept that expired request.
6. Repeat and tap **Codes Match** only after checking the codes. Pairing succeeds, and the phone can connect/stream.
7. Disconnect and reconnect the already-paired phone in Bluetooth mode. No new pairing prompt is expected. Switching to Spotify powers the radio off and disconnects it.
8. During a fresh pending confirmation, stop the app using the cage stop/restart procedure in M10. The request must fail; a new phone must not be silently paired while the app is unavailable. Restarting the app begins in Spotify mode with Bluetooth off.
9. Check there is no competing auto-accept agent:

   ```sh
   systemctl status bt-agent.service --no-pager
   pgrep -af 'bt-agent|bluetoothctl'
   ```

   An absent/inactive `bt-agent` unit is expected. Close any diagnostic `bluetoothctl` session that registered its own agent. Bus monitoring is preferable for observing the app's `RegisterAgent`, `RequestDefaultAgent`, and `UnregisterAgent` calls.
10. If a phone negotiates Just Works or passkey-entry instead of numeric comparison, expect rejection/cancellation with an error. Record phone model and association flow; the app must not pretend that `DisplayPasskey` notifications are an approval gate.

**Pass:** new pairing requires explicit touchscreen consent; reject/cancel/timeout/exit never silently accept; saved phones reconnect without prompting. Covers T32/T33/T42.

## M06: Bluetooth detection, metadata, and fallback

1. Connect Phone A in Bluetooth mode and play Spotify/audio on the phone. Expect the correct connected device name; switching to Bluetooth itself must not start playback.
2. If BlueZ publishes a remote player, check title, artist, album, status, duration, and position against the phone. The progress bar is passive and cannot seek. AVRCP buttons are shown only while status is available.
3. Try Play/Pause, Previous, and Next. Unsupported AVRCP features may be unavailable, but permission/operation errors must be visible.
4. Disconnect the phone. Remain in Bluetooth waiting, with no stale track/transport data or automatic switch to Spotify.
5. If metadata is missing, record the actual bus boundary:

   ```sh
   busctl --system tree org.bluez
   ```

   If a `playerN` object exists, set `PLAYER` to its actual path and read:

   ```sh
   busctl --system call org.bluez "$PLAYER" org.freedesktop.DBus.Properties GetAll s org.bluez.MediaPlayer1
   ```

6. When Track is not published, expect **“Controlled by <device>” / “No metadata available”**, fallback art, and only the controls/progress actually supported by the phone. Do not invent metadata based on the app playing on the phone.

**Pass:** connection detection is reliable; published fields are represented correctly; absent fields have a clean fallback. Record whether missing metadata is at the phone/BlueZ or app boundary. Covers T28.

## M07: Two-phone takeover

1. Play Bluetooth audio from Phone A. Pair/connect Phone B from the second phone.
2. Expect the Takeover dialog naming both phones. Tap **Keep Current**; B disconnects and A continues.
3. Reconnect B and choose **Switch to B**; A disconnects, B becomes the active device, and metadata/controls target B.
4. Repeat and do nothing. After **10 seconds**, Keep Current is selected.
5. Disconnect B while the dialog is pending. The stale prompt closes cleanly. Test the other phone disconnecting too; remaining connections must stay in Bluetooth mode.
6. If a disconnect fails, the dialog must remain pending with an error and retry; its countdown must not keep retrying the failed disconnect automatically.

**Pass:** only the chosen phone is audible and resolution follows observed disconnection. Automated tests cover injected permission failures and disconnect timeouts.

## M08: Volume, mute, and external changes

1. Set the shared volume to a moderate nonzero level. Check the percentage label and physical loudness in Spotify and Bluetooth modes.
2. Tap Mute: volume becomes **0%**. Tap Unmute: the previous nonzero level returns.
3. Mute again, then raise the slider. The button immediately represents unmuted state.
4. Verify external polling from the `kistn` shell:

   ```sh
   wpctl set-volume @DEFAULT_AUDIO_SINK@ 25%
   wpctl get-volume @DEFAULT_AUDIO_SINK@
   wpctl set-volume @DEFAULT_AUDIO_SINK@ 0%
   ```

   The UI follows each external value within about one second. Unmute should restore 25%. A volume knob, if attached, should behave equivalently.
5. Verify the 100% tick and 0–150% slider range. Check higher values visually without needing loud playback. On a fresh app with no known nonzero level, unmute falls back to 10%.

**Pass:** slider, percentage, mute state, and external changes agree; muting volume does not change source selection.

## M09: Physical layout and both themes

1. Repeat the key views in dark and light mode: Spotify waiting/active, Bluetooth waiting/active/fallback, Wi-Fi/password/keyboard, pairing, takeover, loading, and power confirmation.
2. Check Previous/Play/Pause/Next SVG icons are centered and legible, with 64×64 transport buttons.
3. Check long titles and device names wrap/elide within their allotted lines, without clipping or overlapping Artist/Album or controls. Check the real panel, not only a screenshot scaled on another screen.
4. Tap all important controls at their edges as well as centers. Wi-Fi Connect/Cancel and transport controls remain reachable when the keyboard, error text, or metadata is present.

**Pass:** no overflow, hidden buttons, theme contrast problems, or font-dependent transport alignment. Covers T34/T38 and physical acceptance of T43.

## M10: Failure, timeout, and recovery

Run these controlled tests with local access. Restore the changed service/unit state after each case.

### A. Failed Spotify startup and explicit retry

1. Select Bluetooth and wait until ready.
2. In the `kistn` shell:

   ```sh
   systemctl --user mask --runtime spotifyd.service
   ```

3. Tap Spotify. The app powers Bluetooth off, then shows an actionable startup error. The loader disappears, Spotify stays selected, and Retry is available. No repeated StartUnit calls while you wait 15 seconds.
4. Restore and retry:

   ```sh
   systemctl --user unmask --runtime spotifyd.service
   systemctl --user daemon-reload
   ```

5. Tap Retry. Spotify becomes ready and the error clears without requiring MPRIS/phone playback.

If the runtime mask cannot be installed due to an existing runtime unit file, record the case as BLOCKED rather than deleting unit files; the automated private-bus test covers startup rejection.

### B. Missing sink and ten-second timeout

1. Select Spotify, then stop the user session's Bluetooth endpoint provider:

   ```sh
   systemctl --user stop wireplumber.service
   ```

2. Tap Bluetooth. Inspect Adapter1.UUIDs. If the A2DP Sink UUID is absent, the loader stays while readiness is pending, disappears after at most 10 seconds, and leaves Bluetooth selected with an error/Retry. If the UUID remains published on this system, this setup did not create a missing-sink failure; record the observation.
3. Restore the provider:

   ```sh
   systemctl --user start wireplumber.service
   ```

4. When the powered adapter's sink UUID returns, an already-issued startup may recover passively and clear the error without another power/start command. If incoming startup was never issued because outgoing shutdown failed/timed out, only Retry should advance it.

### C. Intentional stop and crash policy

1. In Bluetooth mode, wait at least 15 seconds; spotifyd remains stopped.
2. In Spotify mode, simulate a daemon crash:

   ```sh
   systemctl --user kill --signal=SIGKILL spotifyd.service
   ```

3. Expect a visible source-unavailable/error state followed by passive recovery when systemd's configured crash restart runs after about 12 seconds. The app must not create a rapid restart loop.

### D. Daemon/app restart and stale requests

1. While in Bluetooth mode, optionally restart BlueZ locally:

   ```sh
   sudo systemctl restart bluetooth.service
   ```

2. Pending pairing/takeover state clears, stale metadata disappears, and source selection does not automatically change. Retry Bluetooth if its powered-off adapter needs a fresh explicit startup.
3. Stop/start the kiosk, including once during pairing:

   ```sh
   sudo systemctl stop cage-tty1.service
   sudo systemctl start cage-tty1.service
   ```

   Leave a gap between these commands for M05's app-unavailable pairing check. Restart begins in Spotify mode. No previous pairing request can be accepted by a new prompt.
4. If testing NetworkManager restart, do it from the local console, then restore/open Wi-Fi settings. Cached SSIDs/connection state clear and scanning recovers without duplicate rows or success signals.

**Pass:** visible actionable failures, bounded loading, retained requested source, explicit retry where needed, and late recovery without repeated failed commands. Permission-denial injection has automated coverage; any real permission failure must also be visible, not a silent failure or frozen overlay. Covers T21/T24/T29/T31/T42.

## M11: Reboot and shutdown

1. Tap Reboot, then Cancel. The Pi stays running. Repeat and leave the dialog unanswered: it cancels after 10 seconds.
2. Confirm Reboot. The Pi reboots and returns to M01's Spotify boot state.
3. Repeat cancel/timeout for Shutdown, then confirm shutdown at the end of testing. The Pi powers down.
4. Test while an SSH session is also open, so the configured multi-session power permissions are exercised. A denied/failed operation should leave a visible error and allow retry rather than silently dismissing.

## Evidence and sign-off

For failures, record case ID, exact steps, elapsed time, selected source, phone model/app, screenshot/photo, and whether the relevant D-Bus properties were present. Capture relevant logs before rebooting:

```sh
journalctl -b -u cage-tty1.service --since '-10 minutes' --no-pager
journalctl --user -b -u spotifyd.service -u pipewire.service -u wireplumber.service --since '-10 minutes' --no-pager
journalctl -b -u bluetooth.service -u NetworkManager.service --since '-10 minutes' --no-pager
```

Keep Wi-Fi passwords and other secrets out of shared logs/results.

| Case | Result | Notes / evidence |
| --- | --- | --- |
| M01 Package/kiosk/boot | Pending | |
| M02 Wi-Fi popup/keyboard/activation | Pending | |
| M03 Spotify/metadata/artwork/gestures | Pending | |
| M04 Exclusive switching/loading | Pending | |
| M05 Pairing protection | Pending | |
| M06 Bluetooth metadata/fallback | Pending | |
| M07 Takeover | Pending | |
| M08 Volume/external changes | Pending | |
| M09 Themes/physical layout | Pending | |
| M10 Failure/recovery | Pending | |
| M11 Power | Pending | |

Update `SYSTEM_INTERFACE.md` with the deployed app/system revisions and outcomes. Mark the remaining TODOs only after their required evidence exists: **T23 → M01; T24 → M01/M02/M03/M04/M05/M08/M10/M11; T27 → M02; T28 → M06; T29 → M01/M04/M10; T32/T42 → M05/M10; T34/T38 → M09.** T31/T43 implementations have automated coverage; their physical acceptance is M04/M02/M09.
