import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import BierKistnRadio

Rectangle {
    id: root
    color: Theme.surfaceColor

    readonly property string btStatusText: {
        switch (PlaybackController.playbackState) {
        case PlaybackController.BluetoothActive:
            return "Connected to " + PlaybackController.bluetooth.connectedDeviceName
        case PlaybackController.BluetoothWaiting:
            return "Discoverable"
        default:
            return PlaybackController.bluetooth.adapterPowered ? "Not connected" : "Not available"
        }
    }

    readonly property string wifiStatusText: {
        if (WifiController.connected) {
            return "Connected to " + WifiController.ssid
        }
        return "Not connected"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.defaultSpacing
        spacing: Theme.defaultSpacing

        // ---------- Volume (left) | Brightness (right) ----------
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

            ColumnLayout {
                Layout.preferredWidth: Theme.touchTarget
                spacing: 2
                Layout.preferredHeight: 250

                Label {
                    text: "☀"
                    font.pixelSize: Theme.fontSizeMedium
                    color: Theme.textColor
                    Layout.alignment: Qt.AlignHCenter
                }
                Label {
                    text: brightnessSlider.value + "%"
                    font.pixelSize: Theme.fontSizeSmall
                    font.bold: true
                    color: Theme.textColor
                    Layout.alignment: Qt.AlignHCenter
                }
                Slider {
                    id: brightnessSlider
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: Theme.touchTarget
                Layout.preferredHeight: 250
                    orientation: Qt.Vertical
                    from: 0
                    to: 100
                    value: 70
                }
            }
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
                text: root.btStatusText
                font.pixelSize: Theme.fontSizeMedium
                color: Theme.textColor
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
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
                text: root.wifiStatusText
                font.pixelSize: Theme.fontSizeMedium
                color: Theme.textColor
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }
        }

        // ---------- Wifi Settings button ----------
        Button {
            text: "Wifi Settings"
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.touchTarget
            onClicked: wifiDialog.open()
        }
    }

    WifiDialog {
        id: wifiDialog
    }
}