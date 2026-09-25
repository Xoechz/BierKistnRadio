import QtQuick
import QtQuick.Controls
import QtTest
import BierKistnRadio

TestCase {
    name: "TakeoverDialogTests"
    when: window.visible

    QtObject {
        id: fakeBluetooth
        property bool takeoverPending: false
        property bool takeoverResolving: false
        property string takeoverError: ""
        property string connectedDeviceName: "Current Phone"
        property string takeoverIncomingName: "New Phone"
        property int calls: 0
        property int lastChoice: -1

        function resolveTakeover(choice) {
            calls += 1
            lastChoice = choice
            takeoverResolving = true
        }
    }

    ApplicationWindow {
        id: window
        width: 1024
        height: 600
        visible: true

        TakeoverDialog {
            id: dialog
            bluetoothClient: fakeBluetooth
        }
    }

    function cleanup() {
        fakeBluetooth.takeoverPending = false
        fakeBluetooth.takeoverResolving = false
        fakeBluetooth.takeoverError = ""
        fakeBluetooth.calls = 0
        fakeBluetooth.lastChoice = -1
    }

    function test_pendingFailureAndRetry() {
        fakeBluetooth.takeoverPending = true
        tryCompare(dialog, "visible", true)
        var keep = findChild(dialog, "takeoverKeep")
        var switchButton = findChild(dialog, "takeoverSwitch")
        var countdown = findChild(dialog, "takeoverCountdown")
        var errorLabel = findChild(dialog, "takeoverError")
        verify(keep !== null)
        verify(switchButton !== null)
        compare(dialog.closePolicy, Popup.NoAutoClose)
        compare(countdown.visible, true)

        fakeBluetooth.takeoverResolving = true
        compare(keep.enabled, false)
        compare(switchButton.enabled, false)
        compare(countdown.visible, false)

        fakeBluetooth.takeoverError = "Disconnect failed — try again"
        fakeBluetooth.takeoverResolving = false
        compare(dialog.visible, true)
        compare(errorLabel.visible, true)
        compare(keep.enabled, true)
        compare(switchButton.enabled, true)
        compare(countdown.visible, false)
        wait(1100)
        compare(fakeBluetooth.calls, 0) // a failed disconnect must not auto-retry

        dialog.resolveTakeover(BluetoothClient.KeepCurrent)
        compare(fakeBluetooth.calls, 1)
        compare(fakeBluetooth.lastChoice, BluetoothClient.KeepCurrent)
        fakeBluetooth.takeoverError = ""
        fakeBluetooth.takeoverPending = false
        tryCompare(dialog, "visible", false)
    }
}
