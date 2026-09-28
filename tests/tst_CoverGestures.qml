import QtQuick
import QtQuick.Controls
import QtTest
import BierKistnRadio

TestCase {
    name: "CoverGestures"
    when: window.visible

    QtObject {
        id: spotify
        property string artUrl: ""
        property real position: 0
        property real duration: 120000
        property bool isSpotifyPlaying: false
    }

    QtObject {
        id: bluetooth
        property bool positionPublished: false
        property bool statusPublished: false
        property bool isBluetoothPlaying: false
        property real position: 0
        property real duration: 0
    }

    QtObject {
        id: facade
        property int playbackState: PlaybackController.SpotifyWaiting
        property QtObject spotify: spotify
        property QtObject bluetooth: bluetooth
        property int plays: 0
        property int pauses: 0
        property int nexts: 0
        property int previouses: 0
        function play() { plays++ }
        function pause() { pauses++ }
        function next() { nexts++ }
        function previous() { previouses++ }
    }

    ApplicationWindow {
        id: window
        width: 1024
        height: 600
        visible: true

        CenterColumn {
            id: center
            anchors.fill: parent
            playback: facade
        }
    }

    function init() {
        facade.playbackState = PlaybackController.SpotifyWaiting
        bluetooth.statusPublished = false
        bluetooth.isBluetoothPlaying = false
        spotify.isSpotifyPlaying = false
        facade.plays = 0
        facade.pauses = 0
        facade.nexts = 0
        facade.previouses = 0
    }

    function cover() {
        var area = findChild(center, "coverGesture")
        verify(area !== null)
        return area
    }

    function swipe(area, fromX, toX, toY) {
        mousePress(area, fromX, 180)
        mouseMove(area, toX, toY)
        mouseRelease(area, toX, toY)
    }

    function test_spotifyGestures() {
        facade.playbackState = PlaybackController.SpotifyActive
        var area = cover()
        mouseClick(area, 180, 180)
        compare(facade.plays, 1)
        spotify.isSpotifyPlaying = true
        mouseClick(area, 180, 180)
        compare(facade.pauses, 1)

        swipe(area, 210, 90, 180)
        compare(facade.nexts, 1)
        compare(facade.plays, 1) // swipe must not also tap
        compare(facade.pauses, 1)
        swipe(area, 90, 210, 180)
        compare(facade.previouses, 1)

        swipe(area, 180, 195, 280) // vertical motion is not a skip
        compare(facade.nexts, 1)
        compare(facade.previouses, 1)
        compare(facade.pauses, 1)
    }

    function test_bluetoothAvailability() {
        var area = cover()
        facade.playbackState = PlaybackController.BluetoothActive
        compare(area.enabled, false)
        mouseClick(area, 180, 180)
        compare(facade.plays, 0)

        bluetooth.statusPublished = true
        compare(area.enabled, true)
        mouseClick(area, 180, 180)
        compare(facade.plays, 1)
        bluetooth.isBluetoothPlaying = true
        mouseClick(area, 180, 180)
        compare(facade.pauses, 1)
        swipe(area, 210, 90, 180)
        compare(facade.nexts, 1)

        facade.playbackState = PlaybackController.BluetoothWaiting
        compare(area.enabled, false)
    }

    function test_transportIconsAtPanelSize() {
        facade.playbackState = PlaybackController.SpotifyActive
        var previous = findChild(center, "previousButton")
        var toggle = findChild(center, "playPauseButton")
        var next = findChild(center, "nextButton")
        verify(previous !== null && toggle !== null && next !== null)
        compare(window.width, 1024)
        compare(window.height, 600)

        for (var dark of [true, false]) {
            Theme.darkMode = dark
            for (var button of [previous, toggle, next]) {
                compare(button.width, Theme.touchTargetLarge)
                compare(button.height, Theme.touchTargetLarge)
                compare(button.display, AbstractButton.IconOnly)
                compare(button.icon.width, 28)
                compare(button.icon.height, 28)
                compare(button.icon.color, Theme.textColor)
                verify(button.icon.source.toString().endsWith(".svg"))
            }
            compare(previous.y, toggle.y)
            compare(toggle.y, next.y)
            compare(toggle.x - previous.x, next.x - toggle.x)
            verify(toggle.icon.source.toString().endsWith("/play.svg"))
            spotify.isSpotifyPlaying = true
            verify(toggle.icon.source.toString().endsWith("/pause.svg"))
            spotify.isSpotifyPlaying = false
        }
        Theme.darkMode = true
        mouseClick(previous)
        mouseClick(toggle)
        mouseClick(next)
        compare(facade.previouses, 1)
        compare(facade.plays, 1)
        compare(facade.nexts, 1)
    }
}
