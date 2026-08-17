import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BierKistnRadio

Rectangle {
    color: Theme.backgroundColor

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.defaultSpacing
        spacing: Theme.defaultSpacing

        Label {
            text: "Center Column"
            font.pixelSize: Theme.fontSizeLarge
            font.bold: true
            color: Theme.secondaryTextColor
            Layout.alignment: Qt.AlignHCenter
        }

        Label {
            text: "Album art / progress / transport (T13-T15)"
            font.pixelSize: Theme.fontSizeSmall
            color: Theme.secondaryTextColor
            Layout.alignment: Qt.AlignHCenter
        }

        Item { Layout.fillHeight: true }
    }
}