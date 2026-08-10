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

    QtObject {
        id: appRouter

        property string currentView: "nowplaying"

        function showView(view) {
            currentView = view
        }

        function showSettings(section) {
            currentView = "settings"
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        StatusBar {
            Layout.fillWidth: true
            activeSource: {
                switch (PlaybackController.playbackState) {
                    case PlaybackController.BluetoothWaiting:
                    case PlaybackController.BluetoothActive:
                        return "Bluetooth"
                    default:
                        return "Spotify"
                }
            }
        }

        Loader {
            id: viewLoader
            Layout.fillWidth: true
            Layout.fillHeight: true
            source: {
                switch (appRouter.currentView) {
                    case "nowplaying": return "qrc:/qt/qml/BierKistnRadio/views/NowPlaying.qml"
                    case "settings": return "qrc:/qt/qml/BierKistnRadio/views/Settings.qml"
                    default: return ""
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.touchTargetLarge
            spacing: 0

            Repeater {
                model: [
                    { view: "nowplaying", label: "Now Playing", icon: "▶" },
                    { view: "settings", label: "Settings", icon: "⚙" }
                ]

                Button {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: modelData.icon + "  " + modelData.label
                    flat: true
                    highlighted: appRouter.currentView === modelData.view
                    Material.foreground: appRouter.currentView === modelData.view
                        ? Theme.accentColor
                        : Theme.secondaryTextColor
                    onClicked: appRouter.showView(modelData.view)
                }
            }
        }
    }
}
