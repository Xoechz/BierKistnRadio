# Source Clients + best-effort Bluetooth controls (AVRCP)

`PlaybackController` becomes a **facade**: it owns the composed `playbackState` and source switching, and coordinates two **source clients** — `SpotifyClient` (MPRIS2) and `BluetoothClient` (BlueZ) — which own their source's state and transport. Bluetooth gains **best-effort AVRCP transport controls and metadata**, plus a hard **mute invariant** so switching sources never overlaps audio.

## Context

[ADR 0005](./0005-drop-rs-spotifyd-controls.md) made Spotify and Bluetooth symmetric, phone-driven sources. But Bluetooth was deliberately degraded: no transport, no metadata, "Controlled by <Paired Device>" only. Two questions followed:

**Can the speaker offer basic playback controls without violating the phone-driven model?** MPRIS is local D-Bus — it never travels over Bluetooth. The Bluetooth analog is **AVRCP**, and BlueZ already implements it:

- `org.bluez.MediaPlayer1` (object path `.../dev_{BDADDR}/playerN`, non-deprecated): `Play()`, `Pause()`, `Stop()`, `Next()`, `Previous()`, `FastForward()`, `Rewind()`, plus `Press(byte avc_key)` / `Hold()` / `Release()` for raw passthrough. Read-only `Status` (`playing`/`paused`/`stopped`), `Position` (ms, AVRCP 1.6), and a `Track` dict (`Title`/`Artist`/`Album`/`Duration`) — the "MPRIS-like" surface.
- `org.bluez.MediaControl1` — identical idea but every method is marked **[Deprecated]**; superseded by `MediaPlayer1`.

This is exactly how "codec speaker with buttons" products work: the speaker is the AVRCP **Controller** (CT), the phone is the **Target** (TG). Transport commands are best-effort passthrough — **the phone decides** whether to honor them; `Status`/`Track` only arrive if the phone publishes them at AVRCP 1.6 (Spotify/Android yes, generally; flaky elsewhere). There is **no seek-absolute** in `MediaPlayer1` — no scrubbing over BT. No album art (OBEX BIP is experimental).

**How to guarantee no audio overlap on source switch?** `wpctl set-mute ID 1|0` works (verified). But `@DEFAULT_AUDIO_SINK@` is the **physical output** — spotifyd plays into it, so muting it mutes the very source being switched to. The phone's incoming A2DP audio arrives at a **separate** PipeWire node; muting *that node* silences only the phone. The node's name shape is **device/profile-dependent** — on a Samsung phone mainline PipeWire surfaced it as `bluez_input.<MAC>.2` rather than the classic `bluez_output.<MAC>.a2dp-sink` — so the app MUST discover it by `api.bluez5.address` (the connected MAC), not a hardcoded name. `wpctl` addresses by numeric node ID or `@DEFAULT_*` alias — **not node names** — and node IDs are session-scoped, so the app must discover the bluez node's ID via `pw-cli`/`pw-dump` at call time.

## Decision

### Architecture

1. **`PlaybackController` is the QML-facing facade.** It owns the five-state `playbackState`, the Source toggle (`switchToBluetooth`/`switchToSpotify`), and routes transport calls to the active source. It exposes its two source clients as children via `Q_PROPERTY` (`spotify`, `bluetooth` as `QObject*`), so QML binds `PlaybackController.bluetooth.connectedDeviceName` etc. `PlaybackController` is the only singleton construction path; clients are parented `QObject`s, not QML singletons.
2. **`SpotifyClient`** (new) owns everything MPRIS2: name discovery, `PropertiesChanged` subscription, `title`/`artist`/`album`/`artUrl`/`position`/`duration`/`isSpotifyPlaying`, and transport (`play`/`pause`/`next`/`previous`/`seek`). Content-based state detection (per [ADR 0005](./0005-drop-rs-spotifyd-controls.md)) feeds the facade.
3. **`BluetoothClient`** (renamed from `BluetoothController`) owns everything BlueZ: `Device1` tracking (`connectedDeviceName`), `MediaPlayer1` AVRCP (best-effort transport + `Status`/`Track`/`Position`), `ensureDiscoverable()`, the takeover flow, and `setMuted(bool)`. Its `playerN` object is discovered through the same `QDBusObjectManagerClient` as `Device1` (T7), keyed by parent device; controls always target the **active** device (the current-kept one during a takeover dialog).
4. **`WifiController`, `VolumeController`, `ArtCache` are unchanged** and remain standalone QML singletons. Volume and Wi-Fi are never routed through the facade.

