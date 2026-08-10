import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import BierKistnRadio

Rectangle {
    height: Theme.statusBarHeight
    color: Theme.surfaceColor

    property string activeSource: "Spotify"

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.defaultSpacing
        anchors.rightMargin: Theme.defaultSpacing
        spacing: Theme.defaultSpacing

        Label {
            id: clockLabel
            text: Qt.formatTime(new Date(), "HH:mm")
            font.pixelSize: Theme.fontSizeLarge
            font.bold: true
            color: Theme.textColor
            Layout.alignment: Qt.AlignVCenter
        }

        Item { Layout.fillWidth: true }

        Rectangle {
            width: 100
            height: 32
            radius: 16
            color: Theme.accentColor
            opacity: 0.2
            Layout.alignment: Qt.AlignVCenter

            Label {
                anchors.centerIn: parent
                text: activeSource
                color: Theme.accentColor
                font.pixelSize: Theme.fontSizeSmall
                font.bold: true
            }
        }

        Label {
            text: "📶"
            font.pixelSize: 20
            color: Theme.textColor
            Layout.alignment: Qt.AlignVCenter

            MouseArea {
                anchors.fill: parent
                onClicked: appRouter.showSettings("wifi")
            }
        }

        Label {
            text: "🔵"
            font.pixelSize: 20
            color: Theme.textColor
            Layout.alignment: Qt.AlignVCenter

            MouseArea {
                anchors.fill: parent
                onClicked: appRouter.showSettings("bluetooth")
            }
        }
    }

    Timer {
        interval: 1000
        running: true
        repeat: true
        onTriggered: clockLabel.text = Qt.formatTime(new Date(), "HH:mm")
    }
}
