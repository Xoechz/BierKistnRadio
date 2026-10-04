import QtQuick
import QtQuick.Controls
import QtTest
import BierKistnRadio

TestCase {
    name: "PairingDialogTests"
    when: window.visible
    QtObject {
        id: fakeAgent
        property bool pending: false
        property string deviceName: "Test Phone"
        property string passkey: "000042"
        property int secondsRemaining: 30
        property int entered: -1
        property int requestId: 1
        property bool accepted: false
        property int calls: 0
        signal promptChanged()
        // id: displayed request ID; accept: decision. Returns undefined; assert/record,
        // then simulate backend prompt completion (no D-Bus or real pairing).
        function resolve(id, accept) {
            compare(id, requestId)
            accepted = accept
            calls++
            pending = false
            promptChanged()
        }
    }
    ApplicationWindow {
        id: window
        width: 1024
        height: 600
        visible: true
        PairingDialog { id: dialog; agent: fakeAgent }
    }
    // No arguments; returns undefined. Reset fake pending/decision/action count.
    function cleanup() {
        fakeAgent.pending = false
        fakeAgent.calls = 0
        fakeAgent.accepted = false
    }
    // No arguments; returns tagged button/expected-decision rows for accept/reject cases.
    function test_confirmation_data() {
        return [{ tag: "accept", button: "pairingConfirm", accept: true },
                { tag: "reject", button: "pairingReject", accept: false }]
    }
    // data: one row from test_confirmation_data. Returns undefined; assert padded identity,
    // touch geometry, single decision routing and close using fake backend state.
    function test_confirmation(data) {
        fakeAgent.pending = true
        fakeAgent.promptChanged()
        tryCompare(dialog, "visible", true)
        compare(findChild(dialog, "pairingPasskey").text, "000042")
        compare(findChild(dialog, "pairingDevice").text, "Test Phone")
        var button = findChild(dialog, data.button)
        verify(button.height >= Theme.touchTarget)
        // Predicate: no arguments; returns whether the chosen button is visible.
        tryVerify(function() { return button.visible })
        mouseClick(button)
        compare(fakeAgent.calls, 1)
        compare(fakeAgent.accepted, data.accept)
        tryCompare(dialog, "visible", false)
    }
    // No arguments; returns undefined. Assert dismissing a pending prompt explicitly rejects.
    function test_dialogCancellationRejects() {
        fakeAgent.pending = true
        fakeAgent.promptChanged()
        tryCompare(dialog, "visible", true)
        dialog.close()
        tryCompare(fakeAgent, "calls", 1)
        compare(fakeAgent.accepted, false)
    }
}
