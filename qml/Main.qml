import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
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
                Layout.preferredWidth: 230
                Layout.fillHeight: true
            }

            CenterColumn {
                Layout.fillWidth: true
                Layout.fillHeight: true
            }

            RightSidebar {
                Layout.preferredWidth: 230
                Layout.fillHeight: true
            }
        }
    }
}