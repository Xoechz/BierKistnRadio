import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import BierKistnRadio

Rectangle {
    id: root
    color: Theme.surfaceColor
    property var wifiController: WifiController
    property var playback: PlaybackController

    readonly property string btStatusText: {
        const bt = root.playback.bluetooth
        const name = bt.connectedDeviceName
        if (name !== "") {
            // Visible regardless of Source state (ADR 0008): you can always see
            // that a phone is connected, plus when it is silenced.
            return name + (bt.muted ? " · Muted" : "")
        }
        switch (root.playback.playbackState) {
        case PlaybackController.BluetoothWaiting:
            return "Discoverable"
        default:
            return bt.adapterPowered ? "Not connected" : "Not available"
        }
    }

    readonly property string wifiStatusText: {
        if (root.wifiController.connected) {
            return root.wifiController.ssid
        }
        return "Not connected"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.defaultSpacing
        spacing: Theme.defaultSpacing

        // ---------- Volume ----------
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 250
            spacing: Theme.defaultSpacing

            ColumnLayout {
                Layout.preferredWidth: Theme.touchTarget
                Layout.preferredHeight: 250
                spacing: 2

                Label {
                    text: "🔊"
                    font.pixelSize: Theme.fontSizeMedium
                    color: Theme.textColor
                    Layout.alignment: Qt.AlignHCenter
                }
                Label {
                    text: VolumeController.volume + "%"
                    font.pixelSize: Theme.fontSizeSmall
                    font.bold: true
                    color: Theme.textColor
                    Layout.alignment: Qt.AlignHCenter
                }
                Item {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: Theme.touchTarget
                    Layout.preferredHeight: 250

                    Slider {
                        id: volumeSlider
                        anchors.fill: parent
                        orientation: Qt.Vertical
                        from: 0
                        to: 150
                        value: VolumeController.volume
                        onMoved: VolumeController.setVolume(value)
                    }

                    Rectangle {
                        width: 24
                        height: 2
                        radius: 1
                        color: Theme.primaryColor
                        anchors.horizontalCenter: parent.horizontalCenter
                        y: (volumeSlider.height - volumeSlider.handle.height) *
                           (1 - 100 / 150) + volumeSlider.handle.height / 2
                    }
                }
            }

            Button {
                objectName: "volumeMuteButton"
                Layout.preferredWidth: Theme.touchTarget
                Layout.preferredHeight: Theme.touchTarget
                Layout.alignment: Qt.AlignVCenter
                text: VolumeController.muted ? "🔇" : "🔊"
                Accessible.name: VolumeController.muted ? "Unmute volume" : "Mute volume"
                onClicked: VolumeController.setMuted(!VolumeController.muted)
            }
        }

        Label {
            objectName: "volumeError"
            Layout.fillWidth: true
            text: VolumeController.errorMessage
            visible: text !== ""
            color: Theme.errorColor
            font.pixelSize: Theme.fontSizeSmall
            wrapMode: Text.WordWrap
        }

        // ---------- Dark mode ----------
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.defaultSpacing

            Label {
                text: Theme.darkMode ? "Dark" : "Light"
                font.pixelSize: Theme.fontSizeSmall
                font.bold: true
                color: Theme.textColor
            }
            Item { Layout.fillWidth: true }
            Switch {
                checked: Theme.darkMode
                onToggled: Theme.darkMode = checked
            }
        }

        Item { Layout.fillHeight: true }

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
            text: "Wifi Settings"
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.touchTarget
            onClicked: wifiDialog.open()
        }
    }

    WifiDialog {
        id: wifiDialog
        wifiController: root.wifiController
    }
}
