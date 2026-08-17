import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import BierKistnRadio

Popup {
    id: root
    modal: true
    width: 520
    height: 480
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: Overlay.overlay

    property string selectedSsid: ""
    property string password: ""

    onOpened: {
        root.selectedSsid = ""
        root.password = ""
        WifiController.scan()
    }

    function strengthBars(strength) {
        if (strength < 25) {
            return "▂"
        }
        if (strength < 50) {
            return "▄"
        }
        if (strength < 75) {
            return "▆"
        }
        return "█"
    }

    function connectSelected() {
        WifiController.connect(root.selectedSsid, root.password)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.defaultSpacing
        spacing: Theme.defaultSpacing

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.defaultSpacing

            Label {
                text: "Wifi Settings"
                font.pixelSize: Theme.fontSizeLarge
                font.bold: true
                color: Theme.textColor
                Layout.fillWidth: true
            }
            Button {
                text: "⟳"
                flat: true
                onClicked: WifiController.scan()
            }
        }

        Label {
            text: WifiController.errorMessage
            visible: WifiController.errorMessage !== ""
            color: Theme.errorColor
            font.pixelSize: Theme.fontSizeSmall
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        ListView {
            id: ssidList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: WifiController.networks
            delegate: ItemDelegate {
                required property var modelData
                width: ssidList.width
                height: Theme.touchTarget
                highlighted: root.selectedSsid === modelData.ssid

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.defaultSpacing
                    anchors.rightMargin: Theme.defaultSpacing
                    spacing: Theme.defaultSpacing

                    Label {
                        text: modelData.ssid
                        font.pixelSize: Theme.fontSizeMedium
                        color: Theme.textColor
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Label {
                        text: WifiController.connected && modelData.ssid === WifiController.ssid ? "✓" : ""
                        font.pixelSize: Theme.fontSizeMedium
                        color: Theme.accentColor
                    }
                    Label {
                        text: root.strengthBars(modelData.signalStrength)
                        font.pixelSize: Theme.fontSizeMedium
                        color: Theme.secondaryTextColor
                    }
                }

                onClicked: {
                    root.selectedSsid = modelData.ssid
                    root.password = ""
                    if (!modelData.secured) {
                        root.connectSelected()
                    }
                }
            }
        }

        TextField {
            id: passwordField
            Layout.fillWidth: true
            visible: root.selectedSsid !== ""
            placeholderText: "Password for " + root.selectedSsid
            echoMode: TextInput.Password
            text: root.password
            onTextChanged: root.password = text
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.defaultSpacing

            Button {
                text: "Cancel"
                flat: true
                Layout.preferredHeight: Theme.touchTarget
                onClicked: root.close()
            }
            Item { Layout.fillWidth: true }
            Button {
                text: "Connect"
                enabled: root.selectedSsid !== ""
                Layout.preferredHeight: Theme.touchTarget
                onClicked: root.connectSelected()
            }
        }
    }

    Connections {
        target: WifiController
        function onConnectedChanged() {
            if (WifiController.connected) {
                root.close()
            }
        }
    }
}