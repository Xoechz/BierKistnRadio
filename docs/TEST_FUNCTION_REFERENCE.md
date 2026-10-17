# Test function reference

Companion to [FUNCTION_REFERENCE.md](./FUNCTION_REFERENCE.md). The test functions below exercise the production contracts; their passing assertions are not proof of physical Pi/phone acceptance.

**Return conventions:** C++ test slots and fixture actions return `void`; QML test/actions return JavaScript `undefined`, unless a row states otherwise. Failure is recorded by Qt Test assertions, not by a function's return value. “None” means no explicit parameters. Case-specific lambdas use the callback contracts listed at the end; captured fixture variables are their dependencies.

## C++ controller suite

Source: [tst_controllers.cpp](../tests/tst_controllers.cpp). Every `TestControllers` slot has **parameters: none; return: `void`**. The full names below identify the individual functions, including the data-row producer.

| Function | Purpose / test effects |
| --- | --- |
| `TestControllers::testPlaybackControllerDefaults()` | Verifies boot-view defaults and owned source children. |
| `TestControllers::testPlaybackControllerBluetoothTransitions()` | Uses missing injected buses to verify retained requested source, visible failure, and connection/metadata not selecting sources. |
| `TestControllers::testWifiControllerDefaults()` | Checks initial Wi-Fi properties. |
| `TestControllers::testWifiControllerScanFindsWifiDevice()` | Checks device-type enumeration selects Wi-Fi. |
| `TestControllers::testWifiControllerScanIssuesRequestScanOnWireless()` | Checks scan call targets the Wireless interface. |
| `TestControllers::testWifiControllerScanNoWifiDevice()` | Checks no-adapter lookup error. |
| `TestControllers::testWifiControllerAccessPointAddedRemoved()` | Checks AP insertion/removal updates visible networks. |
| `TestControllers::testWifiControllerConnectIssuesAddAndConnect()` | Legacy test name: verifies actual AddAndActivateConnection profile/arguments, not the obsolete AddAndConnect method. |
| `TestControllers::testWifiControllerConnectOpenNetworkNoSecurity()` | Checks open-network profile omits security. |
| `TestControllers::testWifiControllerDefaultTracksDisconnected()` | Checks disconnected default/status refresh. |
| `TestControllers::testWifiControllerTracksActiveConnectionState()` | Checks active AP produces SSID/strength/connected status. |
| `TestControllers::testWifiControllerSurfacesConnectError()` | Checks activation-call failure is visible. |
| `TestControllers::testWifiControllerStaticHelpers()` | Tests SSID conversion, AP security/strength, and profile helpers. |
| `TestControllers::testWifiControllerListsExistingAccessPoints()` | Checks already-known AP inventory is loaded. |
| `TestControllers::testWifiControllerScanError()` | Checks scan permission/operation failure reporting. |
| `TestControllers::testWifiControllerActivationResult()` | Verifies acknowledgement is not activation success and observed matching/failure state resolves attempt. |
| `TestControllers::testWifiPrivateBusScanAndActivation()` | Exercises real D-Bus marshalling/path/interface/signature, activation signal, daemon disappearance/recovery, and nonduplicated success. |
| `TestControllers::testBluetoothMuteProcessTimeout()` | Runs a temporary hung pw-dump executable, verifies bounded mute lookup error, then retries through a repaired runner seam. Temporarily changes PATH and restores it. |
| `TestControllers::testBluetoothClientDefaults()` | Checks initial Bluetooth/AVRCP/takeover/mute flags. |
| `TestControllers::testBluetoothTracksConnectedDevices()` | Checks add/property/disconnect identity selection. |
| `TestControllers::testBluetoothTakeoverDetection()` | Checks a second connection opens arbitration and removal resolves it. |
| `TestControllers::testBluetoothTakeoverExposesNames()` | Checks current/incoming names and observed disconnect requirement. |
| `TestControllers::testBluetoothTakeoverIncomingNameLiveUpdate()` | Checks contender renaming updates dialog data. |
| `TestControllers::testBluetoothDeviceRemoveClearsPlayerState()` | Checks device removal invalidates active player data. |
| `TestControllers::testBluetoothAdapterStateObserved()` | Checks Powered/Discoverable/Pairable observations. |
| `TestControllers::testBluetoothEnsureDiscoverableCallsSet()` | Verifies correct adapter property setter. |
| `TestControllers::testBluetoothDiscoverabilityErrorAndRecovery()` | Checks failed discovery write and retry/error clearing. |
| `TestControllers::testBluetoothTransportErrorAndRecovery()` | Checks AVRCP failure/retry behavior. |
| `TestControllers::testBluetoothResolveTakeoverKeepDisconnectsNew()` | Verifies KeepCurrent targets newcomer and waits for disconnection. |
| `TestControllers::testBluetoothResolveTakeoverSwitchDisconnectsOld()` | Verifies SwitchToNew targets current phone and waits for observed resolution. |
| `TestControllers::testBluetoothTakeoverDisconnectFailureAndRetry()` | Checks pending choice survives disconnect error and can be retried. |
| `TestControllers::testBluetoothTakeoverIncomingDisappearsDuringDisconnect()` | Checks disappearing contender invalidates pending resolution. |
| `TestControllers::testBluetoothTakeoverDisconnectTimeout()` | Checks five-second disconnect-attempt timeout without silent choice completion. |
| `TestControllers::testBluetoothAvrcpStateFromPlayer()` | Checks published player fields/units/status. |
| `TestControllers::testBluetoothAvrcpOnlyStatusNoTrack()` | Checks optional Track/Position behavior with status-only player. |
| `TestControllers::testBluetoothTransportTargetsActiveDevice()` | Checks AVRCP commands target selected player. |
| `TestControllers::testBluetoothMuteDiscoversNodeAndMutes()` | Checks address-based PipeWire lookup and mute argv. |
| `TestControllers::testBluetoothUnmuteIssuesSetMuteZero()` | Checks active-node unmute argv. |
| `TestControllers::testBluetoothMuteCoversAllConnectedDevices()` | Checks true mute intent dispatches for every connected address. |
| `TestControllers::testBluetoothMuteCommandFailure()` | Checks current mute command failure/recovery and that mute disable/intent cleanup preserves unrelated errors. |
| `TestControllers::testBluetoothObsoleteMuteCallbacks_data()` | Supplies held lookup/write cases for disconnect, removal, adapter power-off, and superseding intent. |
| `TestControllers::testBluetoothObsoleteMuteCallbacks()` | Replays obsolete lookup/write completions; asserts no late wpctl dispatch or stale-node error after invalidation. |
| `TestControllers::testBluetoothNodeIdFromPwDump()` | Checks address matching, profile-independent names, malformed/missing node cases. |
| `TestControllers::testBluetoothAvrcpResetsOnDeviceChange()` | Checks retargeting clears unsupported/stale fields. |
| `TestControllers::testBluetoothRetargetsPlayerAfterTakeover()` | Checks takeover winner's player/metadata/status replaces previous target. |
| `TestControllers::testBluetoothDeviceWithoutNameIsDetected()` | Checks unnamed connected devices still count and use fallback display name. |
| `TestControllers::testBluetoothPlayerAddedBeforeDevice()` | Checks deferred nested-player handling and ignores a similarly named unrelated device. |
| `TestControllers::testBluetoothPrivateBusObjectManagerAndProperties_data()` | Supplies direct snapshot, nested snapshot, and late nested-player cases. |
| `TestControllers::testBluetoothPrivateBusObjectManagerAndProperties()` | Exercises typed BlueZ snapshots/signals with the S25 FE's observed Track/Status/Position/Device payload; verifies metadata/publication, exact AVRCP command targets, live updates/invalidation, player removal/reappearance, disconnect clearing, and daemon recovery. Nested cases reproduced T45 before the fix. |
| `TestControllers::testBackgroundDbusReadErrors_data()` | Registers error-type data rows for the next slot; returns no table value directly. |
| `TestControllers::testBackgroundDbusReadErrors()` | Consumes Qt Test data rows and checks read-fault classification/recovery. |
| `TestControllers::testDisconnectedBusErrors()` | Checks missing bus is surfaced rather than silent. |
| `TestControllers::testSpotifyClientDefaults()` | Checks source defaults without a session. |
| `TestControllers::testSpotifyFirstArtistFromMetadata()` | Checks joined versus first artist and metadata fallback forms. |
| `TestControllers::testSpotifyPrivateBusMprisLifecycle()` | Exercises dynamic service discovery, Player metadata/transport, session loss, and errors on private bus. |
| `TestControllers::testSpotifyIgnoresStaleStateReplies()` | Holds/replays snapshots to check live-update/session-loss generation guards. |
| `TestControllers::testVolumeControllerDefaults()` | Checks cached zero/mute/error defaults. |
| `TestControllers::testVolumeControllerParse()` | Checks ratio parsing, rounding/clamping, invalid output. |
| `TestControllers::testVolumeControllerReadsFromWpctl()` | Checks initial/prompt polling updates percentage. |
| `TestControllers::testVolumeControllerPollsExternalChanges()` | Checks repeated external-value observations. |
| `TestControllers::testVolumeControllerIssuesSetVolumeCommand()` | Checks default-sink set-volume args. |
| `TestControllers::testVolumeControllerClamping()` | Checks lower/upper percentage boundaries. |
| `TestControllers::testVolumeControllerNoReadBackRace()` | Checks a pre-write poll cannot overwrite newer optimistic write. |
| `TestControllers::testVolumeCommandErrorsAndRecovery()` | Checks read/write/malformed-output errors and observed/manual recovery. |
| `TestControllers::testPowerCommandErrorsAndRetry()` | Checks busy guard, real CLI authentication-required text classification, explicit retry, and successful acknowledgement signal. |
| `TestControllers::testVolumeMuteRestoresLastLevel()` | Checks restore history after zeroing volume. |
| `TestControllers::testVolumeMuteFallsBackToTenPercent()` | Checks unmute when no nonzero history exists. |
| `TestControllers::testVolumeMuteTracksSliderAndExternalChanges()` | Checks slider/poll raised volume is treated as unmute and updates restore history. |
| `TestControllers::testArtCacheDirCreation()` | Checks cache-directory initialization. |
| `TestControllers::testArtCacheDownloadReuseAndClear()` | Checks local HTTP image download, URL reuse, waiter notifications, and clearing. |
| `TestControllers::testArtCachePrunesOldCovers()` | Checks size-bound pruning order/data. |
| `TestControllers::testArtCacheRejectsFailuresAndInvalidImages()` | Checks invalid/failed HTTP image results leave empty cache result. |