### Best-effort Bluetooth controls

5. **Transport shown iff `Status` is published**: `Play()/Pause()/Next()/Previous()` (no Stop, no AVRCP volume, no Repeat/Shuffle/Scan interop). Fire-and-forget — commands are queued and forgotten; **the phone is the truth**. The play/pause glyph reflects the last-published `Status`; if none is ever received, transport buttons are hidden entirely ("Controlled by <Paired Device>" only).
6. **Metadata shown iff `Track` is published**: a `BluetoothActive` sub-layout with title/artist/album (elided) + "via <Device>". Fallback when absent: "No Metadata available". `Status` → transport, `Track` → metadata, **independently** (both are rare but are gated separately).
7. **No scrubber over BT.** `MediaPlayer1` has no seek-absolute. Spotify keeps the scrubbable slider *with* a thumb; BT shows a **passive non-interactive progress bar only** (`[======---]`, no thumb) while `Position` is publishing, disappearing silently when it stops.
8. **No error UI for best-effort.** A phone ignoring `Pause` shows nothing — the mute below is the guarantee, pause is a nicety ("audio got redirected" so the phone UI doesn't show a live stream into a muted speaker).
9. **Volume slider is shown everywhere, always**, and is source-independent (`@DEFAULT_AUDIO_SINK@`). It is orthogonal to (10) — it is not the mute mechanism.

### Mute invariant (audio exclusivity guarantee)

> **Superseded by [ADR 0008](./0008-two-sided-audio-exclusivity.md)** for decisions 10–13. ADR 0008 generalises the muter from "BT-mute when switching to Spotify" into a two-sided rule: the inactive Source is muted **and paused**, either direction, and re-asserted whenever one of its streams appears. `setMuted` now mutes all (true) / unmutes active (false).

10. **Switching to Spotify from `BluetoothActive`**: mute the bluez sink node first (hard silence), then send best-effort AVRCP `Pause` (fire-and-forget), then re-query the bus for the Spotify state. Mute-before-pause so no frame relies on phone compliance.
11. **Mute lives in `BluetoothClient.setMuted(bool)`**, not `VolumeController`: it is a property of the *BT stream's* node. Mechanism: `pw-dump`/`pw-cli ls Node` to discover the connected device's bluez audio node — **match by `api.bluez5.address` == the connected MAC**, never by a hardcoded `bluez_output.*.a2dp-sink` name (the shape is device/profile-dependent) — then `wpctl set-mute <id>`. Node discovery happens at call time (IDs are session-scoped).
12. **Re-assertion invariant (like `Discoverable` in [ADR 0004](./0004-phone-driven-bluetooth-connection-model.md))**: mute state lives on the *node*, and BlueZ destroys the node on disconnect. So **any time a BT A2DP stream appears while `playbackState` is a Spotify state, mute it** — a newly connected phone gets re-muted. This supersedes "a BT connect while in a Spotify state does nothing" for the *audio* path (connection state still does nothing; only the stream is silenced).
13. **Unmute only on `switchToBluetooth()`.** No proactive `Play` (auto-resume is the phone's business; a phone that paused itself stays paused). A subtle "muted" badge on the BT indicator shows while muted.
14. **Takeover is unchanged** (phone-driven, 10 s keep-current dialog, [ADR 0004](./0004-phone-driven-bluetooth-connection-model.md) §5) and is reached through the facade (`PlaybackController.bluetooth.resolveTakeover(...)`) but **not** re-routed by state — routing applies only to source transport.

## Considered Options

- **Keep all MPRIS code in `PlaybackController`** — rejected. Two unrelated D-Bus shapes (MPRIS2 vs BlueZ) in one class; 284+ lines and growing; violates one-concern-per-controller (AGENTS §9). The facade split is the same QML surface with two focused clients.
- **Name the clients `*Service`** (C#-inspired) — rejected. "Service" already means *external daemons* throughout this repo ("spotify service not running", "system services", mock MPRIS2/NM/BlueZ services); `SpotifyService` would read as spotifyd itself. "Source" is already the glossary term for the *user-visible input*, and "BluetoothSource" means *the phone* in BT terminology (CONTEXT: *Sink*). "Client" matches the repo's own self-description — "this app is a thin D-Bus client".
- **Mute `@DEFAULT_AUDIO_SINK@`** — rejected. That's the physical output spotifyd shares; muting it silences the target source. The bluez sink node is the correct, isolated target.
- **Use `org.bluez.MediaControl1` / AVRCP volume** — rejected. Every method is deprecated, and volume stays a local, source-independent `wpctl` concern.
- **Show a scrubber over BT** — rejected. `MediaPlayer1` has no seek-absolute; a non-movable "scrubber" on a kiosk reads as broken UI. Passive bar only.

## Consequences

- **`BluetoothController` → `BluetoothClient`** (file rename, no `QML_NAMED_ELEMENT`, reachable only via facade). MPRIS code moves out of `PlaybackController` into the new `SpotifyClient`. `PlaybackController.{h,cpp}` shrinks to facade + routed `play`/`pause`/`next`/`previous`/`seek` + `switchToBluetooth`/`switchToSpotify`.
- **"UI is state-only" in [ADR 0004](./0004-phone-driven-bluetooth-connection-model.md) §2 is now scoped to connection management** (Settings module): no device list, no pair/connect/disconnect rows. Best-effort *playback* controls are a separate, opt-in layer that does not reverse phone-driven *connection* — the phone still finds, pairs, connects, and initiates playback. ADR 0004 is amended accordingly, not rewritten.
- **[ADR 0005](./0005-drop-rs-spotifyd-controls.md)'s symmetry claim is amended**: `SpotifyActive` is no longer the *only* state with metadata + transport — Bluetooth now offers *best-effort, degraded* equivalents. MPRIS2 remains richer (reliable metadata, scrub, art, seek); AVRCP is transport + best-effort metadata.
- **Audio exclusivity is now coordinated** by the toggle via mute (decision 10–13). This supersedes AGENTS/SYSTEM_INTERFACE's "the user manages overlap manually" — the toggle guarantees silence on the outgoing source without disconnecting it (the phone's stream is kept "captured", inaudible).
- **New runtime tooling**: `pw-cli`/`pw-dump` (ships with `pipewire`, already required) for node discovery. No new daemons, no `pactl`.
- **QML surface**: only `PlaybackController` is a C++ singleton; all source data binds through `PlaybackController.spotify.*` / `PlaybackController.bluetooth.*`. `VolumeController`/`WifiController` stay direct singletons for the slider and Settings wifi module.
- **`MediaPlayer1` is discovered by T7's `QDBusObjectManagerClient`** with a second interface filter; no separate watcher. Metadata/status arrive as `PropertiesChanged` on the active device's `playerN`.
- **Testing**: `BluetoothClient` node discovery (pw-dump parse) is unit-testable by injecting a `QProcess` seam; mute state and the re-assertion invariant are controller-logic tests; transport routing is tested via the facade.
- **NixOS system contract**: the polkit rule already covers `org.bluez` calls; AVRCP `/playerN` reads/`MediaPlayer1` calls and `Adapter1.Set` need to work for the kiosk user (same policy). `pw-cli` must be on PATH. Recorded in SYSTEM_INTERFACE.md.