import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import BierKistnRadio

Dialog {
    id: root
    modal: true
    width: 380
    height: 280
    standardButtons: Dialog.NoButton
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: Overlay.overlay

    property string action: "reboot"
    property int countdown: 10

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.defaultSpacing
        spacing: Theme.defaultSpacing

        Label {
            text: root.action === "shutdown" ? "Shutdown Radio?" : "Reboot Radio?"
            font.pixelSize: Theme.fontSizeXLarge
            font.bold: true
            color: Theme.textColor
        }

        Label {
            text: root.action === "shutdown"
                ? "The radio will power off."
                : "The radio will restart."
            font.pixelSize: Theme.fontSizeMedium
            color: Theme.secondaryTextColor
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Label {
            text: "Auto-dismissing in " + root.countdown + "s"
            visible: !PowerController.busy && PowerController.errorMessage === ""
            font.pixelSize: Theme.fontSizeSmall
            color: Theme.secondaryTextColor
        }

        Label {
            objectName: "powerError"
            Layout.fillWidth: true
            text: PowerController.errorMessage
            visible: text !== ""
            wrapMode: Text.WordWrap
            font.pixelSize: Theme.fontSizeSmall
            color: Theme.errorColor
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
                enabled: !PowerController.busy
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.touchTarget
                Material.background: Theme.errorColor
                onClicked: {
                    if (root.action === "shutdown") {
                        PowerController.shutdown()
                    } else {
                        PowerController.reboot()
                    }
                }
            }
        }
    }

    function openFor(act) {
        PowerController.clearError()
        root.action = act
        root.countdown = 10
        root.open()
    }

    Timer {
        id: autoDismiss
        interval: 1000
        repeat: true
        running: root.visible && !PowerController.busy && PowerController.errorMessage === ""
        onTriggered: {
            root.countdown -= 1
            if (root.countdown <= 0) {
                root.close()
            }
        }
    }

    Connections {
        target: PowerController
        function onCommandSucceeded() { root.close() }
    }
}
