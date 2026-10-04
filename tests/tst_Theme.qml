import QtQuick
import QtTest
import BierKistnRadio

TestCase {
    name: "ThemeTests"

    // No arguments; returns undefined. Establish shared dark-mode state before each case.
    function init() {
        Theme.darkMode = true
    }

    // No arguments; returns undefined. Check initialized default through Qt assertions.
    function test_darkModeDefault() {
        compare(Theme.darkMode, true)
    }

    // No arguments; returns undefined. Check dark background/surface values.
    function test_colorsDark() {
        Theme.darkMode = true
        compare(Theme.backgroundColor, "#151515")
        compare(Theme.surfaceColor, "#1e1e1e")
    }

    // No arguments; returns undefined. Check light values after shared toggle.
    function test_colorsLight() {
        Theme.darkMode = false
        compare(Theme.backgroundColor, "#fafafa")
        compare(Theme.surfaceColor, "#ffffff")
    }

    // No arguments; returns undefined. Check 48/64 pixel target-size constants.
    function test_touchTargets() {
        compare(Theme.touchTarget, 48)
        compare(Theme.touchTargetLarge, 64)
    }
}
