import QtQuick
import QtTest
import BierKistnRadio

TestCase {
    name: "ThemeTests"

    function init() {
        Theme.darkMode = true
    }

    function test_darkModeDefault() {
        compare(Theme.darkMode, true)
    }

    function test_colorsDark() {
        Theme.darkMode = true
        compare(Theme.backgroundColor, "#121212")
        compare(Theme.surfaceColor, "#1e1e1e")
    }

    function test_colorsLight() {
        Theme.darkMode = false
        compare(Theme.backgroundColor, "#fafafa")
        compare(Theme.surfaceColor, "#ffffff")
    }

    function test_touchTargets() {
        compare(Theme.touchTarget, 48)
        compare(Theme.touchTargetLarge, 64)
    }
}
