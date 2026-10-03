import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import BierKistnRadio

Popup {
    id: root
    objectName: "wifiDialog"
    parent: Overlay.overlay
    property var wifiController: WifiController
    // Input-method coordinates are window-relative, just like this parent.
    // Keeping this boundary explicit also allows testing compact keyboard space.
    property real keyboardTop: Qt.inputMethod.visible && Qt.inputMethod.keyboardRectangle.y > 0
                               ? Qt.inputMethod.keyboardRectangle.y : parent.height
    readonly property bool compact: keyboardTop < 400
    modal: true
    focus: true
    width: Math.min(520, parent.width)
    height: Math.min(480, keyboardTop)
    padding: Theme.smallSpacing
    closePolicy: Popup.CloseOnEscape
    x: (parent.width - width) / 2
    y: (keyboardTop - height) / 2

    property string selectedSsid: ""
    property bool selectedSecured: false
    property string password: ""

    onOpened: {
        if (!root.wifiController.connecting) {
            root.selectedSsid = ""
            root.selectedSecured = false
            root.password = ""
        }
        root.wifiController.scan()
    }
    onClosed: {
        passwordField.focus = false
        Qt.inputMethod.hide()
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
        root.wifiController.connect(root.selectedSsid, root.password)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: root.compact ? Theme.smallSpacing : Theme.defaultSpacing
        spacing: root.compact ? Theme.smallSpacing : Theme.defaultSpacing

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
                objectName: "wifiRefreshButton"
                text: "⟳"
                flat: true
                Layout.preferredWidth: Theme.touchTarget
                Layout.preferredHeight: Theme.touchTarget
                onClicked: root.wifiController.scan()
            }
        }

        Label {
            objectName: "wifiErrorLabel"
            text: root.wifiController.errorMessage
            visible: text !== ""
            color: Theme.errorColor
            font.pixelSize: Theme.fontSizeSmall
            wrapMode: Text.WordWrap
            maximumLineCount: root.compact ? 2 : 3
            elide: Text.ElideRight
            Layout.fillWidth: true
        }

        Label {
            text: "Connecting to " + root.selectedSsid + "…"
            visible: root.wifiController.connecting
            color: Theme.secondaryTextColor
            font.pixelSize: Theme.fontSizeSmall
            Layout.fillWidth: true
        }

        ListView {
            id: ssidList
            objectName: "wifiNetworkList"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            Layout.minimumHeight: 0
            model: root.wifiController.networks
            delegate: ItemDelegate {
                required property var modelData
                enabled: !root.wifiController.connecting
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
                        text: root.wifiController.connected && modelData.ssid === root.wifiController.ssid ? "✓" : ""
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
                    root.selectedSecured = modelData.secured
                    root.password = ""
                    if (!modelData.secured && !root.wifiController.connecting) {
                        root.connectSelected()
                    } else if (modelData.secured) {
                        const ssid = modelData.ssid
                        Qt.callLater(function() {
                            if (root.visible && root.selectedSecured && root.selectedSsid === ssid) {
                                passwordField.forceActiveFocus()
                                Qt.inputMethod.show()
                            }
                        })
                    }
                }
            }
        }

        TextField {
            id: passwordField
            objectName: "wifiPasswordField"
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.touchTarget
            visible: root.selectedSsid !== "" && root.selectedSecured
            placeholderText: "Password for " + root.selectedSsid
            echoMode: TextInput.Password
            text: root.password
            onTextChanged: root.password = text
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.defaultSpacing

            Button {
                objectName: "wifiCancelButton"
                text: "Cancel"
                flat: true
                Layout.preferredHeight: Theme.touchTarget
                onClicked: root.close()
            }
            Item { Layout.fillWidth: true }
            Button {
                objectName: "wifiConnectButton"
                text: "Connect"
                enabled: root.selectedSsid !== "" && !root.wifiController.connecting
                Layout.preferredHeight: Theme.touchTarget
                onClicked: root.connectSelected()
            }
        }
    }

    Connections {
        target: root.wifiController
        function onConnectionSucceeded(ssid) {
            if (root.visible && ssid === root.selectedSsid) {
                root.close()
            }
        }
    }
}