### Controller mock methods

| Function | Parameters | Return | Purpose / effects |
| --- | --- | --- | --- |
| `MockBlueZObjects::introspect(path)` | Object path, unnamed/unused | `QString`: ObjectManager XML | Supplies snapshot signature for private-bus fixture. |
| `MockBlueZObjects::handleMessage(message, connection)` | Incoming D-Bus message; replying connection | `bool`: true for handled GetAll/snapshot or valid player controls, false otherwise | Replies with configured errors/malformed payload or typed BlueZ tree/player properties; configurable player path/presence/properties support nested/late discovery. Records Play/Pause/Next/Previous only for the configured path, MediaPlayer1 interface and empty signature. |
| MockBlueZObjects local `add(path, interface, props)` lambda | Object path; interface name; property dictionary | `void` | Appends one typed ObjectManager map entry to the captured QDBusArgument. |
| `MockMprisPlayer::introspect(path)` | Object path, unused | `QString`: Properties/Player XML | Describes GetAll/PropertiesChanged and transport/SetPosition signatures. |
| `MockMprisPlayer::handleMessage(message, connection)` | Incoming call; reply connection | `bool`: handled status | Records controls, replies with mock metadata/state/errors, or stores delayed GetAll requests for stale-reply tests. |
| `MockNetworkManager::introspect(path)` | Object path, unused | `QString`: NM/Wireless/Properties XML | Declares device/AP/scan/activation API signatures. |
| `MockNetworkManager::handleMessage(message, connection)` | Call and replying connection | `bool`: true after reply/error | Enforces fixture paths/interfaces/signatures, returns devices/AP/security data, records scan/activation args, and rejects invalid API calls. |

