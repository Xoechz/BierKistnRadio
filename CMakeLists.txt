cmake_minimum_required(VERSION 3.21)
project(BierKistnRadio VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Qml
    Quick
    QuickControls2
    Svg
    VirtualKeyboard
    Test
    QuickTest
)

qt_standard_project_setup(REQUIRES 6.5)

qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)

# --- Core library: C++ controllers + QML module ---
qt_add_library(bierkistn_core STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/controllers/PlaybackController.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/controllers/SpotifyClient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/controllers/BluetoothClient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/controllers/WifiController.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/controllers/VolumeController.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/controllers/ArtCache.cpp
)

target_include_directories(bierkistn_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/controllers
)

target_link_libraries(bierkistn_core PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Svg
    Qt6::VirtualKeyboard
    Qt6::DBus
)

set_source_files_properties(qml/Theme.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(bierkistn_core
    URI BierKistnRadio
    VERSION 1.0
    OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/BierKistnRadio
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/StatusBar.qml
        qml/ConfirmDialog.qml
        qml/LeftColumn.qml
        qml/CenterColumn.qml
        qml/RightSidebar.qml
)

# --- Application executable ---
qt_add_executable(bierkistnRadio
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
)

target_link_libraries(bierkistnRadio PRIVATE bierkistn_core)

install(TARGETS bierkistnRadio
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

# --- Testing ---
enable_testing()

# C++ controller unit tests
qt_add_executable(tst_controllers
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_controllers.cpp
)

target_link_libraries(tst_controllers PRIVATE
    bierkistn_core
    Qt6::Test
)

add_test(NAME tst_controllers COMMAND tst_controllers)

# QML view tests
qt_add_executable(tst_qml
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_qml.cpp
)

target_link_libraries(tst_qml PRIVATE
    bierkistn_core
    Qt6::QuickTest
)

target_compile_definitions(tst_qml PRIVATE
    QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests"
)

add_test(NAME tst_qml COMMAND tst_qml)

set_tests_properties(tst_qml PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QML_IMPORT_PATH=${CMAKE_BINARY_DIR}"
)
