import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtTest
import BierKistnRadio

Item {
    ApplicationWindow {
        id: window
        width: 1024
        height: 600
        visible: true
        Material.theme: Theme.materialTheme

        QtObject {
            id: bluetooth
            property string connectedDeviceName: ""
            property bool adapterPowered: false
            property bool muted: true
            property string errorMessage: ""
        }
        QtObject {
            id: fakePlayback
            property int playbackState: PlaybackController.SpotifyWaiting
            property QtObject bluetooth: bluetooth
        }

        QtObject {
            id: wifi
            property bool connected: false
            property bool connecting: false
            property string ssid: ""
            property string errorMessage: ""
            property var networks: [
                {
                    ssid: "Secured Test",
                    signalStrength: 75,
                    secured: true
                },
                {
                    ssid: "Open Test",
                    signalStrength: 40,
                    secured: false
                }
            ]
            property int scans: 0
            property int connects: 0
            property string requestedSsid: ""
            property string requestedPassword: ""
            signal connectionSucceeded(string ssid)
            function scan() {
                scans++
            }
            function connect(ssid, password) {
                requestedSsid = ssid
                requestedPassword = password
                connects++
                connecting = true
            }
        }

        RightSidebar {
            id: sidebar
            x: window.width - width
            y: Theme.statusBarHeight
            width: 250
            height: window.height - y
            wifiController: wifi
            playback: fakePlayback
        }

        TestCase {
            id: test
            name: "WifiDialogIntegration"
            when: window.visible
            function popup() {
                return findChild(sidebar, "wifiDialog")
            }
            function init() {
                window.requestActivate()
                wifi.connecting = false
                wifi.errorMessage = ""
                wifi.connects = 0
                wifi.scans = 0
                popup().keyboardTop = window.height
            }
            function cleanup() {
                popup().close()
                tryCompare(popup(), "visible", false)
                Theme.darkMode = true
            }
            function onScreen(item, height) {
                var point = item.mapToItem(window.contentItem, 0, 0)
                verify(point.x >= 0 && point.x + item.width <= window.width + 1, item.objectName + " outside horizontal bounds: " + point.x)
                verify(point.y >= 0 && point.y + item.height <= height + 1, item.objectName + " outside vertical bounds: " + point.y)
            }
            function openFromButton() {
                var button = findChild(sidebar, "wifiSettingsButton")
                verify(button !== null)
                onScreen(button, window.height)
                mouseClick(button)
                tryCompare(popup(), "opened", true)
                compare(popup().parent, sidebar.Overlay.overlay)
                onScreen(popup().contentItem, window.height)
                onScreen(findChild(popup(), "wifiCancelButton"), window.height)
            }
            function test_actualButtonAndRepeatedCloseBothThemes() {
                for (var dark of [true, false]) {
                    Theme.darkMode = dark
                    for (var i = 0; i < 3; ++i) {
                        openFromButton()
                        compare(wifi.scans, (dark ? 0 : 3) + i + 1)
                        var list = findChild(popup(), "wifiNetworkList")
                        compare(list.count, 2)
                        verify(list.height > 0)
                        mouseClick(findChild(popup(), "wifiCancelButton"))
                        tryCompare(popup(), "visible", false)
                    }
                }
            }
            function test_passwordKeyboardGeometryAndActivationResult() {
                openFromButton()
                var list = findChild(popup(), "wifiNetworkList")
                tryVerify(function () {
                    return list.itemAtIndex(0) !== null
                })
                mouseClick(list.itemAtIndex(0))
                compare(popup().selectedSsid, "Secured Test")
                var password = findChild(popup(), "wifiPasswordField")
                tryCompare(password, "activeFocus", true)
                popup().keyboardTop = 280
                wifi.errorMessage = "Permission denied — check system config"
                wait(50)
                onScreen(password, 280)
                onScreen(findChild(popup(), "wifiConnectButton"), 280)
                onScreen(findChild(popup(), "wifiCancelButton"), 280)
                password.text = "test-password"
                mouseClick(findChild(popup(), "wifiConnectButton"))
                compare(wifi.connects, 1)
                compare(wifi.requestedPassword, "test-password")
                compare(popup().visible, true)
                // invocation is not connection success
                wifi.connecting = false
                wifi.errorMessage = "Wi-Fi connection failed"
                compare(popup().visible, true)
                compare(findChild(popup(), "wifiErrorLabel").visible, true)
                wifi.connectionSucceeded("Different network")
                compare(popup().visible, true)
                wifi.connectionSucceeded("Secured Test")
                tryCompare(popup(), "visible", false)
                compare(password.activeFocus, false)
            }
            function test_escapeClearsModalInputBlock() {
                openFromButton()
                keyClick(Qt.Key_Escape)
                tryCompare(popup(), "visible", false)
                openFromButton()
                // underlying button must accept input again
            }
        }
    }
}
