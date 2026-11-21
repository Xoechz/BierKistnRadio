import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.VirtualKeyboard
import BierKistnRadio

ApplicationWindow {
    id: root
    width: 1024
    height: 600
    visible: true
    flags: Qt.FramelessWindowHint
    visibility: Window.FullScreen
    color: Theme.backgroundColor

    Material.theme: Theme.materialTheme
    Material.accent: Theme.materialAccent

    Shortcut {
        sequence: "Alt+F4"
        // Handler: no arguments/return; explicit Alt+F4 closes the application window.
        onActivated: root.close()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        StatusBar {
            Layout.fillWidth: true
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            LeftColumn {
                inputPanel: keyboardPanel
                Layout.preferredWidth: Theme.metadataWidth
                Layout.fillHeight: true
            }

            CenterColumn {
                Layout.fillWidth: true
                Layout.fillHeight: true
            }

            RightSidebar {
                Layout.preferredWidth: Theme.volumeRailWidth
                Layout.fillHeight: true
            }
        }
    }

    TakeoverDialog {}
    PairingDialog {}
    SourceTransitionOverlay {}

    InputPanel {
        id: keyboardPanel
        parent: Overlay.overlay
        z: 1
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        visible: active
    }
}
