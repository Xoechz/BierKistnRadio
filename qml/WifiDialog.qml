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
    // The in-app InputPanel shares the overlay parent. Its actual geometry remains
    // available even when the platform input method reports an empty rectangle.
    property var inputPanel: null
    readonly property real keyboardTop: inputPanel && inputPanel.visible
                                       ? Math.max(0, Math.min(parent.height, inputPanel.y))
                                       : parent.height
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

    // Handler: no arguments/return; retain in-flight selection, otherwise reset then scan.
    onOpened: {
        if (!root.wifiController.connecting) {
            root.selectedSsid = ""
            root.selectedSecured = false
            root.password = ""
        }
        root.wifiController.scan()
    }
    // Handler: no arguments/return; dismiss input focus/keyboard, not an issued NM activation.
    onClosed: {
        passwordField.focus = false
        Qt.inputMethod.hide()
    }

    // strength: signal percentage. Returns one bar glyph at <25/<50/<75/otherwise thresholds.
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

    // No arguments; returns undefined. Pass selected SSID/password; wait for observed success.
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
                // Handler: no arguments/return; request fresh inventory/scan.
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

        TextField {
            id: passwordField
            objectName: "wifiPasswordField"
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.touchTarget
            visible: root.selectedSsid !== "" && root.selectedSecured
            placeholderText: "Password for " + root.selectedSsid
            echoMode: TextInput.Password
            text: root.password
            // Handler: no arguments/return; keep the masked field's edited value in the popup.
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
                // Handler: no arguments/return; close UI without cancelling an issued activation.
                onClicked: root.close()
            }
            Item { Layout.fillWidth: true }
            Button {
                objectName: "wifiConnectButton"
                text: "Connect"
                enabled: root.selectedSsid !== "" && !root.wifiController.connecting
                Layout.preferredHeight: Theme.touchTarget
                // Handler: no arguments/return; start selected activation, not close-on-call.
                onClicked: root.connectSelected()
            }
        }

        // Network inventory yields space to credentials/actions while typing.
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

                // Handler: no arguments/return; modelData supplies SSID/security.
                // Open networks activate immediately; secured fields receive deferred focus.
                onClicked: {
                    root.selectedSsid = modelData.ssid
                    root.selectedSecured = modelData.secured
                    root.password = ""
                    if (!modelData.secured && !root.wifiController.connecting) {
                        root.connectSelected()
                    } else if (modelData.secured) {
                        const ssid = modelData.ssid
                        // Callback: no arguments; returns undefined. Captured SSID must still
                        // match a visible secured selection before showing the keyboard.
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

    }

    Connections {
        target: root.wifiController
        // ssid: actually activated network. Returns undefined; only matching selection closes.
        function onConnectionSucceeded(ssid) {
            if (root.visible && ssid === root.selectedSsid) {
                root.close()
            }
        }
    }
}