## C++ pairing suite

Source: [tst_pairing.cpp](../tests/tst_pairing.cpp).

| Function | Parameters | Return | Purpose / effects |
| --- | --- | --- | --- |
| `MockAgentManager::introspect(path)` | Unused path | Empty `QString` | No introspection XML needed for explicit method-call fixtures. |
| `MockAgentManager::handleMessage(message, bus)` | Incoming call; responding bus | `bool`: handled status | Records cancellation/registration/capability/default/unregister calls, injects configured authorization errors, validates manager signatures. |
| `TestPairing::initTestCase()` | None | `void` | Starts private daemon, connects app/server buses, registers BlueZ mock. |
| `TestPairing::cleanupTestCase()` | None | `void` | Disconnects buses and terminates daemon. |
| `TestPairing::ready(agent)` | Agent reference under test | `void` | Installs deterministic name/bond lookups, enables agent, waits/asserts default-agent registration and DisplayYesNo capability/path. |
| `TestPairing::call(method, args)` | Agent1 method name; typed arguments, default empty | `QDBusPendingCall`: request with 40 s timeout | Calls app export from mock BlueZ connection, allowing backend's 30 s rejection to finish before caller timeout. |
| `TestPairing::confirmationAndStaleDecisions()` | None | `void` | Checks padded code, explicit delayed acceptance, duplicate rejection, new-prompt IDs, and stale decision rejection. |
| `TestPairing::displayUpdatesCancelAndExit()` | None | `void` | Checks DisplayPasskey cancellation rather than fake approval, daemon Cancel, and pending confirmation rejection on destruction. |
| `TestPairing::timeoutAndServiceAuthorization()` | None | `void` | Exercises real 30 s timeout, expired decision, paired/unpaired authorization, and Just Works rejection. |
| `TestPairing::registrationErrorAndRecovery()` | None | `void` | Checks default-agent denial, enable-cycle retry, disabling pending prompt, and disabled request rejection. |
| `TestPairing::senderValidationAndDaemonRestart()` | None | `void` | Checks forged sender/remotely inaccessible resolve, service disappearance/re-registration, and old decision IDs. |
| `TestPairing::adapterAvailabilityAndPairedReconnect()` | None | `void` | Checks source permission plus power gate, cached Paired authorization/invalidation, identity lookup, and radio-loss rejection. |
| `TestPairing::unregisterFailureIsVisible()` | None | `void` | Checks explicit unregistration permission failure and later registration clears error. |

