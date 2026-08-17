#!/usr/bin/env bash
# Point Qt at the devShell's QML modules and plugins. A home-manager/global Qt
# profile on QML2_IMPORT_PATH can shadow the devShell's Qt and hide modules like
# QtQuick.VirtualKeyboard. Safe to source multiple times; no-op if the devShell
# prefix list is not present.
set -u

if [[ -n "${QT_ADDITIONAL_PACKAGES_PREFIX_PATH:-}" ]]; then
    import_dirs=""
    plugin_dirs=""
    IFS=':' read -r -a prefixes <<< "$QT_ADDITIONAL_PACKAGES_PREFIX_PATH"
    for p in "${prefixes[@]}"; do
        qml="$p/lib/qt-6/qml"
        plugins="$p/lib/qt-6/plugins"
        [[ -d "$qml" ]] && import_dirs="${import_dirs:+$import_dirs:}$qml"
        [[ -d "$plugins" ]] && plugin_dirs="${plugin_dirs:+$plugin_dirs:}$plugins"
    done
    if [[ -n "$import_dirs" ]]; then
        export QML2_IMPORT_PATH="$import_dirs"
        export QT_PLUGIN_PATH="$plugin_dirs"
    fi
fi