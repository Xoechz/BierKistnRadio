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
    closePolicy: Popup.NoAutoClose
    anchors.centerIn: Overlay.overlay

    property var bluetoothClient: PlaybackController.bluetooth
    visible: root.bluetoothClient.takeoverPending

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
            text: "Keep playing on " + root.bluetoothClient.connectedDeviceName
                  + ", or switch to " + root.bluetoothClient.takeoverIncomingName + "?"
            font.pixelSize: Theme.fontSizeMedium
            color: Theme.secondaryTextColor
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Label {
            objectName: "takeoverCountdown"
            text: "Auto-selecting Keep Current in " + root.countdown + "s"
            font.pixelSize: Theme.fontSizeSmall
            color: Theme.secondaryTextColor
            visible: !root.bluetoothClient.takeoverResolving
                     && root.bluetoothClient.takeoverError === ""
        }

        Label {
            text: "Disconnecting device…"
            font.pixelSize: Theme.fontSizeSmall
            color: Theme.secondaryTextColor
            visible: root.bluetoothClient.takeoverResolving
        }

        Label {
            objectName: "takeoverError"
            text: root.bluetoothClient.takeoverError
            font.pixelSize: Theme.fontSizeSmall
            color: Theme.errorColor
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            visible: text !== ""
        }

        Item { Layout.fillHeight: true }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.defaultSpacing

            Button {
                objectName: "takeoverKeep"
                text: "Keep Current"
                enabled: !root.bluetoothClient.takeoverResolving
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.touchTarget
                onClicked: root.resolveTakeover(BluetoothClient.KeepCurrent)
            }
            Button {
                objectName: "takeoverSwitch"
                text: "Switch to " + root.bluetoothClient.takeoverIncomingName
                enabled: !root.bluetoothClient.takeoverResolving
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.touchTarget
                Material.background: Theme.primaryColor
                onClicked: root.resolveTakeover(BluetoothClient.SwitchToNew)
            }
        }
    }

    function resolveTakeover(choice) {
        root.bluetoothClient.resolveTakeover(choice)
    }

    onVisibleChanged: {
        if (root.visible) {
            root.countdown = 10
        }
    }

    Connections {
        target: root.bluetoothClient
        function onTakeoverResolvingChanged() {
            if (!root.bluetoothClient.takeoverResolving
                    && root.bluetoothClient.takeoverPending) {
                root.countdown = 10
            }
        }
    }

    Timer {
        id: countdownTimer
        interval: 1000
        repeat: true
        running: root.visible && !root.bluetoothClient.takeoverResolving
                 && root.bluetoothClient.takeoverError === ""
        onTriggered: {
            root.countdown -= 1
            if (root.countdown <= 0) {
                root.resolveTakeover(BluetoothClient.KeepCurrent)
            }
        }
    }
}
