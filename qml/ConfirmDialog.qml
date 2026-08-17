import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import BierKistnRadio

Dialog {
    id: root
    modal: true
    width: 360
    height: 220
    standardButtons: Dialog.NoButton
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: Overlay.overlay

    property string action: "reboot"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.defaultSpacing
        spacing: Theme.defaultSpacing

        Label {
            text: root.action === "shutdown" ? "Shutdown Radio?" : "Reboot Radio?"
            font.pixelSize: Theme.fontSizeLarge
            font.bold: true
            color: Theme.textColor
        }

        Item { Layout.fillHeight: true }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.defaultSpacing

            Button {
                text: "Cancel"
                flat: true
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.touchTarget
                onClicked: root.close()
            }
            Button {
                text: "Confirm"
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.touchTarget
                Material.background: Theme.errorColor
                onClicked: root.close()
            }
        }
    }

    function openFor(act) {
        root.action = act
        root.open()
    }
}