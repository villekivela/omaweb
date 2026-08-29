cmake_minimum_required(VERSION 3.30)

project(Tanto VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

option(TANTO_BUILD_BROWSER "Build the QtWebEngine application" ON)
option(TANTO_BUILD_UI_LAB "Build the engine-free QML UI runner" ON)
option(TANTO_BUILD_UI_PROTOTYPE "Build the throwaway chrome-design prototype (src/ui-lab/prototype)" OFF)
option(TANTO_SOURCE_QML "Load QML from the source tree for fast iteration" ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(tanto_qt_components Core Gui Qml Quick Sql)
if(TANTO_BUILD_BROWSER)
    list(APPEND tanto_qt_components WebEngineQuick)
endif()
if(BUILD_TESTING)
    list(APPEND tanto_qt_components Test QuickTest)
endif()

find_package(Qt6 6.11 REQUIRED COMPONENTS ${tanto_qt_components})
qt_standard_project_setup(REQUIRES 6.11)

function(tanto_strict_target target)
    target_compile_options(${target} PRIVATE
        $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wall;-Wextra;-Wpedantic>
    )
endfunction()

add_library(tanto-core STATIC
    src/core/BrowserController.cpp
    src/core/BrowserController.h
    src/core/SessionStore.cpp
    src/core/SessionStore.h
    src/core/SpaceListModel.cpp
    src/core/SpaceListModel.h
    src/core/TabListModel.cpp
    src/core/TabListModel.h
)
target_include_directories(tanto-core PUBLIC src/core)
target_link_libraries(tanto-core PUBLIC Qt6::Core Qt6::Sql)
tanto_strict_target(tanto-core)

add_library(tanto-ui STATIC
    src/ui/ThemeController.cpp
    src/ui/ThemeController.h
)
target_include_directories(tanto-ui PUBLIC src/ui)
target_link_libraries(tanto-ui PUBLIC Qt6::Core)
tanto_strict_target(tanto-ui)

add_library(tanto-engine-api STATIC
    src/engine/api/EngineCapabilities.h
    src/engine/api/EngineViewContract.cpp
    src/engine/api/EngineViewContract.h
)
target_include_directories(tanto-engine-api PUBLIC src/engine/api)
target_link_libraries(tanto-engine-api PUBLIC Qt6::Core)
tanto_strict_target(tanto-engine-api)

if(TANTO_BUILD_BROWSER)
    add_library(tanto-engine-qt INTERFACE)
    target_sources(tanto-engine-qt INTERFACE src/engine/qt/EngineView.qml)
    target_link_libraries(tanto-engine-qt INTERFACE
        tanto-engine-api
        Qt6::WebEngineQuick
    )
endif()

add_library(tanto-platform INTERFACE)
target_link_libraries(tanto-platform INTERFACE Qt6::Gui Qt6::Quick)

set(tanto_ui_files
    src/ui/Main.qml
    src/ui/ChromeButton.qml
    src/ui/PinnedTabDelegate.qml
    src/ui/TabDelegate.qml
)

function(tanto_configure_qml target engine_view engine_directory)
    target_compile_definitions(${target} PRIVATE
        TANTO_VERSION="${PROJECT_VERSION}"
        TANTO_UI_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}/src/ui"
    )

    if(TANTO_SOURCE_QML)
        target_compile_definitions(${target} PRIVATE
            TANTO_MAIN_QML_URL="file://${CMAKE_CURRENT_SOURCE_DIR}/src/ui/Main.qml"
            TANTO_ENGINE_VIEW_URL="file://${CMAKE_CURRENT_SOURCE_DIR}/${engine_view}"
            TANTO_THEME_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/themes/default.json"
            TANTO_ICON_FONT_URL="file://${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/material-symbols-rounded-subset.ttf"
        )
    else()
        set_source_files_properties("${engine_view}" PROPERTIES QT_RESOURCE_ALIAS "EngineView.qml")
        qt_add_resources(${target} "tanto_ui"
            PREFIX "/tanto/ui"
            BASE "${CMAKE_CURRENT_SOURCE_DIR}/src/ui"
            FILES ${tanto_ui_files}
        )
        qt_add_resources(${target} "tanto_engine_view"
            PREFIX "/tanto/engine"
            BASE "${CMAKE_CURRENT_SOURCE_DIR}/${engine_directory}"
            FILES "${engine_view}"
        )
        qt_add_resources(${target} "tanto_theme"
            PREFIX "/tanto/themes"
            BASE "${CMAKE_CURRENT_SOURCE_DIR}/assets/themes"
            FILES assets/themes/default.json
        )
        qt_add_resources(${target} "tanto_fonts"
            PREFIX "/tanto/fonts"
            BASE "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts"
            FILES assets/fonts/material-symbols-rounded-subset.ttf
        )
        target_compile_definitions(${target} PRIVATE
            TANTO_MAIN_QML_URL="qrc:/tanto/ui/Main.qml"
            TANTO_ENGINE_VIEW_URL="qrc:/tanto/engine/EngineView.qml"
            TANTO_THEME_PATH=":/tanto/themes/default.json"
            TANTO_ICON_FONT_URL="qrc:/tanto/fonts/material-symbols-rounded-subset.ttf"
        )
    endif()
endfunction()

if(TANTO_BUILD_BROWSER)
    qt_add_executable(tanto MACOSX_BUNDLE src/main.cpp)
    target_link_libraries(tanto PRIVATE
        tanto-core
        tanto-ui
        tanto-engine-qt
        tanto-platform
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
    )
    tanto_configure_qml(tanto "src/engine/qt/EngineView.qml" "src/engine/qt")
    tanto_strict_target(tanto)

    set_target_properties(tanto PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME "Tanto"
        MACOSX_BUNDLE_GUI_IDENTIFIER "dev.tanto.browser"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "1"
        WIN32_EXECUTABLE TRUE
    )
endif()

if(TANTO_BUILD_UI_LAB)
    qt_add_executable(tanto-ui-lab MACOSX_BUNDLE src/ui-lab/main.cpp)
    target_link_libraries(tanto-ui-lab PRIVATE
        tanto-core
        tanto-ui
        tanto-engine-api
        tanto-platform
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
    )
    tanto_configure_qml(tanto-ui-lab "src/ui-lab/MockEngineView.qml" "src/ui-lab")
    tanto_strict_target(tanto-ui-lab)
    set_target_properties(tanto-ui-lab PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME "Tanto UI Lab"
        MACOSX_BUNDLE_GUI_IDENTIFIER "dev.tanto.browser.ui-lab"
    )
endif()

if(BUILD_TESTING)
    add_test(NAME tanto-shared-qml-boundary
        COMMAND ${CMAKE_COMMAND}
            -DTANTO_UI_DIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}/src/ui
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/check_shared_qml.cmake
    )

    qt_add_executable(tanto-core-tests tests/core/tst_browsercontroller.cpp)
    target_link_libraries(tanto-core-tests PRIVATE tanto-core Qt6::Test)
    tanto_strict_target(tanto-core-tests)
    add_test(NAME tanto-core COMMAND tanto-core-tests)

    qt_add_executable(tanto-qml-smoke src/ui-lab/main.cpp)
    target_link_libraries(tanto-qml-smoke PRIVATE
        tanto-core
        tanto-ui
        tanto-engine-api
        tanto-platform
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
    )
    tanto_configure_qml(tanto-qml-smoke "src/ui-lab/MockEngineView.qml" "src/ui-lab")
    tanto_strict_target(tanto-qml-smoke)
    add_test(NAME tanto-qml-load COMMAND tanto-qml-smoke --validate-qml)
    set_tests_properties(tanto-qml-load PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    if(TANTO_BUILD_UI_LAB)
        qt_add_executable(tanto-ui-tests tests/ui/tst_ui.cpp)
        target_link_libraries(tanto-ui-tests PRIVATE
            tanto-core
            tanto-ui
            Qt6::Gui
            Qt6::Qml
            Qt6::Quick
            Qt6::QuickTest
        )
        target_compile_definitions(tanto-ui-tests PRIVATE
            QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/ui"
            TANTO_MOCK_ENGINE_VIEW_URL="file://${CMAKE_CURRENT_SOURCE_DIR}/src/ui-lab/MockEngineView.qml"
            TANTO_THEME_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/themes/default.json"
            TANTO_UI_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}/src/ui"
        )
        tanto_strict_target(tanto-ui-tests)
        add_test(NAME tanto-ui COMMAND tanto-ui-tests)
        set_tests_properties(tanto-ui PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        )
    endif()

    if(TANTO_BUILD_BROWSER)
        qt_add_executable(tanto-qt-engine-contract-tests
            tests/engine/tst_qtenginecontract.cpp
        )
        target_link_libraries(tanto-qt-engine-contract-tests PRIVATE
            tanto-engine-qt
            Qt6::Gui
            Qt6::Qml
            Qt6::Quick
            Qt6::Test
        )
        target_compile_definitions(tanto-qt-engine-contract-tests PRIVATE
            TANTO_MOCK_ENGINE_VIEW_PATH="${CMAKE_CURRENT_SOURCE_DIR}/src/ui-lab/MockEngineView.qml"
            TANTO_QT_ENGINE_VIEW_PATH="${CMAKE_CURRENT_SOURCE_DIR}/src/engine/qt/EngineView.qml"
        )
        tanto_strict_target(tanto-qt-engine-contract-tests)
        add_test(NAME tanto-qt-engine-contract COMMAND tanto-qt-engine-contract-tests)
        set_tests_properties(tanto-qt-engine-contract PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        )
    endif()
endif()

# THROWAWAY: chrome-design prototype. Off by default; see src/ui-lab/prototype/README.md.
if(TANTO_BUILD_UI_PROTOTYPE)
    qt_add_executable(tanto-ui-prototype MACOSX_BUNDLE src/ui-lab/prototype/main.cpp)
    target_link_libraries(tanto-ui-prototype PRIVATE
        tanto-core
        tanto-ui
        tanto-platform
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
    )
    target_compile_definitions(tanto-ui-prototype PRIVATE
        TANTO_THEME_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/themes/default.json"
        TANTO_ICON_FONT_URL="file://${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/material-symbols-rounded-subset.ttf"
        TANTO_PROTOTYPE_SHELL_URL="file://${CMAKE_CURRENT_SOURCE_DIR}/src/ui-lab/prototype/PrototypeShell.qml"
    )
    tanto_strict_target(tanto-ui-prototype)
    set_target_properties(tanto-ui-prototype PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME "Tanto UI Prototype"
        MACOSX_BUNDLE_GUI_IDENTIFIER "dev.tanto.browser.ui-prototype"
    )
endif()
