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

        Label {
            text: "Settings"
            font.pixelSize: Theme.fontSizeXLarge
            font.bold: true
            color: Theme.textColor
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 200
            radius: Theme.cornerRadius
            color: Theme.surfaceColor

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.defaultSpacing
                spacing: 8

                Label {
                    text: "Wi-Fi"
                    font.pixelSize: Theme.fontSizeMedium
                    font.bold: true
                    color: Theme.textColor
                }

                Label {
                    text: WifiController.connected
                        ? "Connected: " + WifiController.ssid
                        : "Not connected"
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.secondaryTextColor
                }

                Button {
                    text: "Scan"
                    onClicked: WifiController.scan()
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 200
            radius: Theme.cornerRadius
            color: Theme.surfaceColor

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.defaultSpacing
                spacing: 8

                Label {
                    text: "Bluetooth"
                    font.pixelSize: Theme.fontSizeMedium
                    font.bold: true
                    color: Theme.textColor
                }

                Label {
                    text: BluetoothController.discoverable
                        ? "Discoverable"
                        : "Not discoverable"
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.secondaryTextColor
                }

                Switch {
                    text: "Discoverable"
                    checked: BluetoothController.discoverable
                    onToggled: BluetoothController.setDiscoverable(checked)
                }
            }
        }

        Item { Layout.fillHeight: true }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.defaultSpacing

            Button {
                text: "Brightness"
                Layout.fillWidth: true
            }

            Button {
                text: "Power Off"
                Layout.fillWidth: true
                Material.foreground: Theme.errorColor
            }
        }
    }
}
