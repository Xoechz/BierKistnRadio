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
            text: "Left Column"
            font.pixelSize: Theme.fontSizeLarge
            font.bold: true
            color: Theme.secondaryTextColor
            Layout.fillWidth: true
        }

        Label {
            text: "Metadata (T12)"
            font.pixelSize: Theme.fontSizeSmall
            color: Theme.secondaryTextColor
            Layout.fillWidth: true
        }

        Item { Layout.fillHeight: true }
    }
}