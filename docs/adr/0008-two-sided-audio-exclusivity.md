# Two-sided audible-Source exclusivity (mute + pause both directions)

Audio exclusivity is **no longer one-sided**. At any moment exactly the **active Source's** streams are audible; the inactive Source is **muted and paused** — when you switch away *and* whenever one of its streams appears while the other mode is active. This supersedes and amends ADR 0006 decisions 10–13.

## Context

[ADR 0006](./0006-source-clients-and-best-effort-avrcp-controls.md) committed to a one-sided rule: mute (only) the BT stream when switching *to* Spotify, re-asserting on a BT connect while in a Spotify state, and unmute only on `switchToBluetooth()`. Two gaps surfaced at runtime:

1. **Spotify can auto-start while Source = Bluetooth.** `PlaybackController::onSpotifyChanged()` early-returns whenever `playbackState` is a Bluetooth state, so spotifyd becoming playable in BT mode was never silenced — live overlap, exactly the bug the mute invariant existed to prevent.
2. The "hard mute" (`BluetoothClient::setMuted`) only muted the **active** device's node (and no-oped when no active device), so it could fail to silence the device that was *actually* streaming — the mute "didn't work".

The core guarantee: **only one source is audible at a time**, and the source that is *shown* (the `Source`, [ADR 0004](./0004-phone-driven-bluetooth-connection-model.md), the toggle) is the one that owns audio.

## Decision

### The invariant

- **Only the active Source is audible.** In Bluetooth mode, only the **active phone** is audible (the takeover winner, [ADR 0004](./0004-phone-driven-bluetooth-connection-model.md) / [ADR 0006](./0006-source-clients-and-best-effort-avrcp-controls.md) — BlueZ feeds one A2DP SRC at a time anyway); other connected-but-not-active devices stay muted+paused.
- **The inactive Source is muted and paused** — when you switch away *and* when any of its streams appears while the other mode is active (re-asserted on every relevant connect).
- **Unmute on entering the mode** (`switchToBluetooth` / a new stream of the active mode appearing).

### Per-side mechanism

- **BT side ("muted and paused")**: hard-mute the connected devices' BlueZ/PipeWire A2DP nodes by address (`api.bluez5.address`, discovered at call time via `pw-dump`; `wpctl set-mute`, per ADR 0006 #11 — never the whole sink); plus best-effort AVRCP `Pause` on those players. In Spotify mode mute **all** connected nodes; in BT mode unmute only the **active** device's node.
- **Spotify side ("muted = paused")**: spotify's output has **no independent node** — it feeds the physical sink the BT path also routes through, so there is no spotify-only mute. "Muting" spotifyd is **MPRIS `Pause`**. No auto-resume on return (ADR 0004: the phone initiates playback).

### Triggers (the exclusivity engine)

1. Switch to Spotify → mute + AVRCP-pause all connected BT; re-derive global Spotify state; Spotify becomes audible.
2. Switch to Bluetooth → `MPRIS Pause` spotifyd; unmute the active BT node; BT becomes audible; (still assert Discoverable if none is paired/connected).
3. A new BT stream while in a Spotify state → mute + AVRCP-pause it (re-assert; ADR 0006 #12 generalized).
4. A new BT stream while in BT mode → unmute it (and let the takeover flow pick the openly active one).
5. A new Spotify session while in BT mode → `MPRIS Pause` it.
6. Re-assert the mute on every BT connect, not only the transition points.

### UI

- Muted ("· Muted" suffix) is surfaced on the Right Sidebar's Bluetooth status only **while a device is connected and muted** (no phantom "muted" on an empty bus).
- Bluetooth device is shown ("Connected to \<name\>") in **any** Source state, not just Bluetooth mode, so a connected phone is visible while you play Spotify.

## Considered Options

- **All connected BT audible in BT mode** — rejected (user: "only one active phone is allowed to play"). Takeover + single A2DP SRC keep one audible; non-active muted.
- **Auto-resume spotifyd on return to Spotify** — rejected; phone-driven resume (ADR 0004). The app never issues Play unless asked.
- **Mute `@DEFAULT_AUDIO_SINK@` to silence spotify** — rejected; that's the shared sink that also carries BT; it would mute everything.
- **Keep the one-sided (BT-mute-only) rule** — rejected; spotify pulsing in BT mode was exactly the overlap ADR 0006 existed to block.

## Consequences

- **Amends ADR 0006 §10–13**: the invariant is now "inactive Source muted+paused", not just "BT muted when Switching to Spotify". Muting in BT mode also trips AVRCP pause; pausing spotifyd is the spotify-side mute.
- **`BluetoothClient::setMuted(bool)` semantics**: `true` mutes **all** connected devices' nodes; `false` unmutes the **active** device's node. A connect re-asserts the current intent (`m_muted`).
- **Right Sidebar** shows connected device in any state + "· Muted" suffix on the muted, connected stream.
- **No new daemons or system changes** — same `pw-dump`/`wpctl`/AVRCP tooling as ADR 0006.