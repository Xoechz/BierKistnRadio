import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtTest
import BierKistnRadio

Item {
    QtObject {
        id: fakePlayback
        property bool switching: false
        property int playbackState: PlaybackController.SpotifyWaiting
        property string sourceError: ""
    }
    ApplicationWindow {
        id: window
        width: 1024
        height: 600
        visible: true
        Material.theme: Theme.materialTheme
        property int clicks: 0
        Button {
            id: underlyingButton
            x: 20
            y: 20
            text: "Underlying touch UI"
            // Handler: no arguments/return; count any click that incorrectly passed through modal.
            onClicked: window.clicks++
        }
        SourceTransitionOverlay {
            id: loading
            playback: fakePlayback
        }
        TestCase {
            name: "SourceTransitionOverlayTests"
            when: window.visible
            // No arguments; returns undefined. Reset transition/error/theme and wait for hide.
            function cleanup() {
                fakePlayback.switching = false
                fakePlayback.sourceError = ""
                fakePlayback.playbackState = PlaybackController.SpotifyWaiting
                window.clicks = 0
                Theme.darkMode = true
                tryCompare(loading, "visible", false)
            }
            // No arguments; returns undefined. Assert full extent, spinner, blocked input,
            // backend-only timeout policy and input recovery in both themes.
            function test_slowStartupBlocksWholeWindow() {
                for (var dark of [true, false]) {
                    const previousClicks = window.clicks
                    Theme.darkMode = dark
                    fakePlayback.playbackState = PlaybackController.BluetoothWaiting
                    fakePlayback.switching = true
                    tryCompare(loading, "opened", true)
                    compare(loading.width, 1024)
                    compare(loading.height, 600)
                    compare(loading.closePolicy, Popup.NoAutoClose)
                    compare(findChild(loading, "sourceLoadingIndicator").running, true)
                    verify(findChild(loading, "sourceLoadingLabel").text.indexOf("Bluetooth") >= 0)
                    mouseClick(underlyingButton)
                    keyClick(Qt.Key_Escape)
                    compare(window.clicks, previousClicks)
                    compare(loading.visible, true)
                    wait(350)
                    compare(loading.visible, true)
                    // no UI-owned premature timeout
                    fakePlayback.switching = false
                    tryCompare(loading, "visible", false)
                    mouseClick(underlyingButton)
                    compare(window.clicks, previousClicks + 1)
                }
            }
            // No arguments; returns undefined. Assert failure hide, retained waiting source,
            // no loader on passive recovery, then explicit retry/show and ready/hide.
            function test_failureRetryAndLateRecovery() {
                fakePlayback.switching = true
                tryCompare(loading, "opened", true)
                fakePlayback.sourceError = "Spotify startup timed out — retry"
                fakePlayback.switching = false
                tryCompare(loading, "visible", false)
                compare(fakePlayback.playbackState, PlaybackController.SpotifyWaiting)
                // Passive late readiness/error clearing must not display another loader.
                fakePlayback.sourceError = ""
                wait(100)
                compare(loading.visible, false)
                fakePlayback.switching = true
                // explicit backend retry
                tryCompare(loading, "opened", true)
                fakePlayback.switching = false
                tryCompare(loading, "visible", false)
                // A ready service without phone/MPRIS stays on the waiting hint.
                compare(fakePlayback.playbackState, PlaybackController.SpotifyWaiting)
            }
        }
    }
}