## C++ exclusive-source suite

Source: [tst_source_switching.cpp](../tests/tst_source_switching.cpp).

| Function | Parameters | Return | Purpose / effects |
| --- | --- | --- | --- |
| `SourceServices::introspect(path)` | Unused path | Empty `QString` | Fixture uses explicit D-Bus method messages. |
| `SourceServices::adapterProps()` | None | `QVariantMap`: Powered and local-sink UUID list | Creates current adapter fixture snapshot. |
| `SourceServices::handleMessage(message, bus)` | Incoming call; replying bus | `bool`: true when handled/rejected, false otherwise | Strictly validates systemd/BlueZ call paths/signatures, records ordering, holds reads/state transitions, returns jobs/snapshots or errors. |
| Local `reject()` lambda in handleMessage | None; captures message/bus | `bool`: true | Sends InvalidArgs so the fixture cannot validate an invalid boundary call by accident. |
| `TestSourceSwitching::initTestCase()` | None | `void` | Starts private bus, registers shared systemd/BlueZ mock object tree and marshalling types. |
| `TestSourceSwitching::cleanupTestCase()` | None | `void` | Disconnects clients and terminates private daemon. |
| `TestSourceSwitching::init()` | None | `void` | Resets service/adapter, hold/error flags, recorded commands and pending reads before each test. |
| `TestSourceSwitching::boot(playback)` | Facade reference under test | `void` | Waits/asserts ready Spotify waiting without MPRIS/phone or source error. |
| `TestSourceSwitching::bootAndOrderedTransitions()` | None | `void` | Checks production 10 s deadline and both command orders, with no redundant boot command. |
| `TestSourceSwitching::shutdownInvalidatesSupplementalMute_data()` | None | `void` | Supplies held PipeWire lookup and held mute-write cases. |
| `TestSourceSwitching::shutdownInvalidatesSupplementalMute()` | None | `void` | Uses private-bus lifecycle plus deferred command runner to verify shutdown issues no mute commands, obsolete completions cannot dispatch wpctl/report stale errors, ready Bluetooth re-enables muting, genuine failures remain visible, and the next Spotify switch clears obsolete mute errors. |
| `TestSourceSwitching::shutdownMustBeObserved()` | None | `void` | Holds outgoing state despite successful acknowledgement and verifies incoming backend cannot start yet. |
| `TestSourceSwitching::permissionFailureAndManualRetry()` | None | `void` | Checks denial/retained selection, no passive retry, explicit recovery, and opposite-side shutdown denial. |
| `TestSourceSwitching::timeoutAndLateRecovery()` | None | `void` | Uses shorter injected deadline to check one issued startup, error, and later observed readiness without another command. |
| `TestSourceSwitching::sinkAndServiceReadiness()` | None | `void` | Checks Powered alone/activating service do not complete; sink publication/active service do. |
| `TestSourceSwitching::staleReadsCannotAdvance()` | None | `void` | Holds/replays pre-command systemd reads and verifies only fresh observation advances. |
| `TestSourceSwitching::malformedStateIsAnError()` | None | `void` | Checks malformed Unit reply is visible and valid later observation recovers without unnecessary commands. |
| `TestSourceSwitching::outgoingTimeoutRequiresRetry()` | None | `void` | Checks late outgoing shutdown cannot automatically issue never-started incoming backend. |
| `TestSourceSwitching::failedSpotifyStartupAndRecovery()` | None | `void` | Checks failed unit startup, no relaunch loop, and passive active-state recovery. |
| `TestSourceSwitching::staleAdapterReadAfterLiveUpdate()` | None | `void` | Checks stale adapter snapshot cannot supersede current live PropertiesChanged state. |
| `TestSourceSwitching::missingUnitIsUnavailable()` | None | `void` | Checks real NoSuchUnit diagnostic becomes actionable unavailability and issues no lifecycle command. |

