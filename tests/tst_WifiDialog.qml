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
            // No arguments; returns undefined. Count scans of predetermined fixture rows.
            function scan() {
                scans++
            }
            // ssid/password: submitted credentials. Returns undefined; record/count and mark
            // pending, deliberately without emitting successful activation.
            function connect(ssid, password) {
                requestedSsid = ssid
                requestedPassword = password
                connects++
                connecting = true
            }
        }

        // Reproduce in-app panel occlusion without platform keyboardRectangle.
        Rectangle {
            id: keyboardPanel
            parent: Overlay.overlay
            width: window.width
            height: 320
            y: window.height - height
            visible: false
            z: 1
            color: "black"
        }

        RightSidebar {
            id: sidebar
            x: window.width - width
            y: Theme.statusBarHeight
            width: 250
            height: window.height - y
            wifiController: wifi
            playback: fakePlayback
            inputPanel: keyboardPanel
        }

        TestCase {
            id: test
            name: "WifiDialogIntegration"
            when: window.visible
            // No arguments; returns named WifiDialog QObject (null if absent).
            function popup() {
                return findChild(sidebar, "wifiDialog")
            }
            // No arguments; returns undefined. Activate test window and reset Wi-Fi/keyboard space.
            function init() {
                window.requestActivate()
                wifi.connecting = false
                wifi.errorMessage = ""
                wifi.connects = 0
                wifi.scans = 0
                keyboardPanel.visible = false
                keyboardPanel.height = 320
            }
            // No arguments; returns undefined. Close/wait for popup and restore shared dark theme.
            function cleanup() {
                popup().close()
                tryCompare(popup(), "visible", false)
                Theme.darkMode = true
                keyboardPanel.visible = false
            }
            // item: visual QObject; height: allowed window-relative pixels. Returns undefined;
            // assert x/y/extent bounds with one-pixel tolerance, including keyboard-space cases.
            function onScreen(item, height) {
                var point = item.mapToItem(window.contentItem, 0, 0)
                verify(point.x >= 0 && point.x + item.width <= window.width + 1, item.objectName + " outside horizontal bounds: " + point.x)
                verify(point.y >= 0 && point.y + item.height <= height + 1, item.objectName + " outside vertical bounds: " + point.y)
            }
            // No arguments; returns undefined. Click actual sidebar button and assert overlay
            // parent, opened state and reachable content/Cancel rather than opening directly.
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
            // No arguments; returns undefined. Check repeated scanning/list/Cancel in both themes.
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
            // No arguments; returns theme/available-height rows for panel occlusion cases.
            function test_passwordKeyboardGeometryAndActivationResult_data() {
                return [
                    { tag: "dark-280", dark: true, top: 280 },
                    { tag: "light-280", dark: false, top: 280 },
                    { tag: "dark-250", dark: true, top: 250 },
                    { tag: "light-250", dark: false, top: 250 }
                ]
            }
            // data: theme/keyboard top. Returns undefined. Exercise sidebar-to-popup
            // panel geometry, reachable input/actions and matching-only activation close.
            function test_passwordKeyboardGeometryAndActivationResult(data) {
                Theme.darkMode = data.dark
                openFromButton()
                var list = findChild(popup(), "wifiNetworkList")
                // Predicate: no arguments; returns whether first network delegate exists yet.
                tryVerify(function () {
                    return list.itemAtIndex(0) !== null
                })
                mouseClick(list.itemAtIndex(0))
                compare(popup().selectedSsid, "Secured Test")
                var password = findChild(popup(), "wifiPasswordField")
                tryCompare(password, "activeFocus", true)
                keyboardPanel.height = window.height - data.top
                keyboardPanel.visible = true
                wifi.errorMessage = "Permission denied — check system config. This longer error must not hide password entry or connection actions."
                wait(50)
                compare(popup().keyboardTop, data.top)
                onScreen(password, data.top)
                onScreen(findChild(popup(), "wifiErrorLabel"), data.top)
                onScreen(findChild(popup(), "wifiConnectButton"), data.top)
                onScreen(findChild(popup(), "wifiCancelButton"), data.top)
                mouseClick(password)
                keyClick(Qt.Key_T)
                keyClick(Qt.Key_4)
                mouseClick(findChild(popup(), "wifiConnectButton"))
                compare(wifi.connects, 1)
                compare(wifi.requestedPassword, "t4")
                compare(popup().visible, true)
                // invocation is not connection success
                wifi.connecting = false
                wifi.errorMessage = "Wi-Fi connection failed"
                compare(popup().visible, true)
                compare(findChild(popup(), "wifiErrorLabel").visible, true)
                // Hiding/showing the panel restores/shrinks available space by binding.
                keyboardPanel.visible = false
                tryCompare(popup(), "keyboardTop", window.height)
                keyboardPanel.visible = true
                tryCompare(popup(), "keyboardTop", data.top)
                wifi.connectionSucceeded("Different network")
                compare(popup().visible, true)
                wifi.connectionSucceeded("Secured Test")
                tryCompare(popup(), "visible", false)
                compare(password.activeFocus, false)
            }
            // No arguments; returns undefined. Cancel remains clickable above the panel.
            function test_cancelWhileKeyboardOpen() {
                openFromButton()
                var list = findChild(popup(), "wifiNetworkList")
                tryVerify(function () { return list.itemAtIndex(0) !== null })
                mouseClick(list.itemAtIndex(0))
                var password = findChild(popup(), "wifiPasswordField")
                tryCompare(password, "activeFocus", true)
                keyboardPanel.visible = true
                wait(50)
                mouseClick(findChild(popup(), "wifiCancelButton"))
                tryCompare(popup(), "visible", false)
                compare(password.activeFocus, false)
            }
            // No arguments; returns undefined. Escape-dismiss and verify underlying reopen works.
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
