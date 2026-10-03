# Exclusive source lifecycle instead of mute/pause-only switching

The mute/pause-only policy did not reliably prevent Spotify and Bluetooth audio from overlapping. Source switching now disables the outgoing backend and observes that shutdown before enabling the requested backend. This supersedes [ADR 0008](./0008-two-sided-audio-exclusivity.md) as the primary exclusivity mechanism; per-device muting and best-effort pause remain supplemental safeguards and takeover controls.

## Decision

- Boot into Spotify. Spotify → Bluetooth stops the kiosk user's `spotifyd.service`, observes `ActiveState=inactive` or `failed`, then powers on the BlueZ adapter. Bluetooth → Spotify observes `Adapter1.Powered=false` before starting `spotifyd.service`.
- A running user service is Spotify-ready without MPRIS or a selected track. Bluetooth readiness requires a powered adapter advertising the local A2DP Sink service UUID. A phone connection is not required. Source selection, reconnection, and playback initiation remain explicit and phone-driven.
- Bound the entire transition to 10 seconds. A successful method reply only acknowledges a job/property request; fresh observed state confirms its effect. The requested source remains selected on failure, with an actionable error and manual Retry.
- On failure or timeout, freeze command progression. Continue passive observation so an already-requested startup can recover later; never keep issuing a failed start/stop/power command. If an outgoing shutdown completes after timeout but the incoming startup was never requested, require manual Retry.
- Readiness and transition errors belong to the facade, not to MPRIS track presence. `SpotifyServiceClient` owns the session-bus systemd interface; `BluetoothClient` owns adapter power and local-service observation. QML binds `switching`, `sourceReady`, and `sourceError` and routes retry through the facade.

## Consequences

Switching to Spotify disconnects Bluetooth phones by powering off the radio; switching to Bluetooth removes Spotify Connect by stopping spotifyd. The system repo must preserve a shared kiosk user bus, allow adapter writes, boot Bluetooth off, and avoid service policies that undo intentional stops. The concrete interface and remaining target verification are recorded in [SYSTEM_INTERFACE.md §15.1](../../SYSTEM_INTERFACE.md#151-exclusive-source-lifecycle). T31 supplies the full-screen loading overlay separately.
