pragma Singleton
import QtQuick
import QtQuick.Controls.Material

QtObject {
    property bool darkMode: true

    readonly property color backgroundColor: darkMode ? "#151515" : "#FAFAFA"
    readonly property color surfaceColor: darkMode ? "#1E1E1E" : "#FFFFFF"
    readonly property color statusBarColor: darkMode ? "#101010" : "#DDDDDD"
    readonly property color primaryColor: darkMode ? "#BB86FC" : "#6200EE"
    readonly property color accentColor: darkMode ? "#03DAC6" : "#03DAC6"
    readonly property color textColor: darkMode ? "#FFFFFF" : "#1A1A1A"
    readonly property color secondaryTextColor: darkMode ? "#B0B0B0" : "#666666"
    readonly property color errorColor: darkMode ? "#CF6679" : "#B00020"

    readonly property int touchTarget: 48
    readonly property int touchTargetLarge: 64

    readonly property int statusBarHeight: 48
    readonly property int cornerRadius: 12
    readonly property int defaultSpacing: 16
    readonly property int smallSpacing: 8

    readonly property int fontSizeSmall: 14
    readonly property int fontSizeMedium: 18
    readonly property int fontSizeLarge: 24
    readonly property int fontSizeXLarge: 32

    readonly property int materialTheme: darkMode ? Material.Dark : Material.Light
    readonly property color materialAccent: darkMode ? accentColor : primaryColor
}
