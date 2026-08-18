import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import BierKistnRadio

Rectangle {
    id: root
    color: Theme.backgroundColor

    readonly property bool showSpotifyArt:
        root.playbackState === PlaybackController.SpotifyActive
        && PlaybackController.spotify.artUrl !== ""

    readonly property int playbackState: PlaybackController.playbackState

    readonly property bool showSpotifyProgress:
        root.playbackState === PlaybackController.SpotifyActive

    readonly property bool showBluetoothBar:
        root.playbackState === PlaybackController.BluetoothActive
        && PlaybackController.bluetooth.positionPublished

    readonly property bool bluetoothHasDuration:
        PlaybackController.bluetooth.duration > 0

    readonly property bool showTimeLabels:
        root.showSpotifyProgress || (root.showBluetoothBar && root.bluetoothHasDuration)

    readonly property bool showTransport:
        root.playbackState === PlaybackController.SpotifyActive
        || (root.playbackState === PlaybackController.BluetoothActive
            && PlaybackController.bluetooth.statusPublished)

    readonly property string playPauseGlyph: {
        if (root.playbackState === PlaybackController.SpotifyActive) {
            return PlaybackController.spotify.isSpotifyPlaying ? "⏸" : "▶"
        }
        if (root.playbackState === PlaybackController.BluetoothActive) {
            return PlaybackController.bluetooth.isBluetoothPlaying ? "⏸" : "▶"
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

    property double backendSpPos: PlaybackController.spotify.position
    onBackendSpPosChanged: root.reanchorSp()

    property bool backendSpPlaying: PlaybackController.spotify.isSpotifyPlaying
    onBackendSpPlayingChanged: root.reanchorSp()

    function reanchorSp() {
        root.spAnchorMs = PlaybackController.spotify.position
        root.spAnchorTime = Date.now()
        root.recomputeSp()
    }

    function recomputeSp() {
        if (root.showSpotifyProgress && PlaybackController.spotify.isSpotifyPlaying) {
            var est = root.spAnchorMs + (Date.now() - root.spAnchorTime)
            var dur = PlaybackController.spotify.duration
            if (dur > 0 && est > dur) {
                est = dur
            }
            root.spPosMs = est
        } else {
            // Paused or not showing: surface the last true position.
            root.spPosMs = PlaybackController.spotify.position
        }
    }

    Timer {
        id: spInterpolator
        interval: 500
        repeat: true
        running: root.showSpotifyProgress
                 && PlaybackController.spotify.isSpotifyPlaying
                 && !scrubSlider.pressed
        onTriggered: root.recomputeSp()
    }

    Component.onCompleted: root.reanchorSp()

    readonly property double currentMs:
        root.showSpotifyProgress ? root.spPosMs
                                 : PlaybackController.bluetooth.position

    readonly property double totalMs:
        root.showSpotifyProgress ? PlaybackController.spotify.duration
                                 : PlaybackController.bluetooth.duration

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
        var playing
        if (root.playbackState === PlaybackController.SpotifyActive) {
            playing = PlaybackController.spotify.isSpotifyPlaying
        } else if (root.playbackState === PlaybackController.BluetoothActive) {
            playing = PlaybackController.bluetooth.isBluetoothPlaying
        }
        if (playing) {
            PlaybackController.pause()
        } else {
            PlaybackController.play()
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
                ? PlaybackController.spotify.artUrl
                : "qrc:/qt/qml/BierKistnRadio/assets/fallback-album.svg"
            fillMode: Image.PreserveAspectFit
            asynchronous: true
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
                to: Math.max(1, PlaybackController.spotify.duration / 1000)
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
                        PlaybackController.seek(root.spPosMs)
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
                    ? PlaybackController.bluetooth.position / PlaybackController.bluetooth.duration
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
                onClicked: PlaybackController.previous()
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
                onClicked: PlaybackController.next()
            }
        }

        Item { Layout.fillHeight: true }
    }
}