import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import BierKistnRadio

Rectangle {
    id: root
    height: Theme.statusBarHeight
    color: Theme.statusBarColor

    readonly property bool switching: PlaybackController.switching

    // Binding: no explicit arguments; returns whether requested view is a Bluetooth state.
    readonly property bool bluetoothActive: {
        switch (PlaybackController.playbackState) {
        case PlaybackController.BluetoothWaiting:
        case PlaybackController.BluetoothActive:
            return true
        default:
            return false
        }
    }

    Label {
        id: clockLabel
        anchors.left: parent.left
        anchors.leftMargin: Theme.defaultSpacing
        anchors.verticalCenter: parent.verticalCenter
        text: Qt.formatTime(new Date(), "HH:mm")
        font.pixelSize: Theme.fontSizeLarge
        font.bold: true
        color: Theme.textColor
    }

    Row {
        anchors.centerIn: parent
        enabled: !root.switching
        Repeater {
            model: ["Spotify", "Bluetooth"]
            delegate: Button {
                id: segment
                required property string modelData
                required property int index
                readonly property bool selected: root.bluetoothActive === (index === 1)
                width: 104
                height: Theme.touchTarget
                text: selected && root.switching ? "…" : modelData
                Accessible.name: modelData
                background: Rectangle {
                    anchors.fill: parent
                    anchors.topMargin: Theme.smallSpacing
                    anchors.bottomMargin: Theme.smallSpacing
                    radius: height / 2
                    color: !segment.selected ? Theme.surfaceColor
                          : segment.index === 1 ? Theme.bluetoothColor : Theme.spotifyColor
                }
                contentItem: Label {
                    text: segment.text
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font.bold: segment.selected
                    color: !segment.selected ? Theme.secondaryTextColor
                          : segment.index === 1 ? Theme.bluetoothTextColor : Theme.spotifyTextColor
                }
                onClicked: if (!selected) { root.switchSource() }
            }
        }
    }

    // ---------- Reboot / Shutdown (right) ----------
    RowLayout {
        anchors.right: parent.right
        anchors.rightMargin: Theme.defaultSpacing
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.defaultSpacing

        Button {
            text: "↻"
            flat: true
            font.pixelSize: Theme.fontSizeLarge
            Material.foreground: Theme.textColor
            // Handler: no arguments/return; ask for confirmation before any reboot command.
            onClicked: powerDialog.openFor("reboot")
        }
        Button {
            text: "⏻"
            flat: true
            font.pixelSize: Theme.fontSizeLarge
            Material.foreground: Theme.errorColor
            // Handler: no arguments/return; ask for confirmation before poweroff.
            onClicked: powerDialog.openFor("shutdown")
        }
    }

    ConfirmDialog {
        id: powerDialog
    }

    // No arguments; returns undefined. Toggle explicit selection unless already switching.
    function switchSource() {
        if (root.switching) {
            return
        }
        if (root.bluetoothActive) {
            PlaybackController.switchToSpotify()
        } else {
            PlaybackController.switchToBluetooth()
        }
    }

    Timer {
        interval: 1000
        running: true
        repeat: true
        // Handler: no arguments/return; refresh local-time clock every second.
        onTriggered: clockLabel.text = Qt.formatTime(new Date(), "HH:mm")
    }
}
