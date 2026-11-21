import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import BierKistnRadio

Rectangle {
    id: root
    color: Theme.backgroundColor
    property var playback: PlaybackController
    property var artCache: ArtCache
    property string cachedArtUrl: ""
    readonly property string requestedArtUrl: root.playbackState === PlaybackController.SpotifyActive
        ? root.playback.spotify.artUrl : ""

    // Handler: no arguments/return; clear stale art and request URL-correlated caching.
    onRequestedArtUrlChanged: {
        root.cachedArtUrl = ""
        if (root.requestedArtUrl !== "") {
            root.cachedArtUrl = root.artCache.cacheArt(root.requestedArtUrl,
                                                       root.requestedArtUrl).toString()
        }
    }

    Connections {
        target: root.artCache
        // key: caller correlation URL; cachedUrl: local URL or empty failure.
        // Returns undefined; obsolete artwork completions cannot replace current art.
        function onArtCached(key, cachedUrl) {
            if (key === root.requestedArtUrl) {
                root.cachedArtUrl = cachedUrl.toString()
            }
        }
    }

    readonly property int playbackState: root.playback.playbackState

    readonly property bool showSpotifyProgress:
        root.playbackState === PlaybackController.SpotifyActive

    readonly property bool showBluetoothBar:
        root.playbackState === PlaybackController.BluetoothActive
        && root.playback.bluetooth.positionPublished

    readonly property bool bluetoothHasDuration:
        root.playback.bluetooth.duration > 0

    readonly property bool showTimeLabels:
        root.showSpotifyProgress || (root.showBluetoothBar && root.bluetoothHasDuration)

    readonly property bool showTransport:
        root.playbackState === PlaybackController.SpotifyActive
        || (root.playbackState === PlaybackController.BluetoothActive
            && root.playback.bluetooth.statusPublished)

    readonly property bool isPlaying:
        root.playbackState === PlaybackController.SpotifyActive
            ? root.playback.spotify.isSpotifyPlaying
            : root.playbackState === PlaybackController.BluetoothActive
              && root.playback.bluetooth.isBluetoothPlaying

    // ---- Interpolated Spotify position -------------------------------------
    // spotifyd only publishes `Position` on the 5 s poll (no PropertiesChanged
    // for it), so the bare backend value would step every 5 s. Extrapolate
    // smoothly between backend syncs using wall-clock, and re-anchor each time a
    // real position (or play-state/track) change arrives.
    property double spPosMs: 0
    property double spAnchorMs: 0
    property double spAnchorTime: Date.now()

    property double backendSpPos: root.playback.spotify.position
    // Handler: no arguments/return; a real position update supersedes interpolation.
    onBackendSpPosChanged: root.reanchorSp()

    property bool backendSpPlaying: root.playback.spotify.isSpotifyPlaying
    // Handler: no arguments/return; play/pause changes reanchor elapsed time.
    onBackendSpPlayingChanged: root.reanchorSp()

    // No arguments; returns undefined. Save backend milliseconds and wall-clock anchor.
    function reanchorSp() {
        root.spAnchorMs = root.playback.spotify.position
        root.spAnchorTime = Date.now()
        root.recomputeSp()
    }

    // No arguments; returns undefined. Extrapolate while playing, cap to known duration;
    // paused/inactive display uses the last backend position rather than advancing it.
    function recomputeSp() {
        if (root.showSpotifyProgress && root.playback.spotify.isSpotifyPlaying) {
            var est = root.spAnchorMs + (Date.now() - root.spAnchorTime)
            var dur = root.playback.spotify.duration
            if (dur > 0 && est > dur) {
                est = dur
            }
            root.spPosMs = est
        } else {
            // Paused or not showing: surface the last true position.
            root.spPosMs = root.playback.spotify.position
        }
    }

    Timer {
        id: spInterpolator
        interval: 500
        repeat: true
        running: root.showSpotifyProgress
                  && root.playback.spotify.isSpotifyPlaying
                 && !scrubSlider.pressed
        // Handler: no arguments/return; update smooth position only while not scrubbing.
        onTriggered: root.recomputeSp()
    }

    // Handler: no arguments/return; initialize the interpolation anchor.
    Component.onCompleted: root.reanchorSp()

    readonly property double currentMs:
        root.showSpotifyProgress ? root.spPosMs
                                  : root.playback.bluetooth.position

    readonly property double totalMs:
        root.showSpotifyProgress ? root.playback.spotify.duration
                                  : root.playback.bluetooth.duration

    // ms: finite numeric milliseconds. Returns nonnegative minutes:ss (minutes may exceed 59).
    function formatTime(ms) {
        if (ms < 0) {
            ms = 0
        }
        var totalSeconds = Math.floor(ms / 1000)
        var minutes = Math.floor(totalSeconds / 60)
        var seconds = totalSeconds % 60
        return minutes + ":" + (seconds < 10 ? "0" : "") + seconds
    }

    // No arguments; returns undefined. Toggle only if this source publishes transport;
    // the facade additionally checks actual readiness before issuing commands.
    function togglePlayPause() {
        if (!root.showTransport) {
            return
        }
        var playing
        if (root.playbackState === PlaybackController.SpotifyActive) {
            playing = root.playback.spotify.isSpotifyPlaying
        } else if (root.playbackState === PlaybackController.BluetoothActive) {
            playing = root.playback.bluetooth.isBluetoothPlaying
        }
        if (playing) {
            root.playback.pause()
        } else {
            root.playback.play()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.smallSpacing
        spacing: Theme.smallSpacing

        // ---------- Album art (always visible) ----------
        Image {
            id: albumArt
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: Theme.artworkSize
            Layout.preferredHeight: Theme.artworkSize
            source: root.cachedArtUrl !== "" ? root.cachedArtUrl
                : "qrc:/qt/qml/BierKistnRadio/assets/fallback-album.svg"
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            // Handler: no arguments/return; failed cached image falls back to bundled art.
            onStatusChanged: {
                if (status === Image.Error && root.cachedArtUrl !== "") {
                    root.cachedArtUrl = ""
                }
            }

            MouseArea {
                id: coverGesture
                objectName: "coverGesture"
                anchors.fill: parent
                enabled: root.showTransport
                // A horizontal move of at least one touch target is a swipe;
                // shorter movements count as taps only within the tap radius.
                readonly property real swipeDistance: Theme.touchTarget
                readonly property real tapDistance: Theme.smallSpacing * 2
                property real startX: 0
                property real startY: 0

                // mouse: area-relative event; returns undefined. Remember gesture origin.
                onPressed: mouse => {
                    startX = mouse.x
                    startY = mouse.y
                }
                // mouse: release event; returns undefined. Choose one in-bounds swipe/tap
                // action; horizontal distance/dominance avoids a swipe also toggling playback.
                onReleased: mouse => {
                    if (!root.showTransport || mouse.x < 0 || mouse.x > width
                            || mouse.y < 0 || mouse.y > height) {
                        return
                    }
                    var dx = mouse.x - startX
                    var dy = mouse.y - startY
                    if (Math.abs(dx) >= swipeDistance && Math.abs(dx) > Math.abs(dy)) {
                        if (dx < 0) {
                            root.playback.next()
                        } else {
                            root.playback.previous()
                        }
                    } else if (Math.abs(dx) <= tapDistance && Math.abs(dy) <= tapDistance) {
                        root.togglePlayPause()
                    }
                }
            }
        }

        // ---------- Progress: scrubber / passive bar (T14) ----------
        Item {
            id: progressRow
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.touchTarget + Theme.smallSpacing

            Label {
                id: currentTimeLabel
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                text: root.formatTime(root.currentMs)
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryTextColor
                visible: root.showTimeLabels
            }

            Slider {
                id: scrubSlider
                width: parent.width
                height: Theme.touchTarget
                visible: root.showSpotifyProgress
                from: 0
                to: Math.max(1, root.playback.spotify.duration / 1000)
                value: root.spPosMs / 1000
                // While pressed: follow the thumb visually/label-wise only, do
                // not spam seeks. Commit ONE seek when the drag ends.
                // Handler: no arguments/return; preview thumb position, then seek once on release.
                onPressedChanged:
                    if (scrubSlider.pressed) {
                        root.spAnchorMs = scrubSlider.value * 1000
                        root.spAnchorTime = Date.now()
                        root.spPosMs = scrubSlider.value * 1000
                    } else {
                        root.spAnchorMs = root.spPosMs
                        root.spAnchorTime = Date.now()
                        root.playback.seek(root.spPosMs)
                    }
                // Handler: no arguments/return; slider seconds -> displayed milliseconds, no seek.
                onMoved: {
                    root.spPosMs = scrubSlider.value * 1000
                    root.spAnchorMs = root.spPosMs
                    root.spAnchorTime = Date.now()
                }
            }

            ProgressBar {
                id: passiveBar
                width: parent.width
                height: Theme.touchTarget
                visible: root.showBluetoothBar
                from: 0
                to: 1
                value: root.bluetoothHasDuration
                    ? root.playback.bluetooth.position / root.playback.bluetooth.duration
                    : 0
            }

            Label {
                id: totalTimeLabel
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                text: root.formatTime(root.totalMs)
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryTextColor
                visible: root.showTimeLabels
            }
        }

        // ---------- Transport buttons (T15) ----------
        RowLayout {
            id: transportRow
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredHeight: Theme.touchTargetLarge
            spacing: Theme.defaultSpacing
            visible: root.showTransport

            Button {
                objectName: "previousButton"
                Layout.preferredWidth: Theme.touchTargetLarge
                Layout.preferredHeight: Theme.touchTargetLarge
                display: AbstractButton.IconOnly
                icon.source: "qrc:/qt/qml/BierKistnRadio/assets/previous.svg"
                icon.color: Theme.textColor
                icon.width: 28
                icon.height: 28
                Accessible.name: "Previous"
                // Handler: no arguments/return; route Previous through the facade.
                onClicked: root.playback.previous()
            }
            Button {
                objectName: "playPauseButton"
                Layout.preferredWidth: Theme.touchTargetLarge
                Layout.preferredHeight: Theme.touchTargetLarge
                display: AbstractButton.IconOnly
                icon.source: root.isPlaying
                    ? "qrc:/qt/qml/BierKistnRadio/assets/pause.svg"
                    : "qrc:/qt/qml/BierKistnRadio/assets/play.svg"
                icon.color: Theme.textColor
                icon.width: 28
                icon.height: 28
                Accessible.name: root.isPlaying ? "Pause" : "Play"
                highlighted: true
                // Handler: no arguments/return; route explicit play/pause toggle.
                onClicked: root.togglePlayPause()
            }
            Button {
                objectName: "nextButton"
                Layout.preferredWidth: Theme.touchTargetLarge
                Layout.preferredHeight: Theme.touchTargetLarge
                display: AbstractButton.IconOnly
                icon.source: "qrc:/qt/qml/BierKistnRadio/assets/next.svg"
                icon.color: Theme.textColor
                icon.width: 28
                icon.height: 28
                Accessible.name: "Next"
                // Handler: no arguments/return; route Next through the facade.
                onClicked: root.playback.next()
            }
        }

        Item { Layout.fillHeight: true; visible: !root.showTransport }
    }
}
