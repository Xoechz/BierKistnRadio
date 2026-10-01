import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import BierKistnRadio

Rectangle {
    id: root
    color: Theme.surfaceColor

    property var playback: PlaybackController
    readonly property int playbackState: playback.playbackState

    readonly property string btDeviceName: playback.bluetooth.connectedDeviceName
    readonly property bool btTrack: playback.bluetooth.trackPublished

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.defaultSpacing
        spacing: Theme.defaultSpacing

        // ---------- Hint / error text (non-active states) ----------
        Label {
            Layout.fillWidth: true
            font.pixelSize: Theme.fontSizeLarge
            color: Theme.secondaryTextColor
            wrapMode: Text.WordWrap
            visible: root.playbackState !== PlaybackController.SpotifyActive
                     && root.playbackState !== PlaybackController.BluetoothActive
            text: {
                switch (root.playbackState) {
                case PlaybackController.SpotifyUnavailable:
                    return "Spotify service not running — check system config"
                case PlaybackController.SpotifyWaiting:
                    return "Open Spotify on your phone — choose this speaker"
                case PlaybackController.BluetoothWaiting:
                    return "Discoverable — connect your phone"
                }
                return ""
            }
        }

        // ---------- BluetoothActive (no track) ----------
        Label {
            id: btNoTrackTitle
            objectName: "btNoTrackTitle"
            Layout.fillWidth: true
            font.pixelSize: Theme.fontSizeXLarge
            font.bold: true
            color: Theme.textColor
            wrapMode: Text.Wrap
            maximumLineCount: 3
            elide: Text.ElideRight
            visible: root.playbackState === PlaybackController.BluetoothActive && !root.btTrack
            text: "Controlled by " + root.btDeviceName
        }
        Label {
            id: btNoTrackSubtitle
            objectName: "btNoTrackSubtitle"
            Layout.fillWidth: true
            font.pixelSize: Theme.fontSizeMedium
            color: Theme.secondaryTextColor
            visible: root.playbackState === PlaybackController.BluetoothActive && !root.btTrack
            text: "No metadata available"
        }

        // ---------- Track title ----------
        Label {
            id: trackTitleLabel
            objectName: "trackTitleLabel"
            Layout.fillWidth: true
            font.pixelSize: Theme.fontSizeXLarge
            font.bold: true
            color: Theme.textColor
            wrapMode: Text.Wrap
            maximumLineCount: 3
            elide: Text.ElideRight
            visible: root.playbackState === PlaybackController.SpotifyActive
                     || (root.playbackState === PlaybackController.BluetoothActive && root.btTrack)
            text: {
                switch (root.playbackState) {
                case PlaybackController.SpotifyActive:
                    return root.playback.spotify.title
                case PlaybackController.BluetoothActive:
                    return root.playback.bluetooth.trackTitle + " via " + root.btDeviceName
                }
                return ""
            }
        }

        // ---------- Artist ----------
        Label {
            id: artistLabel
            objectName: "artistLabel"
            Layout.fillWidth: true
            font.pixelSize: Theme.fontSizeMedium
            color: Theme.secondaryTextColor
            wrapMode: Text.WordWrap
            maximumLineCount: 2
            elide: Text.ElideRight
            visible: root.playbackState === PlaybackController.SpotifyActive
                     || (root.playbackState === PlaybackController.BluetoothActive && root.btTrack)
            text: {
                switch (root.playbackState) {
                case PlaybackController.SpotifyActive:
                    return root.playback.spotify.artist
                case PlaybackController.BluetoothActive:
                    return root.playback.bluetooth.trackArtist
                }
                return ""
            }
        }

        // ---------- Album ----------
        Label {
            id: albumLabel
            objectName: "albumLabel"
            Layout.fillWidth: true
            font.pixelSize: Theme.fontSizeMedium
            color: Theme.secondaryTextColor
            wrapMode: Text.WordWrap
            maximumLineCount: 2
            elide: Text.ElideRight
            visible: root.playbackState === PlaybackController.SpotifyActive
                     || (root.playbackState === PlaybackController.BluetoothActive && root.btTrack)
            text: {
                switch (root.playbackState) {
                case PlaybackController.SpotifyActive:
                    return root.playback.spotify.album
                case PlaybackController.BluetoothActive:
                    return root.playback.bluetooth.trackAlbum
                }
                return ""
            }
        }

        // ---------- Release date (Spotify only, T25) ----------
        Label {
            id: releaseDateLabel
            Layout.fillWidth: true
            font.pixelSize: Theme.fontSizeSmall
            color: Theme.secondaryTextColor
            visible: root.playbackState === PlaybackController.SpotifyActive
                       && root.playback.releaseDate !== ""
            wrapMode: Text.WordWrap
            text: "Release Date: " + root.playback.releaseDate
        }

        Item { Layout.fillHeight: true }
    }
}
