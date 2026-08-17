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

    readonly property double currentMs:
        root.showSpotifyProgress ? PlaybackController.spotify.position
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
        anchors.margins: Theme.defaultSpacing
        spacing: Theme.defaultSpacing

        // ---------- Album art (always visible) ----------
        Image {
            id: albumArt
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 300
            Layout.preferredHeight: 300
            Layout.topMargin: 24
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
            spacing: Theme.defaultSpacing
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
                value: PlaybackController.spotify.position / 1000
                onMoved: PlaybackController.seek(value * 1000)
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