## C++ MusicBrainz HTTP suite

Source: [tst_release_date.cpp](../tests/tst_release_date.cpp).

| Function | Parameters | Return | Purpose / effects |
| --- | --- | --- | --- |
| `RecordingServer()` | None | No return | Starts request-time clock and installs TCP accept/read handling; caller must listen on a local port. |
| `RecordingServer::enqueue(status, body)` | HTTP numeric status; response bytes | `void` | Queues response and immediately flushes available pending requests. |
| `RecordingServer::flush()` | None | `void` | Pairs queued sockets/responses, writes Content-Length JSON HTTP reply, closes each connection. |
| RecordingServer newConnection lambda | None | `void` | Accepts socket and registers data/cleanup callbacks. |
| RecordingServer socket readyRead lambda | None | `void` | Accumulates bytes until headers complete, records request/time, enqueues socket, flushes. |
| `TestReleaseDate::testLookupAndCache()` | None | `void` | Checks query escaping/headers/limit, date result, notification, cache reuse, and clear. |
| `TestReleaseDate::testUnknownAndConnectionFailure()` | None | `void` | Checks missing input, ambiguous/absent recordings, and HTTP service failure are distinct results. |
| `TestReleaseDate::testStaleResponseAndRateLimit()` | None | `void` | Checks old result cannot update new track and request starts are at least one second apart. |

