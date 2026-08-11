# Drop `rs.spotifyd.Controls` — Spotify is phone-driven, like Bluetooth

The app no longer uses spotifyd's custom D-Bus interface `rs.spotifyd.Controls`. There is no `TransferPlayback` button, no timeout/error handling for it, and the shared session-bus contract no longer depends on the interface — only MPRIS2 (`org.mpris.MediaPlayer2.spotifyd.instance$PID`).

## What changed

- **`transferPlayback()` → `switchToSpotify()`** just re-queries the bus for the appropriate Spotify state. Spotify playback is initiated on the phone (the phone's Spotify app selects this speaker as its playback device). The Pi mirrors the state via MPRIS2 and offers transport once a track is loaded.
- **`SpotifyReady` renamed to `SpotifyWaiting`.** The distinction is no longer "Controls present but MPRIS2 absent" (that window effectively doesn't exist in practice: once spotifyd is connected to Spotify, the MPRIS2 name is present too). Instead it is content-based: **MPRIS2 present but no track loaded** → the Pi is not the active playback device yet → show the hint "Open Spotify on your phone — choose this speaker", mirroring `BluetoothWaiting`.
- **`PlaybackController` state machine drops Controls-name tracking.** Service discovery and `serviceOwnerChanged` matching now only watch `org.mpris.MediaPlayer2.spotifyd.*`. `SpotifyUnavailable` = no MPRIS2 name (spotifyd not running / not connected).

## Considered Options

- **Keep `TransferPlayback` for Pi-initiated claiming** (status quo of ADR 0003) — rejected. spotifyd is discoverable on the LAN via zeroconf without stored credentials; any Spotify client on the LAN can select the speaker and push playback. The phone's device picker already *is* the transfer mechanism, exactly parallel to the Bluetooth A2DP sink model of [ADR 0004](./0004-phone-driven-bluetooth-connection-model.md). A Pi-initiated transfer is redundant: if a session is active, the phone can already re-select the speaker; if no session is active, there is nothing to transfer.
- **Drop `rs.spotifyd.Controls` entirely** (chosen) — removes a custom (non-standard) D-Bus interface from the app, deletes the `TransferPlayback` button flow, and shrinks the state machine. MPRIS2 alone is sufficient: it carries track metadata, transport, position, and playback state.

## Consequences

- **The app is a MPRIS2-only client for Spotify.** No custom spotifyd interface calls, no `rs.spotifyd.*` name watching. Spotify's MPRIS2 surface lives in `SpotifyClient`, coordinated by the `PlaybackController` facade (see [ADR 0006](./0006-source-clients-and-best-effort-avrcp-controls.md)).
- **Spotify and Bluetooth are now symmetric phone-driven sources.** Neither is initiated from the Pi. The Now-Playing view has a "waiting" state for each source (`SpotifyWaiting`, `BluetoothWaiting`) that tells the user to use their phone. `SpotifyActive` shows rich metadata + transport over MPRIS2. Bluetooth is the *degraded best-effort* mirror: its transport + metadata come from AVRCP `MediaPlayer1` and are shown only when the phone publishes `Status`/`Track` (see [ADR 0006](./0006-source-clients-and-best-effort-avrcp-controls.md)).
- **`SpotifyWaiting` detection is content-based, not name-based.** Because the MPRIS2 well-known name is present as soon as spotifyd is connected, "not the active device yet" is inferred from MPRIS2 *content* — no loaded track (`mpris:trackid` == `/org/mpris/MediaPlayer2/TrackList/NoTrack`, or `PlaybackStatus == "Stopped"`). The controller republishes this as `playbackState`.
- **The NixOS system contract no longer mentions spotifyd Controls.** The system repo still runs spotifyd with `use_mpris = true` as a systemd user service on the session bus; only discovery of the MPRIS2 name is required. `SYSTEM_INTERFACE.md` is updated accordingly.
- **No Pi-side way to force-claim playback.** The device cannot make itself the active playback device without a phone/Spotify client acting. On a kiosk this is acceptable and consistent with [ADR 0004](./0004-phone-driven-bluetooth-connection-model.md).