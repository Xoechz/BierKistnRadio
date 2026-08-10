# MPRIS2 over Mopidy as the playback abstraction

The Now-Playing UI needs rich track metadata, a scrubbable progress bar, and transport control for both Spotify and internet radio. We expose all of this through a single **MPRIS2** D-Bus interface, backed by **Mopidy** (with `mopidy-spotify`, `mopidy-mpris`, and internet-radio extensions). `spotifyd` is dropped — it is only a Spotify Connect receiver and does not surface the metadata, queue, or seek control the UI requires, so a spotifyd-driven Now-Playing view would be sparse and require a separate, hand-rolled state path per source. MPRIS2 keeps the UI source-agnostic: one transport API, one metadata shape, one progress model.

## Considered Options

- **MPRIS2 via Mopidy** (chosen) — one D-Bus interface for Spotify and radio; full metadata + seek.
- **spotifyd + per-source state in the UI** — rejected; spotifyd exposes no rich metadata/queue over D-Bus, forcing the UI to special-case every source.
- **Direct Spotify Web API + separate radio daemon** — rejected; doubles the auth/sync surface and still needs a radio control path.

## Consequences

- The UI talks to exactly one MPRIS2 player name; "Source" in the UI is a Mopidy playlist/tracklist context, not a separate player.
- Bluetooth-sink playback is **not** an MPRIS source — it is PipeWire audio routing. The Now-Playing view for a Bluetooth sink shows the phone as the controller, not Mopidy metadata.
- Adding a future source (e.g. podcasts) means a Mopidy extension, not a new UI state machine.