## QML view suites

### Cover gestures and artwork

Source: [tst_CoverGestures.qml](../tests/tst_CoverGestures.qml).

| Function / signal | Parameters | Return | Purpose / effects |
| --- | --- | --- | --- |
| `fakeArtCache.cacheArt(url, key)` | Source URL and correlation key (unused values) | Empty string | Increments request count; models pending artwork without network access. |
| `fakeArtCache.artCached(key, cachedUrl)` (signal) | Correlation key; URL-like result | No consumed result | Injects completion/stale/failure events. |
| `facade.play()` | None | `undefined` | Increments plays; no real transport. |
| `facade.pause()` | None | `undefined` | Increments pauses. |
| `facade.next()` | None | `undefined` | Increments nexts. |
| `facade.previous()` | None | `undefined` | Increments previouses. |
| `init()` | None | `undefined` | Resets source/playback/artwork fixture and action counters. |
| `cover()` | None | Gesture-area QObject | Finds/asserts coverGesture child and returns it. |
| `swipe(area, fromX, toX, toY)` | Target MouseArea; starting x; release x/y in pixels | `undefined` | Presses at start y=180, moves, releases to simulate gesture. |
| `test_spotifyGestures()` | None | `undefined` | Checks taps, both horizontal swipes, no double action, and vertical-move rejection. |
| `test_bluetoothAvailability()` | None | `undefined` | Checks gesture availability follows Status/source and dispatches BT actions. |
| `test_transportIconsAtPanelSize()` | None | `undefined` | Checks 1024×600, icon/touch dimensions, centering, tint and Play/Pause source in both themes, plus button routing. |
| `test_artDoesNotReappearAfterTrackChanges()` | None | `undefined` | Checks cache correlation, stale/failed completions and leaving active Spotify. |

### Left Column

Source: [tst_LeftColumn.qml](../tests/tst_LeftColumn.qml).

| Function | Parameters | Return | Purpose / effects |
| --- | --- | --- | --- |
| `facade.retrySource()` | None | `undefined` | Increments test retry counter. |
| `init()` | None | `undefined` | Resets theme, source/client metadata/errors, and counters. |
| `cleanup()` | None | `undefined` | Restores dark theme. |
| `checkTitleFits(title)` | Title Label QObject under test | `undefined` | Asserts bounds, line count, width/truncation and nonoverlap with artist/album or no-track subtitle. |
| `test_sourceTransitionErrorAndRetry()` | None | `undefined` | Checks lifecycle error/Retry visibility, touch height, click, disabling while switching, and recovery hiding. |
| `test_spotifyLongTitleBothThemes()` | None | `undefined` | Checks normal long title and repeated long word wrapping/elision. |
| `test_bluetoothTitleAndDeviceNameBothThemes()` | None | `undefined` | Checks BT track/device suffix and no-track title geometry. |
| `test_sourceErrorsStayWithSelectedSource()` | None | `undefined` | Checks selected client's error source and clear behavior. |

### Pairing dialog

Source: [tst_PairingDialog.qml](../tests/tst_PairingDialog.qml).

