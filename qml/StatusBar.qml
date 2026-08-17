import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import BierKistnRadio

Rectangle {
    id: root
    height: Theme.statusBarHeight
    color: Theme.statusBarColor

    property bool switching: false

    readonly property bool bluetoothActive: {
        switch (PlaybackController.playbackState) {
        case PlaybackController.BluetoothWaiting:
        case PlaybackController.BluetoothActive:
            return true
        default:
            return false
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.defaultSpacing
        anchors.rightMargin: Theme.defaultSpacing
        spacing: Theme.defaultSpacing

        // ---------- Clock (left) ----------
        Label {
            id: clockLabel
            text: Qt.formatTime(new Date(), "HH:mm")
            font.pixelSize: Theme.fontSizeLarge
            font.bold: true
            color: Theme.textColor
            Layout.alignment: Qt.AlignVCenter
        }

        // ---------- Source toggle (center) ----------
        Item { Layout.fillWidth: true }

        Rectangle {
            id: sourceToggle
            Layout.alignment: Qt.AlignVCenter
            width: 200
            height: 32
            radius: height / 2
            color: Theme.surfaceColor
            border.color: Theme.primaryColor
            border.width: 1
            opacity: root.switching ? 0.3 : 1.0

            Label {
                anchors.centerIn: parent
                text: root.switching ? "…" : ""
                font.pixelSize: Theme.fontSizeMedium
                color: Theme.accentColor
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 8

                Label {
                    text: "Spotify"
                    font.pixelSize: Theme.fontSizeSmall
                    font.bold: !root.bluetoothActive
                    color: root.bluetoothActive ? Theme.secondaryTextColor : Theme.accentColor
                }
                Label {
                    text: "○══○"
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.secondaryTextColor
                }
                Label {
                    text: "Bluetooth"
                    font.pixelSize: Theme.fontSizeSmall
                    font.bold: root.bluetoothActive
                    color: root.bluetoothActive ? Theme.accentColor : Theme.secondaryTextColor
                }
            }

            MouseArea {
                anchors.fill: parent
                enabled: !root.switching
                onClicked: root.switchSource()
            }
        }

        Item { Layout.fillWidth: true }

        // ---------- Reboot / Shutdown (right) ----------
        RowLayout {
            Layout.alignment: Qt.AlignVCenter
            spacing: Theme.defaultSpacing

            Button {
                text: "↻"
                flat: true
                font.pixelSize: Theme.fontSizeLarge
                Material.foreground: Theme.textColor
                onClicked: powerDialog.openFor("reboot")
            }
            Button {
                text: "⏻"
                flat: true
                font.pixelSize: Theme.fontSizeLarge
                Material.foreground: Theme.errorColor
                onClicked: powerDialog.openFor("shutdown")
            }
        }
    }

    ConfirmDialog {
        id: powerDialog
    }

    function switchSource() {
        if (root.switching) {
            return
        }
        root.switching = true
        resetTimer.restart()
        if (root.bluetoothActive) {
            PlaybackController.switchToSpotify()
        } else {
            PlaybackController.switchToBluetooth()
        }
    }

    Connections {
        target: PlaybackController
        function onPlaybackStateChanged() {
            root.switching = false
            resetTimer.stop()
        }
    }

    Timer {
        id: resetTimer
        interval: 3000
        onTriggered: root.switching = false
    }

    Timer {
        interval: 1000
        running: true
        repeat: true
        onTriggered: clockLabel.text = Qt.formatTime(new Date(), "HH:mm")
    }
}