import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BierKistnRadio

Popup {
    id: root
    property var agent: PlaybackController.bluetooth.pairing
    property var displayedRequestId: 0
    modal: true
    width: 480
    height: 340
    anchors.centerIn: Overlay.overlay
    closePolicy: Popup.NoAutoClose
    visible: agent.pending

    Connections {
        target: root.agent
        function onPromptChanged() {
            root.displayedRequestId = root.agent.requestId
        }
    }
    Component.onCompleted: displayedRequestId = agent.requestId
    onClosed: {
        if (agent.pending) {
            agent.resolve(displayedRequestId, false)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.defaultSpacing
        spacing: Theme.defaultSpacing
        Label {
            text: "Pair Bluetooth phone?"
            font.pixelSize: Theme.fontSizeXLarge
            font.bold: true
            color: Theme.textColor
        }
        Label {
            objectName: "pairingDevice"
            text: root.agent.deviceName
            font.pixelSize: Theme.fontSizeMedium
            color: Theme.textColor
            wrapMode: Text.Wrap
            maximumLineCount: 2
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
        Label {
            objectName: "pairingPasskey"
            text: root.agent.passkey
            font.pixelSize: Theme.fontSizeXLarge * 1.5
            font.bold: true
            color: Theme.textColor
            Layout.alignment: Qt.AlignHCenter
        }
        Label {
            text: "Confirm only if this code matches your phone."
            color: Theme.secondaryTextColor
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        Label {
            text: "Rejecting in " + root.agent.secondsRemaining + "s"
            color: Theme.secondaryTextColor
            font.pixelSize: Theme.fontSizeSmall
        }
        Item { Layout.fillHeight: true }
        RowLayout {
            Layout.fillWidth: true
            Button {
                objectName: "pairingReject"
                text: "Reject"
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.touchTarget
                onClicked: root.agent.resolve(root.displayedRequestId, false)
            }
            Button {
                objectName: "pairingConfirm"
                text: "Codes Match"
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.touchTarget
                onClicked: root.agent.resolve(root.displayedRequestId, true)
            }
        }
    }
}
