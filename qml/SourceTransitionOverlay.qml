import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BierKistnRadio

Popup {
    id: root
    objectName: "sourceTransitionOverlay"
    property var playback: PlaybackController
    parent: Overlay.overlay
    x: 0
    y: 0
    width: parent.width
    height: parent.height
    padding: 0
    z: 200
    modal: true
    dim: false
    focus: true
    closePolicy: Popup.NoAutoClose
    visible: playback.switching
    onAboutToShow: Qt.inputMethod.hide()

    background: Rectangle { color: Theme.loadingOverlayColor }

    ColumnLayout {
        anchors.centerIn: parent
        spacing: Theme.defaultSpacing
        BusyIndicator {
            objectName: "sourceLoadingIndicator"
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: Theme.touchTargetLarge
            Layout.preferredHeight: Theme.touchTargetLarge
            running: root.visible
        }
        Label {
            objectName: "sourceLoadingLabel"
            text: root.playback.playbackState === PlaybackController.BluetoothWaiting
                  || root.playback.playbackState === PlaybackController.BluetoothActive
                  ? "Switching to Bluetooth…" : "Switching to Spotify…"
            color: Theme.textColor
            font.pixelSize: Theme.fontSizeLarge
            Layout.alignment: Qt.AlignHCenter
        }
    }
}
