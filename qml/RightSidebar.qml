import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BierKistnRadio

Rectangle {
    color: Theme.surfaceColor

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.defaultSpacing
        spacing: Theme.defaultSpacing

        Label {
            text: "Right Sidebar"
            font.pixelSize: Theme.fontSizeLarge
            font.bold: true
            color: Theme.secondaryTextColor
            Layout.fillWidth: true
        }

        Label {
            text: "Brightness / volume / statuses (T11)"
            font.pixelSize: Theme.fontSizeSmall
            color: Theme.secondaryTextColor
            Layout.fillWidth: true
        }

        Item { Layout.fillHeight: true }
    }
}