import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import BierKistnRadio

Rectangle {
    id: root
    color: Theme.surfaceColor

    property var playback: PlaybackController
    property var wifiController: WifiController
    property var inputPanel: null

    component FieldCaption: Label {
        font.pixelSize: Theme.fontSizeSmall
        font.bold: true
        color: Theme.secondaryTextColor
    }

    // Binding: no explicit arguments; returns cached device/mute/source/power status text.
    readonly property string btStatusText: {
        const bt = root.playback.bluetooth
        const name = bt.connectedDeviceName
        if (name !== "") {
            // Visible regardless of Source state (ADR 0008): you can always see
            // that a phone is connected, plus when it is silenced.
            return "Connected to " + name + (bt.muted ? " · Muted" : "")
        }
        switch (root.playback.playbackState) {
        case PlaybackController.BluetoothWaiting:
            return "Discoverable"
        default:
            return bt.adapterPowered ? "Not connected" : "Not available"
        }
    }

    // Binding: no explicit arguments; returns connected SSID or disconnected text.
    readonly property string wifiStatusText: {
        if (root.wifiController.connected) {
            return "Connected to " + root.wifiController.ssid
        }
        return "Not connected"
    }

    readonly property int playbackState: playback.playbackState

    readonly property string btDeviceName: playback.bluetooth.connectedDeviceName
    readonly property bool btTrack: playback.bluetooth.trackPublished
    readonly property string transitionError: playback.sourceError || ""
    // Binding: no explicit arguments; returns lifecycle error first, then selected client error.
    readonly property string sourceError: {
        if (transitionError !== "") {
            return transitionError
        }
        switch (playbackState) {
        case PlaybackController.BluetoothWaiting:
        case PlaybackController.BluetoothActive:
            return playback.bluetooth.errorMessage || ""
        default:
            return playback.spotify.errorMessage || ""
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.defaultSpacing
        spacing: Theme.defaultSpacing

        Flickable {
            id: viewport
            Layout.fillHeight: true
            Layout.fillWidth: true
            clip: true
            contentHeight: metadata.implicitHeight
            boundsBehavior: Flickable.StopAtBounds

            ColumnLayout {
                id: metadata
                width: viewport.width
                spacing: 2
                Label {
                    objectName: "sourceErrorLabel"
                    Layout.fillWidth: true
                    visible: root.sourceError !== ""
                    text: root.sourceError
                    color: Theme.errorColor
                    font.pixelSize: Theme.fontSizeSmall
                    wrapMode: Text.Wrap
                }

                Button {
                    objectName: "sourceRetryButton"
                    text: "Retry"
                    visible: root.transitionError !== ""
                    enabled: !root.playback.switching
                    Layout.fillWidth: true
                    Layout.preferredHeight: Theme.touchTarget
                    // Handler: no arguments/return; manually retry the retained source selection.
                    onClicked: root.playback.retrySource()
                }

                // ---------- Hint / error text (non-active states) ----------
                Label {
                    Layout.fillWidth: true
                    font.pixelSize: Theme.fontSizeLarge
                    color: Theme.secondaryTextColor
                    wrapMode: Text.WordWrap
                    visible: root.playbackState !== PlaybackController.SpotifyActive
                             && root.playbackState !== PlaybackController.BluetoothActive
                    // Binding: no explicit arguments; returns waiting/unavailable hint by view state.
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
                FieldCaption {
                    text: "Track"
                    visible: trackTitleLabel.visible
                }
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
                    visible: text !== ""
                    // Binding: no explicit arguments; returns Spotify title or BT title/device suffix.
                    text: {
                        switch (root.playbackState) {
                        case PlaybackController.SpotifyActive:
                            return root.playback.spotify.title
                        case PlaybackController.BluetoothActive:
                            return root.btTrack && root.playback.bluetooth.trackTitle
                                ? root.playback.bluetooth.trackTitle
                        }
                        return ""
                    }
                }

                // ---------- Artist ----------
                FieldCaption {
                    text: "Artist"
                    visible: artistLabel.visible
                }
                Label {
                    id: artistLabel
                    objectName: "artistLabel"
                    Layout.fillWidth: true
                    font.pixelSize: Theme.fontSizeLarge
                    color: Theme.secondaryTextColor
                    wrapMode: Text.WordWrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                    visible: text !== ""
                    // Binding: no explicit arguments; returns selected-source artist or empty.
                    text: {
                        switch (root.playbackState) {
                        case PlaybackController.SpotifyActive:
                            return root.playback.spotify.artist
                        case PlaybackController.BluetoothActive:
                            return root.btTrack ? root.playback.bluetooth.trackArtist : ""
                        }
                        return ""
                    }
                }

                // ---------- Album ----------
                FieldCaption {
                    text: "Album"
                    visible: albumLabel.visible
                }
                Label {
                    id: albumLabel
                    objectName: "albumLabel"
                    Layout.fillWidth: true
                    font.pixelSize: Theme.fontSizeMedium
                    color: Theme.secondaryTextColor
                    wrapMode: Text.WordWrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                    visible: text !== ""
                    // Binding: no explicit arguments; returns selected-source album or empty.
                    text: {
                        switch (root.playbackState) {
                        case PlaybackController.SpotifyActive:
                            return root.playback.spotify.album
                        case PlaybackController.BluetoothActive:
                            return root.btTrack ? root.playback.bluetooth.trackAlbum : ""
                        }
                        return ""
                    }
                }

                // ---------- Release date (Spotify only, T25) ----------
                FieldCaption {
                    text: "Release Date"
                    visible: releaseDateLabel.visible
                }
                Label {
                    id: releaseDateLabel
                    objectName: "releaseDateLabel"
                    Layout.fillWidth: true
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.secondaryTextColor
                    visible: root.playbackState === PlaybackController.SpotifyActive
                               && /^\d{4}(?:-\d{2}){0,2}$/.test(root.playback.releaseDate)
                    wrapMode: Text.WordWrap
                    text: root.playback.releaseDate.replace(/^(\d{4})-(\d{2})-(\d{2})$/, "$3.$2.$1")
                }
                Label {
                    Layout.fillWidth: true
                    text: root.playback.releaseDate
                    visible: root.playbackState === PlaybackController.SpotifyActive
                             && text !== "" && text !== "Release Date unknown" && !releaseDateLabel.visible
                    color: Theme.errorColor
                    font.pixelSize: Theme.fontSizeSmall
                    wrapMode: Text.WordWrap
                }
            }
        }

        // ---------- Bluetooth status ----------
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            Label {
                text: "Bluetooth"
                font.pixelSize: Theme.fontSizeSmall
                font.bold: true
                color: Theme.secondaryTextColor
            }
            Label {
                Layout.fillWidth: true
                text: root.btStatusText
                font.pixelSize: Theme.fontSizeMedium
                color: Theme.textColor
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }
            Label {
                Layout.fillWidth: true
                text: root.playback.bluetooth.errorMessage
                visible: text !== "" && root.playback.playbackState !== PlaybackController.BluetoothWaiting
                         && root.playback.playbackState !== PlaybackController.BluetoothActive
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.errorColor
                wrapMode: Text.Wrap
            }
        }

        // ---------- Wi-Fi status ----------
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            Label {
                text: "Wi-Fi"
                font.pixelSize: Theme.fontSizeSmall
                font.bold: true
                color: Theme.secondaryTextColor
            }
            Label {
                Layout.fillWidth: true
                text: root.wifiStatusText
                font.pixelSize: Theme.fontSizeMedium
                color: Theme.textColor
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }
        }

        // ---------- Wifi Settings button ----------
        Button {
            objectName: "wifiSettingsButton"
            text: "Wi-Fi Settings"
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.touchTarget
            // Handler: no arguments/return; open popup whose visual parent is window overlay.
            onClicked: wifiDialog.open()
        }
    }

    WifiDialog {
        id: wifiDialog
        wifiController: root.wifiController
        inputPanel: root.inputPanel
    }
}
