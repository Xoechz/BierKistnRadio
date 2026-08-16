import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import BierKistnRadio

Page {
    background: Rectangle { color: Theme.backgroundColor }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.defaultSpacing
        spacing: Theme.defaultSpacing

        // ---------- Title / primary line ----------
        Label {
            text: {
                switch (PlaybackController.playbackState) {
                case PlaybackController.SpotifyUnavailable:
                    return "Spotify service not running";
                case PlaybackController.SpotifyWaiting:
                    return "Open Spotify on your phone — choose this speaker";
                case PlaybackController.SpotifyActive:
                    return PlaybackController.spotify.title;
                case PlaybackController.BluetoothWaiting:
                    return "Discoverable — connect your phone";
                case PlaybackController.BluetoothActive:
                    return PlaybackController.bluetooth.trackPublished
                        ? PlaybackController.bluetooth.trackTitle
                        : "Controlled by " + PlaybackController.bluetooth.connectedDeviceName;
                }
                return "";
            }
            font.pixelSize: Theme.fontSizeXLarge
            font.bold: true
            color: Theme.textColor
            Layout.fillWidth: true
            elide: Text.ElideRight
        }

        Label {
            text: {
                switch (PlaybackController.playbackState) {
                case PlaybackController.SpotifyActive:
                    return PlaybackController.spotify.artist;
                case PlaybackController.BluetoothActive:
                    return PlaybackController.bluetooth.trackPublished
                        ? PlaybackController.bluetooth.trackArtist +
                          (PlaybackController.bluetooth.trackAlbum
                               ? " — " + PlaybackController.bluetooth.trackAlbum
                               : "")
                        : "No Metadata available";
                default:
                    return "";
                }
            }
            font.pixelSize: Theme.fontSizeMedium
            color: Theme.secondaryTextColor
            Layout.fillWidth: true
            elide: Text.ElideRight
        }

        // ---------- Album art (Spotify only) ----------
        Image {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: 200
            source: PlaybackController.spotify.artUrl
            fillMode: Image.PreserveAspectFit
            visible: PlaybackController.playbackState === PlaybackController.SpotifyActive
                     && PlaybackController.spotify.artUrl !== ""
        }

        // ---------- Progress: scrubbable slider (Spotify) ----------
        Slider {
            Layout.fillWidth: true
            from: 0
            to: PlaybackController.spotify.duration > 0
                ? PlaybackController.spotify.duration : 1
            value: PlaybackController.spotify.position
            enabled: true
            onMoved: PlaybackController.seek(value)
            visible: PlaybackController.playbackState === PlaybackController.SpotifyActive
        }

        // ---------- Progress: passive bar (Bluetooth, no thumb) ----------
        ProgressBar {
            Layout.fillWidth: true
            from: 0
            to: PlaybackController.bluetooth.positionPublished &&
                PlaybackController.bluetooth.duration > 0
                ? 1 : 1
            value: PlaybackController.bluetooth.positionPublished
                ? (PlaybackController.bluetooth.position /
                   PlaybackController.bluetooth.duration)
                : 0
            visible: PlaybackController.playbackState === PlaybackController.BluetoothActive
                     && PlaybackController.bluetooth.positionPublished
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.defaultSpacing
            visible: PlaybackController.playbackState === PlaybackController.BluetoothActive
                     && PlaybackController.bluetooth.statusPublished

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
                text: PlaybackController.bluetooth.isBluetoothPlaying ? "⏸" : "▶"
                font.pixelSize: 28
                highlighted: true
                onClicked: PlaybackController.pause()
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

        // ---------- Transport: Spotify ----------
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.touchTargetLarge
            spacing: Theme.defaultSpacing
            visible: PlaybackController.playbackState === PlaybackController.SpotifyActive

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
                text: PlaybackController.spotify.isSpotifyPlaying ? "⏸" : "▶"
                font.pixelSize: 28
                highlighted: true
                onClicked: PlaybackController.spotify.isSpotifyPlaying
                    ? PlaybackController.pause()
                    : PlaybackController.play()
            }
            Button {
                Layout.preferredWidth: Theme.touchTargetLarge
                Layout.preferredHeight: Theme.touchTargetLarge
                text: "⏭"
                font.pixelSize: 28
                onClicked: PlaybackController.next()
            }
        }

        // ---------- Volume slider (universal, source-independent) ----------
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.defaultSpacing

            Label {
                Layout.preferredWidth: 48
                text: "🔉"
                color: Theme.secondaryTextColor
                Layout.alignment: Qt.AlignVCenter
            }

            Slider {
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.touchTargetLarge
                from: 0
                to: 150
                value: VolumeController.volume
                onMoved: VolumeController.setVolume(value)
            }
        }
    }
}
