import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import BierKistnRadio

Popup {
    id: root
    modal: true
    width: 480
    height: 400
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: Overlay.overlay

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.defaultSpacing
        spacing: Theme.defaultSpacing

        Label {
            text: "Wifi Settings"
            font.pixelSize: Theme.fontSizeLarge
            font.bold: true
            color: Theme.textColor
        }

        Item { Layout.fillHeight: true }

        Label {
            text: "SSID list with password entry lands in T17"
            font.pixelSize: Theme.fontSizeSmall
            color: Theme.secondaryTextColor
        }

        Button {
            text: "Close"
            Layout.alignment: Qt.AlignRight
            Layout.preferredHeight: Theme.touchTarget
            onClicked: root.close()
        }
    }
}