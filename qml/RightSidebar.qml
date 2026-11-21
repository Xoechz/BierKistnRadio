import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import BierKistnRadio

Rectangle {
    id: root
    color: Theme.surfaceColor
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.smallSpacing
        spacing: Theme.smallSpacing

        Label {
            text: VolumeController.volume + "%"
            font.pixelSize: Theme.fontSizeSmall
            font.bold: true
            color: Theme.textColor
            Layout.alignment: Qt.AlignHCenter
        }
        Item {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: Theme.touchTarget
            Layout.fillHeight: true

            Slider {
                id: volumeSlider
                anchors.fill: parent
                orientation: Qt.Vertical
                from: 0
                to: 150
                value: VolumeController.volume
                // Handler: no arguments/return; request shared percentage, clamped in C++.
                onMoved: VolumeController.setVolume(value)
            }

            Rectangle {
                width: 24
                height: 2
                radius: 1
                color: Theme.primaryColor
                anchors.horizontalCenter: parent.horizontalCenter
                y: (volumeSlider.height - volumeSlider.handle.height) *
                   (1 - 100 / 150) + volumeSlider.handle.height / 2
            }
        }

        Button {
            objectName: "volumeMuteButton"
            Layout.preferredWidth: Theme.touchTarget
            Layout.preferredHeight: Theme.touchTarget
            Layout.alignment: Qt.AlignHCenter
            text: VolumeController.muted ? "🔇" : "🔊"
            Accessible.name: VolumeController.muted ? "Unmute volume" : "Mute volume"
            // Handler: no arguments/return; zero/restore level, not the separate sink mute flag.
            onClicked: VolumeController.setMuted(!VolumeController.muted)
        }

        Label {
            objectName: "volumeError"
            Layout.fillWidth: true
            text: VolumeController.errorMessage
            visible: text !== ""
            color: Theme.errorColor
            font.pixelSize: Theme.fontSizeSmall
            wrapMode: Text.WordWrap
        }

        Label {
            Layout.alignment: Qt.AlignHCenter
            text: "Dark mode"
            color: Theme.textColor
            font.pixelSize: Theme.fontSizeSmall
        }
        Switch {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredHeight: Theme.touchTarget
            Accessible.name: "Dark mode"
            checked: Theme.darkMode
            onToggled: Theme.darkMode = checked
        }
    }
}
