import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import BierKistnRadio

Rectangle {
    id: root
    color: Theme.backgroundColor
    property var playback: PlaybackController

    readonly property bool showSpotifyArt:
        root.playbackState === PlaybackController.SpotifyActive
        && root.playback.spotify.artUrl !== ""

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

    readonly property string playPauseGlyph: {
        if (root.playbackState === PlaybackController.SpotifyActive) {
            return root.playback.spotify.isSpotifyPlaying ? "⏸" : "▶"
        }
        if (root.playbackState === PlaybackController.BluetoothActive) {
            return root.playback.bluetooth.isBluetoothPlaying ? "⏸" : "▶"
        }
        return "▶"
    }

    // ---- Interpolated Spotify position -------------------------------------
    // spotifyd only publishes `Position` on the 5 s poll (no PropertiesChanged
    // for it), so the bare backend value would step every 5 s. Extrapolate
    // smoothly between backend syncs using wall-clock, and re-anchor each time a
    // real position (or play-state/track) change arrives.
    property double spPosMs: 0
    property double spAnchorMs: 0
    property double spAnchorTime: Date.now()

    property double backendSpPos: root.playback.spotify.position
    onBackendSpPosChanged: root.reanchorSp()

    property bool backendSpPlaying: root.playback.spotify.isSpotifyPlaying
    onBackendSpPlayingChanged: root.reanchorSp()

    function reanchorSp() {
        root.spAnchorMs = root.playback.spotify.position
        root.spAnchorTime = Date.now()
        root.recomputeSp()
    }

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
        onTriggered: root.recomputeSp()
    }

    Component.onCompleted: root.reanchorSp()

    readonly property double currentMs:
        root.showSpotifyProgress ? root.spPosMs
                                  : root.playback.bluetooth.position

    readonly property double totalMs:
        root.showSpotifyProgress ? root.playback.spotify.duration
                                  : root.playback.bluetooth.duration

    function formatTime(ms) {
        if (ms < 0) {
            ms = 0
        }
        var totalSeconds = Math.floor(ms / 1000)
        var minutes = Math.floor(totalSeconds / 60)
        var seconds = totalSeconds % 60
        return minutes + ":" + (seconds < 10 ? "0" : "") + seconds
    }

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
            Layout.preferredWidth: 360
            Layout.preferredHeight: 360
            Layout.topMargin: 12
            source: root.showSpotifyArt
                ? root.playback.spotify.artUrl
                : "qrc:/qt/qml/BierKistnRadio/assets/fallback-album.svg"
            fillMode: Image.PreserveAspectFit
            asynchronous: true

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

                onPressed: mouse => {
                    startX = mouse.x
                    startY = mouse.y
                }
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

        Item { Layout.fillHeight: true }

        // ---------- Progress: scrubber / passive bar (T14) ----------
        RowLayout {
            id: progressRow
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.touchTarget
            spacing: Theme.smallSpacing
            visible: root.showSpotifyProgress || root.showBluetoothBar

            Label {
                id: currentTimeLabel
                text: root.formatTime(root.currentMs)
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryTextColor
                visible: root.showTimeLabels
            }

            Slider {
                id: scrubSlider
                Layout.fillWidth: true
                visible: root.showSpotifyProgress
                from: 0
                to: Math.max(1, root.playback.spotify.duration / 1000)
                value: root.spPosMs / 1000
                // While pressed: follow the thumb visually/label-wise only, do
                // not spam seeks. Commit ONE seek when the drag ends.
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
                onMoved: {
                    root.spPosMs = scrubSlider.value * 1000
                    root.spAnchorMs = root.spPosMs
                    root.spAnchorTime = Date.now()
                }
            }

            ProgressBar {
                id: passiveBar
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.touchTarget
                visible: root.showBluetoothBar
                from: 0
                to: 1
                value: root.bluetoothHasDuration
                    ? root.playback.bluetooth.position / root.playback.bluetooth.duration
                    : 0
            }

            Label {
                id: totalTimeLabel
                text: root.formatTime(root.totalMs)
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryTextColor
                visible: root.showTimeLabels
            }
        }

        Item { Layout.fillHeight: true }

        // ---------- Transport buttons (T15) ----------
        RowLayout {
            id: transportRow
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredHeight: Theme.touchTargetLarge
            spacing: Theme.defaultSpacing
            visible: root.showTransport

            Button {
                Layout.preferredWidth: Theme.touchTargetLarge
                Layout.preferredHeight: Theme.touchTargetLarge
                text: "⏮"
                font.pixelSize: 28
                onClicked: root.playback.previous()
            }
            Button {
                Layout.preferredWidth: Theme.touchTargetLarge
                Layout.preferredHeight: Theme.touchTargetLarge
                text: root.playPauseGlyph
                font.pixelSize: 28
                highlighted: true
                onClicked: root.togglePlayPause()
            }
            Button {
                Layout.preferredWidth: Theme.touchTargetLarge
                Layout.preferredHeight: Theme.touchTargetLarge
                text: "⏭"
                font.pixelSize: 28
                onClicked: root.playback.next()
            }
        }

        Item { Layout.fillHeight: true }
    }
}
