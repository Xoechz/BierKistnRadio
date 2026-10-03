import QtQuick
import QtQuick.Controls
import QtTest
import BierKistnRadio

TestCase {
    name: "LeftColumnTitles"
    when: window.visible

    QtObject {
        id: spotify
        property string title: ""
        property string artist: "Artist"
        property string album: "Album"
        property string errorMessage: ""
    }

    QtObject {
        id: bluetooth
        property string connectedDeviceName: "Phone"
        property bool trackPublished: true
        property string trackTitle: ""
        property string trackArtist: "Artist"
        property string trackAlbum: "Album"
        property string errorMessage: ""
    }

    QtObject {
        id: facade
        property int playbackState: PlaybackController.SpotifyActive
        property QtObject spotify: spotify
        property QtObject bluetooth: bluetooth
        property string releaseDate: "2024-01-01"
        property string sourceError: ""
        property bool switching: false
        property int retries: 0
        function retrySource() { retries++ }
    }

    ApplicationWindow {
        id: window
        width: 1024
        height: 600
        visible: true

        LeftColumn {
            id: leftColumn
            width: 250
            height: parent.height - Theme.statusBarHeight
            playback: facade
        }
    }

    function init() {
        Theme.darkMode = true
        facade.playbackState = PlaybackController.SpotifyActive
        spotify.title = ""
        bluetooth.trackPublished = true
        bluetooth.trackTitle = ""
        bluetooth.connectedDeviceName = "Phone"
        spotify.errorMessage = ""
        bluetooth.errorMessage = ""
        facade.sourceError = ""
        facade.switching = false
        facade.retries = 0
    }

    function cleanup() {
        Theme.darkMode = true
    }

    function test_sourceTransitionErrorAndRetry() {
        facade.playbackState = PlaybackController.BluetoothWaiting
        facade.sourceError = "Spotify shutdown: Permission denied — check system config"
        var error = findChild(leftColumn, "sourceErrorLabel")
        var retry = findChild(leftColumn, "sourceRetryButton")
        tryCompare(error, "text", facade.sourceError)
        tryCompare(retry, "visible", true)
        verify(retry.height >= Theme.touchTarget)
        mouseClick(retry)
        compare(facade.retries, 1)
        facade.switching = true
        compare(retry.enabled, false)
        facade.sourceError = ""
        tryCompare(retry, "visible", false)
    }

    function checkTitleFits(title) {
        var artist = findChild(leftColumn, "artistLabel")
        var album = findChild(leftColumn, "albumLabel")
        verify(title !== null && artist !== null && album !== null)
        tryVerify(function() {
            return title.x >= 0 && title.width > 0
                   && title.x + title.width <= leftColumn.width
        })
        verify(title.width > 0)
        verify(title.lineCount > 0 && title.lineCount <= 3)
        verify(title.contentWidth <= title.width + 1,
               title.objectName + ": contentWidth=" + title.contentWidth
               + " width=" + title.width + " text=" + title.text)
        verify(title.y >= 0 && title.y + title.height <= leftColumn.height)
        if (title.objectName === "btNoTrackTitle") {
            var subtitle = findChild(leftColumn, "btNoTrackSubtitle")
            verify(subtitle !== null && subtitle.visible)
            verify(subtitle.y >= title.y + title.height)
            verify(subtitle.y + subtitle.height <= leftColumn.height)
            return
        }
        verify(artist.y >= title.y + title.height)
        verify(album.y >= artist.y + artist.height)
        verify(album.y + album.height <= leftColumn.height)
    }

    function test_spotifyLongTitleBothThemes() {
        var title = findChild(leftColumn, "trackTitleLabel")
        for (var dark of [true, false]) {
            Theme.darkMode = dark
            spotify.title = "Good Vibrations - Remastered"
            tryVerify(function() { return title.lineCount > 1 })
            checkTitleFits(title)

            spotify.title = "UnbreakablyLongSongTitleWithoutSpacesOrHyphens".repeat(5)
            tryCompare(title, "lineCount", 3)
            verify(title.truncated)
            checkTitleFits(title)
        }
    }

    function test_bluetoothTitleAndDeviceNameBothThemes() {
        var title = findChild(leftColumn, "trackTitleLabel")
        facade.playbackState = PlaybackController.BluetoothActive
        for (var dark of [true, false]) {
            Theme.darkMode = dark
            bluetooth.trackTitle = "Good Vibrations - Remastered"
            bluetooth.connectedDeviceName = "A very long phone name"
            tryVerify(function() { return title.lineCount > 1 })
            checkTitleFits(title)

            bluetooth.trackTitle = "UnbreakablyLongSongTitleWithoutSpacesOrHyphens".repeat(5)
            tryCompare(title, "lineCount", 3)
            verify(title.truncated)
            checkTitleFits(title)

            bluetooth.trackPublished = false
            var noTrackTitle = findChild(leftColumn, "btNoTrackTitle")
            tryVerify(function() { return noTrackTitle.visible })
            checkTitleFits(noTrackTitle)
            bluetooth.trackPublished = true
        }
    }

    function test_sourceErrorsStayWithSelectedSource() {
        var error = findChild(leftColumn, "sourceErrorLabel")
        verify(error !== null)
        compare(error.visible, false)
        spotify.errorMessage = "Permission denied — check system config"
        compare(error.visible, true)
        compare(error.text, spotify.errorMessage)
        facade.playbackState = PlaybackController.BluetoothWaiting
        compare(error.visible, false)
        bluetooth.errorMessage = "Bluetooth discoverability: timed out — try again"
        compare(error.visible, true)
        compare(error.text, bluetooth.errorMessage)
        bluetooth.errorMessage = ""
        compare(error.visible, false)
    }
}
