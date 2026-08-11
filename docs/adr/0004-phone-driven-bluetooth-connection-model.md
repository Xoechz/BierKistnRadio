# Phone-driven Bluetooth connection model

The Bluetooth sink is a **phone-driven** device: the phone finds, pairs, and connects to the Pi; the Pi advertises itself but never initiates pairing, discovery, or connection. The UI is **state-only** — it shows the currently-connected device and mediates a single takeover dialog; it does not enumerate devices or manage pairings. The Pi's discoverability/pair-ability is owned by the NixOS system config, not the app.

This supersedes the implied Bluetooth device-management model in [ADR 0003](./0003-spotifyd-and-bluetooth-sink-mopidy-dropped.md)'s T7/T8/T16 and the `BluetoothController` scaffolding (device list, `pair`, `connectDevice`, `disconnectDevice`, `setDiscoverable` UI toggle).

## Context

The Pi acts as an A2DP sink; the phone plays audio and streams it to the Pi. The question was what the UI's Bluetooth surface should be: a full device manager (scan, list, pair, connect per-row) vs. a thin, phone-driven state view. BlueZ research established:

- `StartDiscovery` / `Device1.Pair()` only have meaning if the **Pi** drives discovery & pairing. For the Pi to be *found*, only `Discoverable=true` (+ `Powered`/`Connectable`) is needed.
- JustWorks pairing completes with **no agent** (BlueZ falls back to `NoInputNoOutput`); an agent is only needed for numeric/PIN flows, which phone-driven pairing avoids.
- `Discoverable` defaults to **false** and reverts after 180 s — so "always findable" belongs in system config (`DiscoverableTimeout=0`, `AutoEnable`), not re-asserted from the app.
- Multiple phones can connect at once; WirePlumber picks a default sink but **does not** auto-kick the old one.

## Decision

1. **Phone drives everything.** The Pi advertises; the phone finds, pairs, connects. The Pi UI never initiates pairing, never runs `StartDiscovery`, never sends `Connect` to a device.
2. **UI is state-only.** The Settings → Bluetooth module shows only the current connection state (and acts on takeover). No device list, no Pair/Connect/Disconnect rows.
3. **Discoverability/pairing policy is NixOS-owned as a BASE config, with app re-assertion on demand.** The system repo sets the adapter to always discoverable + pairable (`DiscoverableTimeout=0`, `Pairable`, `AutoEnable`). Because BlueZ drops `Discoverable` the moment a device connects (it is not restored by an enforcement service), the **app re-asserts `Discoverable=true`** whenever the user switches to the Bluetooth source while no device is connected (`BluetoothWaiting`). The app has no Discoverable toggle — just this one-shot assertion when it needs a phone to find it.
4. **Connection/takeover state is UI-owned.** `BluetoothController` watches `Device1` objects via `PropertiesChanged` and calls `Device1.Disconnect()` to kick a device. This is independent of who owns discoverability.
5. **One active phone, takeover mediated by a modal dialog.** When a second device connects while one is active, a modal dialog asks "Keep <current> or switch to <new>?". Default after **10 seconds** is **keep current**; choosing keep disconnects the new phone.

## Considered Options

- **Full device manager (UI-driven pairing)** — rejected: `StartDiscovery`/`Pair`/per-row connect are dead weight in a phone-driven model; contradicts the "connection happens on the phone" principle. A phone is always a better discovery/pairing UI than a 1024×600 touchscreen.
- **Auto-switch newest, no dialog** — rejected: silently lets anyone walking up with a phone take over the speaker; violates the user's "not everyone can kick me out" requirement.
- **Discoverable as a UI toggle** — rejected: reintroduces the "why can't my phone find it?" failure mode, and the takeover dialog (not discoverability-off) is the real takeover protection. Kept NixOS-owned for simplicity.

## Consequences

- TODO **T7 collapses**: no `StartDiscovery`, no discovery-based device list. Implemented as: watch `org.bluez` `Device1` objects + `PropertiesChanged` for `Connected`/`Name`, and the takeover dialog flow.
- TODO **T16 simplifies**: Settings → Bluetooth shows current connected device + takeover state; the Discoverable switch and device rows are gone.
- **`BluetoothController` API shrinks**: drops `devices`, `pair`, `connectDevice`, `disconnectDevice`, and the `setDiscoverable` *toggle*. Keeps a connection/state surface (`connectedDeviceName`, takeover flow, `disconnectCurrent`) and adds the one-shot `ensureDiscoverable()` re-assertion for `BluetoothWaiting`.
- **`Powered` state stays a concern**: the app still needs the adapter powered; whether that's NixOS (`AutoEnable`) or app-asserted is a system-repo detail recorded in SYSTEM_INTERFACE.md.
- **`Discoverable` re-assertion is app-owned**: BlueZ drops `Discoverable` on connect and never restores it; the app re-asserts it via `Adapter1.Set(Discoverable, true)` when entering `BluetoothWaiting` (user switched to Bluetooth with no device connected). No system-level `bluetoothctl discoverable on` enforcement service is needed.
- **Two phones at once**: app enforces single-active via the takeover dialog (disconnect on "keep current" or on switching). WirePlumber's default-sink behavior is not relied on.
- **logind active-session caveat**: BlueZ/PipeWire only expose Bluetooth nodes to the active logind session — the kiosk user must hold the active seat (cage session) for devices to appear. Recorded in SYSTEM_INTERFACE.md.
