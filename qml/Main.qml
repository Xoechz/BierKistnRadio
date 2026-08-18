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
                Layout.preferredWidth: 250
                Layout.fillHeight: true
            }

            CenterColumn {
                Layout.fillWidth: true
                Layout.fillHeight: true
            }

            RightSidebar {
                Layout.preferredWidth: 250
                Layout.fillHeight: true
            }
        }
    }

    TakeoverDialog {}

    InputPanel {
        id: keyboardPanel
        z: parent.z + 100
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        visible: Qt.inputMethod.visible
    }
}