import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import BierKistnRadio

Page {
    background: Rectangle { color: Theme.backgroundColor }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.defaultSpacing
        spacing: Theme.defaultSpacing

        Label {
            text: PlaybackController.isSinkMode
                ? "Controlled by " + PlaybackController.pairedDeviceName
                : PlaybackController.title
            font.pixelSize: Theme.fontSizeXLarge
            font.bold: true
            color: Theme.textColor
            Layout.fillWidth: true
            elide: Text.ElideRight
        }

        Label {
            text: PlaybackController.artist
            font.pixelSize: Theme.fontSizeMedium
            color: Theme.secondaryTextColor
            visible: !PlaybackController.isSinkMode
            Layout.fillWidth: true
            elide: Text.ElideRight
        }

        Image {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: 200
            source: PlaybackController.artUrl
            fillMode: Image.PreserveAspectFit
            visible: !PlaybackController.isSinkMode && PlaybackController.artUrl !== ""
        }

        Slider {
            Layout.fillWidth: true
            from: 0
            to: PlaybackController.duration
            value: PlaybackController.position
            enabled: !PlaybackController.isStation && !PlaybackController.isSinkMode
            onMoved: PlaybackController.seek(value)
            visible: !PlaybackController.isStation && !PlaybackController.isSinkMode
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.touchTargetLarge
            spacing: Theme.defaultSpacing
            visible: !PlaybackController.isSinkMode

            Button {
                Layout.preferredWidth: Theme.touchTargetLarge
                Layout.preferredHeight: Theme.touchTargetLarge
                text: "⏮"
                font.pixelSize: 28
                onClicked: PlaybackController.previous()
            }

            Button {
                Layout.preferredWidth: Theme.touchTargetLarge
                Layout.preferredHeight: Theme.touchTargetLarge
                text: PlaybackController.isPlaying ? "⏸" : "▶"
                font.pixelSize: 28
                highlighted: true
                onClicked: PlaybackController.isPlaying
                    ? PlaybackController.pause()
                    : PlaybackController.play()
            }

            Button {
                Layout.preferredWidth: Theme.touchTargetLarge
                Layout.preferredHeight: Theme.touchTargetLarge
                text: "⏭"
                font.pixelSize: 28
                onClicked: PlaybackController.next()
            }

            Item { Layout.fillWidth: true }

            Slider {
                Layout.preferredWidth: 200
                Layout.preferredHeight: Theme.touchTargetLarge
                from: 0
                to: 150
                value: VolumeController.volume
                onMoved: VolumeController.setVolume(value)
            }
        }
    }
}