| Function / signal | Parameters | Return | Purpose / effects |
| --- | --- | --- | --- |
| `fakeAgent.resolve(id, accept)` | Expected request ID; decision boolean | `undefined` | Asserts ID, records choice/count, clears pending and emits promptChanged. |
| `fakeAgent.promptChanged()` (signal) | None | No consumed result | Triggers dialog ID/data synchronization. |
| `cleanup()` | None | `undefined` | Resets fake pending/decision/counters. |
| `test_confirmation_data()` | None | Array of `{tag, button, accept}` rows | Produces accept/reject parameterized cases. |
| `test_confirmation(data)` | One data row naming button/expected decision | `undefined` | Checks code/identity, button touch height, routing, single action, and close. |
| `test_dialogCancellationRejects()` | None | `undefined` | Checks programmatic close explicitly rejects pending pairing. |

### Takeover dialog

Source: [tst_TakeoverDialog.qml](../tests/tst_TakeoverDialog.qml).

| Function | Parameters | Return | Purpose / effects |
| --- | --- | --- | --- |
| `fakeBluetooth.resolveTakeover(choice)` | KeepCurrent/SwitchToNew enum | `undefined` | Records choice/count and marks resolving, without D-Bus. |
| `cleanup()` | None | `undefined` | Resets pending/resolving/error/counters. |
| `test_pendingFailureAndRetry()` | None | `undefined` | Checks modal policy, busy buttons/countdown, error persistence/no automatic retry, manual choice, and observed close. |

### Loading overlay

Source: [tst_SourceTransitionOverlay.qml](../tests/tst_SourceTransitionOverlay.qml).

| Function / handler | Parameters | Return | Purpose / effects |
| --- | --- | --- | --- |
| Underlying button `onClicked` | None | `undefined` | Increments counter to detect click-through. |
| `cleanup()` | None | `undefined` | Clears fake transition/error/selection, counters, theme, and waits for hidden overlay. |
| `test_slowStartupBlocksWholeWindow()` | None | `undefined` | Checks full 1024×600 modal extent, spinner/label, blocked touch/Escape and no premature UI timeout, then input recovery in both themes. |
| `test_failureRetryAndLateRecovery()` | None | `undefined` | Checks loader hiding on failure, retained Spotify waiting, no unsolicited reopening on late recovery, explicit retry showing, and final hide. |

### Wi-Fi popup integration

Source: [tst_WifiDialog.qml](../tests/tst_WifiDialog.qml).

| Function / signal | Parameters | Return | Purpose / effects |
| --- | --- | --- | --- |
| `wifi.scan()` | None | `undefined` | Counts scans; fixture list is predetermined. |
| `wifi.connect(ssid, password)` | Selected SSID/password text | `undefined` | Records args/count and marks connecting; does not emit success automatically. |
| `wifi.connectionSucceeded(ssid)` (signal) | Confirmed SSID | No consumed result | Injects actual activation result matching/mismatching selected network. |
| `popup()` | None | WifiDialog QObject, possibly null if absent | Finds named popup under actual sidebar. |
| `init()` | None | `undefined` | Activates window, clears Wi-Fi fixture state/counters, resets keyboard space. |
| `cleanup()` | None | `undefined` | Closes/waits for hidden popup and restores theme. |
| `onScreen(item, height)` | Visual item; allowed vertical extent in pixels | `undefined` | Maps into window coordinates and asserts horizontal/vertical bounds with one-pixel tolerance. |
| `openFromButton()` | None | `undefined` | Clicks real sidebar button, waits for opened popup, checks overlay parent and on-screen content/Cancel. |
| `test_actualButtonAndRepeatedCloseBothThemes()` | None | `undefined` | Checks repeated open/scan/list/Cancel sequences in both themes. |
| `test_passwordKeyboardGeometryAndActivationResult_data()` | None | Theme/height data rows | Exercises dark/light themes with keyboard tops at 280 and 250 px. |
| `test_passwordKeyboardGeometryAndActivationResult(data)` | Theme and keyboard top | `undefined` | Clicks secured delegate, checks focus, drives an opaque overlay panel through the sidebar's actual geometry binding (without overriding popup height), checks password/error/actions above it, types credentials and clicks Connect, verifies failure persistence, panel hide/show resizing, matching-only close and focus removal. |
| `test_cancelWhileKeyboardOpen()` | None | `undefined` | Selects a secured network, shows the panel, clicks reachable Cancel and verifies popup dismissal/focus removal. |
| `test_escapeClearsModalInputBlock()` | None | `undefined` | Dismisses by Escape and reopens through underlying button. |

