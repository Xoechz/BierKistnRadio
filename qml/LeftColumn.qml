import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import BierKistnRadio

Rectangle {
    id: root
    color: Theme.surfaceColor

    readonly property int playbackState: PlaybackController.playbackState

    readonly property string btDeviceName: PlaybackController.bluetooth.connectedDeviceName
    readonly property bool btTrack: PlaybackController.bluetooth.trackPublished

    property string releaseDateText: ""

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
            Layout.fillWidth: true
            font.pixelSize: Theme.fontSizeXLarge
            font.bold: true
            color: Theme.textColor
            wrapMode: Text.WordWrap
            maximumLineCount: 3
            elide: Text.ElideRight
            visible: root.playbackState === PlaybackController.BluetoothActive && !root.btTrack
            text: "Controlled by " + root.btDeviceName
        }
        Label {
            id: btNoTrackSubtitle
            Layout.fillWidth: true
            font.pixelSize: Theme.fontSizeMedium
            color: Theme.secondaryTextColor
            visible: root.playbackState === PlaybackController.BluetoothActive && !root.btTrack
            text: "No metadata available"
        }

        // ---------- Track title ----------
        Label {
            id: trackTitleLabel
            Layout.fillWidth: true
            font.pixelSize: Theme.fontSizeXLarge
            font.bold: true
            color: Theme.textColor
            wrapMode: Text.WordWrap
            maximumLineCount: 3
            elide: Text.ElideRight
            visible: root.playbackState === PlaybackController.SpotifyActive
                     || (root.playbackState === PlaybackController.BluetoothActive && root.btTrack)
            text: {
                switch (root.playbackState) {
                case PlaybackController.SpotifyActive:
                    return PlaybackController.spotify.title
                case PlaybackController.BluetoothActive:
                    return PlaybackController.bluetooth.trackTitle + " via " + root.btDeviceName
                }
                return ""
            }
        }

        // ---------- Artist ----------
        Label {
            id: artistLabel
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
                    return PlaybackController.spotify.artist
                case PlaybackController.BluetoothActive:
                    return PlaybackController.bluetooth.trackArtist
                }
                return ""
            }
        }

        // ---------- Album ----------
        Label {
            id: albumLabel
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
                    return PlaybackController.spotify.album
                case PlaybackController.BluetoothActive:
                    return PlaybackController.bluetooth.trackAlbum
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
                     && root.releaseDateText !== ""
            text: "Release Date: " + root.releaseDateText
        }

        Item { Layout.fillHeight: true }
    }
}