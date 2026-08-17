import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import BierKistnRadio

Popup {
    id: root
    modal: true
    width: 480
    height: 280
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: Overlay.overlay

    visible: PlaybackController.bluetooth.takeoverPending

    property int countdown: 10

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.defaultSpacing
        spacing: Theme.defaultSpacing

        Label {
            text: "Takeover"
            font.pixelSize: Theme.fontSizeXLarge
            font.bold: true
            color: Theme.textColor
        }

        Label {
            text: "Keep playing on " + PlaybackController.bluetooth.connectedDeviceName
                  + ", or switch to " + PlaybackController.bluetooth.takeoverIncomingName + "?"
            font.pixelSize: Theme.fontSizeMedium
            color: Theme.secondaryTextColor
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Label {
            text: "Auto-selecting Keep Current in " + root.countdown + "s"
            font.pixelSize: Theme.fontSizeSmall
            color: Theme.secondaryTextColor
        }

        Item { Layout.fillHeight: true }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.defaultSpacing

            Button {
                text: "Keep Current"
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.touchTarget
                onClicked: root.resolveTakeover(BluetoothClient.KeepCurrent)
            }
            Button {
                text: "Switch to " + PlaybackController.bluetooth.takeoverIncomingName
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.touchTarget
                Material.background: Theme.primaryColor
                onClicked: root.resolveTakeover(BluetoothClient.SwitchToNew)
            }
        }
    }

    function resolveTakeover(choice) {
        PlaybackController.bluetooth.resolveTakeover(choice)
    }

    onVisibleChanged: {
        if (root.visible) {
            root.countdown = 10
        }
    }

    Timer {
        id: countdownTimer
        interval: 1000
        repeat: true
        running: root.visible
        onTriggered: {
            root.countdown -= 1
            if (root.countdown <= 0) {
                root.resolveTakeover(BluetoothClient.KeepCurrent)
            }
        }
    }
}