import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import BierKistnRadio

Page {
    background: Rectangle { color: Theme.backgroundColor }

    signal sourceSelected(string source)

    GridLayout {
        anchors.fill: parent
        anchors.margins: Theme.defaultSpacing
        columns: 2
        rowSpacing: Theme.defaultSpacing
        columnSpacing: Theme.defaultSpacing

        Repeater {
            model: [
                { name: "Spotify", icon: "🎵", desc: "Spotify Connect" },
                { name: "Radio", icon: "📻", desc: "Internet Radio" },
                { name: "Bluetooth", icon: "🔵", desc: "Bluetooth Sink" }
            ]

            Rectangle {
                required property var modelData
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: Theme.cornerRadius
                color: Theme.surfaceColor
                border.width: 2
                border.color: mouseArea.containsMouse ? Theme.accentColor : "transparent"

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 8

                    Label {
                        text: modelData.icon
                        font.pixelSize: 64
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Label {
                        text: modelData.name
                        font.pixelSize: Theme.fontSizeLarge
                        font.bold: true
                        color: Theme.textColor
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Label {
                        text: modelData.desc
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.secondaryTextColor
                        Layout.alignment: Qt.AlignHCenter
                    }
                }

                MouseArea {
                    id: mouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: sourceSelected(modelData.name)
                }
            }
        }
    }
}