### Theme

Source: [tst_Theme.qml](../tests/tst_Theme.qml).

| Function | Parameters | Return | Purpose / effects |
| --- | --- | --- | --- |
| `init()` | None | `undefined` | Establishes dark mode before each test. |
| `test_darkModeDefault()` | None | `undefined` | Checks initialized default flag. |
| `test_colorsDark()` | None | `undefined` | Checks dark background/surface colors. |
| `test_colorsLight()` | None | `undefined` | Checks light colors after toggle. |
| `test_touchTargets()` | None | `undefined` | Checks 48/64 touch-size constants. |

## Test-local callable contracts

The many injected fixture lambdas are deliberately local to their test scenarios. The contracts below cover their parameter/return shapes; the case matrix above identifies which behavior each instance asserts. A saved `done`/pending watcher is a delayed callback, not a completed operation.

| Callable family / location | Parameters | Return | Purpose / effects |
| --- | --- | --- | --- |
| `setDbusCallableForTest` lambdas in controller tests | Service, objectPath, interface, method, args, done(reply,error) | `void` | Capture/check target/args and supply fixture value/error, or retain completion for timing/stale-reply checks. Arguments follow the production DbusCallable contract; replies are first-value payloads. |
| `setCommandRunnerForTest` Bluetooth lambdas | Full argv; done(stdout,error) | `void` | Capture dump/mute commands and supply fixture JSON/exit failure; no real audio writes. |
| `setCommandRunnerForTest` Volume lambdas | wpctl args excluding executable; done(stdout,error) | `void` | Capture sink volume requests and simulate polls/writes/errors/order. |
| `setCommandRunnerForTest` Power lambda | Action; done(error) | `void` | Records power action and saves completion; does not reboot/shutdown. |
| Private-bus cleanup `qScopeGuard` lambdas | None | `void` | Disconnect unique test connection, terminate/wait for private daemon, or restore PATH as scoped resource exits. |
| HTTP fixture `newConnection` lambdas in art-cache tests | None | `void` | Accept socket and install per-socket response handling. |
| HTTP fixture `readyRead` lambdas in art-cache tests | None | `void` | Consume request bytes/count requests and return test image/status payloads; image bytes/socket/server variables are captured. |
| `TestPairing::ready` name resolver | Device path, unnamed/unused | `QString`: “Test Phone” | Stable identity for code/prompt tests. |
| `TestPairing::ready` paired resolver | Device path | `bool`: path ends with `paired` | Explicit bond fixture for service authorization. |
| `tryVerify` predicate in LeftColumn `checkTitleFits` | None | `bool` | Waits for valid title x/width inside column. |
| `tryVerify` predicates in LeftColumn title tests | None | `bool` | Wait for wrapping or no-track title visibility. |
| `tryVerify` predicate in PairingDialog confirmation test | None | `bool` | Waits until selected button is visible. |
| `tryVerify` predicate in WifiDialog password test | None | `bool` | Waits until delegate at index zero exists. |

## Generated test entry points

`QTEST_MAIN(TestControllers)` and `QTEST_MAIN(TestReleaseDate)` generate GUI-capable test mains; `QTEST_GUILESS_MAIN(TestPairing)` and `QTEST_GUILESS_MAIN(TestSourceSwitching)` generate Core-only test mains. [tst_qml.cpp](../tests/tst_qml.cpp) uses `QUICK_TEST_MAIN(tst_qml)` for the QML test runner. Each generated main takes process `argc`/`argv`, returns an integer process test status (0 success/nonzero failure), and initializes/runs the relevant Qt test harness. The macros' expanded library implementation is outside this authored-function catalogue.
