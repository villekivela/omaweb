cmake_minimum_required(VERSION 3.30)

project(Tanto VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

option(TANTO_BUILD_BROWSER "Build the QtWebEngine application" ON)
option(TANTO_BUILD_UI_LAB "Build the engine-free QML UI runner" ON)
option(TANTO_SOURCE_QML "Load QML from the source tree for fast iteration" ON)

set(TANTO_CONTENT_BLOCKER_CACHE "${CMAKE_CURRENT_SOURCE_DIR}/.cache/content-blocker/release"
    CACHE PATH "Directory containing the bootstrapped content-blocker shared library")
if(APPLE)
    set(tanto_content_blocker_library
        "${TANTO_CONTENT_BLOCKER_CACHE}/libtanto_content_blocker.dylib")
elseif(UNIX)
    set(tanto_content_blocker_library
        "${TANTO_CONTENT_BLOCKER_CACHE}/libtanto_content_blocker.so")
else()
    message(FATAL_ERROR "The content-blocker bootstrap currently supports macOS and Linux")
endif()
if(NOT EXISTS "${tanto_content_blocker_library}")
    message(FATAL_ERROR
        "Missing bootstrapped content blocker. Run scripts/bootstrap_content_blocker.sh")
endif()
if(NOT EXISTS "${tanto_content_blocker_library}.sha256")
    message(FATAL_ERROR "Missing checksum for the bootstrapped content blocker")
endif()
file(STRINGS "${tanto_content_blocker_library}.sha256" tanto_content_blocker_checksum_line
    LIMIT_COUNT 1)
string(REGEX MATCH "^[0-9a-fA-F]+" tanto_content_blocker_expected_checksum
    "${tanto_content_blocker_checksum_line}")
file(SHA256 "${tanto_content_blocker_library}" tanto_content_blocker_actual_checksum)
if(NOT tanto_content_blocker_expected_checksum STREQUAL tanto_content_blocker_actual_checksum)
    message(FATAL_ERROR "The bootstrapped content-blocker checksum does not match")
endif()
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/content-blocker/LADYBIRD_REVISION"
    tanto_ladybird_revision LIMIT_COUNT 1)

# uBlock Origin's scriptlet library and substitute resources. The content
# blocker builds their descriptor files into its own binary, so the pin belongs
# to the bootstrap rather than to this build; the integrity test below is what
# keeps the copies honest.
set(tanto_ubo_scriptlets_root "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ubo-scriptlets")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(tanto_qt_components Concurrent Core Gui Network Qml Quick QuickControls2 Sql)
if(TANTO_BUILD_BROWSER)
    list(APPEND tanto_qt_components WebEngineCore WebEngineQuick)
endif()
if(BUILD_TESTING)
    list(APPEND tanto_qt_components Test QuickTest)
endif()

find_package(Qt6 6.11 REQUIRED COMPONENTS ${tanto_qt_components})
qt_standard_project_setup(REQUIRES 6.11)

add_library(tanto-adblock-rust SHARED IMPORTED GLOBAL)
set_target_properties(tanto-adblock-rust PROPERTIES
    IMPORTED_LOCATION "${tanto_content_blocker_library}"
)

add_library(tanto-content-blocking STATIC
    src/content-blocking/ContentMatcher.cpp
    src/content-blocking/ContentMatcher.h
    src/content-blocking/tanto_blocker.h
)
target_include_directories(tanto-content-blocking PUBLIC src/content-blocking)
target_link_libraries(tanto-content-blocking PUBLIC
    Qt6::Core
    Qt6::Network
    PRIVATE Qt6::Concurrent tanto-adblock-rust
)

function(tanto_strict_target target)
    target_compile_options(${target} PRIVATE
        $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wall;-Wextra;-Wpedantic>
    )
endfunction()

tanto_strict_target(tanto-content-blocking)

add_library(tanto-core STATIC
    src/content-blocking/ContentBlocker.cpp
    src/content-blocking/ContentBlocker.h
    src/core/BrowserController.cpp
    src/core/BrowserController.h
    src/core/DevelopmentLaunch.cpp
    src/core/DevelopmentLaunch.h
    src/core/KeyboardNavigation.cpp
    src/core/KeyboardNavigation.h
    src/core/SessionStore.cpp
    src/core/SessionStore.h
    src/core/SpaceListModel.cpp
    src/core/SpaceListModel.h
    src/core/TabListModel.cpp
    src/core/TabListModel.h
    src/core/WindowManager.cpp
    src/core/WindowManager.h
)
target_include_directories(tanto-core PUBLIC src/core src/content-blocking)
target_link_libraries(tanto-core PUBLIC Qt6::Core Qt6::Network Qt6::Sql
    PRIVATE Qt6::Concurrent tanto-content-blocking)
tanto_strict_target(tanto-core)

add_library(tanto-ui STATIC
    src/ui/FaviconTint.cpp
    src/ui/FaviconTint.h
    src/ui/KitTheme.cpp
    src/ui/KitTheme.h
    src/ui/SystemClipboard.cpp
    src/ui/SystemClipboard.h
    src/ui/ThemeController.cpp
    src/ui/ThemeController.h
)
target_include_directories(tanto-ui PUBLIC src/ui)
target_link_libraries(tanto-ui PUBLIC Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick)
tanto_strict_target(tanto-ui)

# The vendored Omarchy component kit imports Quickshell types. Tanto supplies
# the small subset the kit's shared components need instead of patching the
# copies, so a sync stays a review of upstream's diff. See
# third_party/omarchy-shell/README.md.
add_library(tanto-quickshell-shim STATIC
    src/ui/quickshell-shim/Quickshell.cpp
    src/ui/quickshell-shim/Quickshell.h
    src/ui/quickshell-shim/QuickshellIo.cpp
    src/ui/quickshell-shim/QuickshellIo.h
)
target_include_directories(tanto-quickshell-shim PUBLIC src/ui/quickshell-shim)
target_link_libraries(tanto-quickshell-shim PUBLIC Qt6::Core Qt6::Qml Qt6::QuickControls2)
set_target_properties(tanto-quickshell-shim PROPERTIES AUTOMOC ON)
tanto_strict_target(tanto-quickshell-shim)

# The manifest is the single list of vendored files: the resource build, the
# integrity test, and scripts/sync_omarchy_ui.py all read it.
set(tanto_omarchy_root "${CMAKE_CURRENT_SOURCE_DIR}/third_party/omarchy-shell")
file(READ "${tanto_omarchy_root}/MANIFEST.json" tanto_omarchy_manifest)
string(JSON tanto_omarchy_manifest_files GET "${tanto_omarchy_manifest}" files)
string(JSON tanto_omarchy_file_count LENGTH "${tanto_omarchy_manifest_files}")
set(tanto_omarchy_files "")
if(tanto_omarchy_file_count GREATER 0)
    math(EXPR tanto_omarchy_last_file "${tanto_omarchy_file_count} - 1")
    foreach(index RANGE 0 ${tanto_omarchy_last_file})
        string(JSON tanto_omarchy_file MEMBER "${tanto_omarchy_manifest_files}" ${index})
        # Only the QML module tree goes into the resource; the manifest also
        # tracks upstream's license text.
        if(tanto_omarchy_file MATCHES "^qs/")
            list(APPEND tanto_omarchy_files "third_party/omarchy-shell/${tanto_omarchy_file}")
        endif()
    endforeach()
endif()

add_library(tanto-engine-api STATIC
    src/engine/api/EngineCapabilities.h
    src/engine/api/EngineViewContract.cpp
    src/engine/api/EngineViewContract.h
)
target_include_directories(tanto-engine-api PUBLIC src/engine/api)
target_link_libraries(tanto-engine-api PUBLIC Qt6::Core)
tanto_strict_target(tanto-engine-api)

if(TANTO_BUILD_BROWSER)
    add_library(tanto-engine-qt STATIC
        src/engine/qt/QtContentBlocker.cpp
        src/engine/qt/QtContentBlocker.h
    )
    target_sources(tanto-engine-qt PUBLIC src/engine/qt/EngineView.qml)
    target_include_directories(tanto-engine-qt PUBLIC src/engine/qt)
    target_link_libraries(tanto-engine-qt PUBLIC
        tanto-core
        tanto-engine-api
        Qt6::WebEngineCore
        Qt6::WebEngineQuick
    )
    tanto_strict_target(tanto-engine-qt)
endif()

add_library(tanto-platform STATIC
    src/platform/ExternalProtocolHandler.cpp
    src/platform/ExternalProtocolHandler.h
    src/platform/PagePrinter.cpp
    src/platform/PagePrinter.h
    src/platform/ProcessResources.cpp
    src/platform/ProcessResources.h
    src/platform/SystemNotifier.cpp
    src/platform/SystemNotifier.h
    src/platform/WindowChrome.h
)
target_include_directories(tanto-platform PUBLIC src/platform)
target_link_libraries(tanto-platform PUBLIC Qt6::Gui Qt6::Qml Qt6::Quick)
if(APPLE)
    enable_language(OBJCXX)
    find_library(TANTO_APPKIT_FRAMEWORK AppKit REQUIRED)
    find_library(TANTO_QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
    # Quartz is the umbrella that carries PDFKit, which draws and prints the
    # PDF the engine renders a page into.
    find_library(TANTO_QUARTZ_FRAMEWORK Quartz REQUIRED)
    # The desktop's notification service, which is a bundle service: it
    # identifies the notifying application by its bundle identifier.
    find_library(TANTO_USER_NOTIFICATIONS_FRAMEWORK UserNotifications REQUIRED)
    target_sources(tanto-platform PRIVATE
        src/platform/MacExternalProtocolHandler.mm
        src/platform/MacPagePrinter.mm
        src/platform/MacSystemNotifier.mm
        src/platform/MacWindowChrome.mm
    )
    target_link_libraries(tanto-platform PRIVATE
        ${TANTO_APPKIT_FRAMEWORK}
        ${TANTO_QUARTZCORE_FRAMEWORK}
        ${TANTO_QUARTZ_FRAMEWORK}
        ${TANTO_USER_NOTIFICATIONS_FRAMEWORK}
    )
else()
    target_sources(tanto-platform PRIVATE
        src/platform/WindowChrome.cpp
    )
endif()
tanto_strict_target(tanto-platform)

set(tanto_ui_files
    src/ui/Main.qml
    src/ui/AuxiliaryWindow.qml
    src/ui/BrowserCommands.qml
    src/ui/ChromeButton.qml
    src/ui/ChromeMenu.qml
    src/ui/CommandPanel.qml
    src/ui/DeveloperToolsDock.qml
    src/ui/FindBar.qml
    src/ui/HistoryPage.qml
    src/ui/KeyHint.qml
    src/ui/KeyMap.qml
    src/ui/NavigationCluster.qml
    src/ui/NoticeBox.qml
    src/ui/ActionButton.qml
    src/ui/CommandDialog.qml
    src/ui/PageBackdrop.qml
    src/ui/PageNotice.qml
    src/ui/PageQuestionBar.qml
    src/ui/PagePromptBar.qml
    src/ui/SectionLabel.qml
    src/ui/MultilineField.qml
    src/ui/SettingDropdown.qml
    src/ui/SettingField.qml
    src/ui/SettingRow.qml
    src/ui/SettingToggle.qml
    src/ui/SettingsPage.qml
    src/ui/PanelResizer.qml
    src/ui/SiteTile.qml
    src/ui/SpaceOutline.qml
    src/ui/StartPage.qml
    src/ui/TabEngineHost.qml
    src/ui/TabRow.qml
)

function(tanto_configure_qml target engine_view engine_profile engine_directory)
    target_compile_definitions(${target} PRIVATE
        TANTO_VERSION="${PROJECT_VERSION}"
        TANTO_UI_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}/src/ui"
    )

    if(TANTO_SOURCE_QML)
        target_compile_definitions(${target} PRIVATE
            TANTO_OMARCHY_IMPORT_PATH="${tanto_omarchy_root}"
            TANTO_MAIN_QML_URL="file://${CMAKE_CURRENT_SOURCE_DIR}/src/ui/Main.qml"
            TANTO_ENGINE_VIEW_URL="file://${CMAKE_CURRENT_SOURCE_DIR}/${engine_view}"
            TANTO_ENGINE_PROFILE_URL="file://${CMAKE_CURRENT_SOURCE_DIR}/${engine_profile}"
            TANTO_THEME_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/themes/default.json"
            TANTO_ICON_FONT_URL="file://${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/material-symbols-rounded.ttf"
            TANTO_DEFAULT_KEYBINDINGS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/keybindings/default.json"
            TANTO_KEYBOARD_NAVIGATION_SCRIPT_PATH="${CMAKE_CURRENT_SOURCE_DIR}/src/engine/api/keyboard-navigation.js"
        )
    else()
        set_source_files_properties("${engine_view}" PROPERTIES QT_RESOURCE_ALIAS "EngineView.qml")
        set_source_files_properties("${engine_profile}" PROPERTIES QT_RESOURCE_ALIAS "EngineProfile.qml")
        qt_add_resources(${target} "tanto_ui"
            PREFIX "/tanto/ui"
            BASE "${CMAKE_CURRENT_SOURCE_DIR}/src/ui"
            FILES ${tanto_ui_files}
        )
        # `/qt/qml` is already on the engine's import path, so the vendored
        # `qs.Commons` / `qs.Ui` modules resolve from the resource unchanged.
        qt_add_resources(${target} "tanto_omarchy_ui"
            PREFIX "/qt/qml"
            BASE "${tanto_omarchy_root}"
            FILES ${tanto_omarchy_files}
        )
        qt_add_resources(${target} "tanto_engine_view"
            PREFIX "/tanto/engine"
            BASE "${CMAKE_CURRENT_SOURCE_DIR}/${engine_directory}"
            FILES "${engine_view}"
                "${engine_profile}"
        )
        qt_add_resources(${target} "tanto_theme"
            PREFIX "/tanto/themes"
            BASE "${CMAKE_CURRENT_SOURCE_DIR}/assets/themes"
            FILES assets/themes/default.json
        )
        qt_add_resources(${target} "tanto_fonts"
            PREFIX "/tanto/fonts"
            BASE "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts"
            FILES assets/fonts/material-symbols-rounded.ttf
        )
        qt_add_resources(${target} "tanto_keybindings"
            PREFIX "/tanto/keybindings"
            BASE "${CMAKE_CURRENT_SOURCE_DIR}/assets/keybindings"
            FILES assets/keybindings/default.json
        )
        qt_add_resources(${target} "tanto_keyboard_navigation_script"
            PREFIX "/tanto/engine-api"
            BASE "${CMAKE_CURRENT_SOURCE_DIR}/src/engine/api"
            FILES src/engine/api/keyboard-navigation.js
        )
        target_compile_definitions(${target} PRIVATE
            TANTO_OMARCHY_IMPORT_PATH="qrc:/qt/qml"
            TANTO_MAIN_QML_URL="qrc:/tanto/ui/Main.qml"
            TANTO_ENGINE_VIEW_URL="qrc:/tanto/engine/EngineView.qml"
            TANTO_ENGINE_PROFILE_URL="qrc:/tanto/engine/EngineProfile.qml"
            TANTO_THEME_PATH=":/tanto/themes/default.json"
            TANTO_ICON_FONT_URL="qrc:/tanto/fonts/material-symbols-rounded.ttf"
            TANTO_DEFAULT_KEYBINDINGS_PATH=":/tanto/keybindings/default.json"
            TANTO_KEYBOARD_NAVIGATION_SCRIPT_PATH=":/tanto/engine-api/keyboard-navigation.js"
        )
    endif()
endfunction()

if(TANTO_BUILD_BROWSER)
    qt_add_executable(tanto MACOSX_BUNDLE src/main.cpp)
    target_link_libraries(tanto PRIVATE
        tanto-core
        tanto-ui
        tanto-quickshell-shim
        tanto-engine-qt
        tanto-platform
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
    )
    tanto_configure_qml(tanto "src/engine/qt/EngineView.qml"
        "src/engine/qt/EngineProfile.qml" "src/engine/qt")
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
        tanto-content-blocking
        tanto-ui
        tanto-quickshell-shim
        tanto-engine-api
        tanto-platform
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
    )
    tanto_configure_qml(tanto-ui-lab "src/ui-lab/MockEngineView.qml"
        "src/ui-lab/MockEngineProfile.qml" "src/ui-lab")
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

    add_test(NAME tanto-vendored-omarchy-ui
        COMMAND ${CMAKE_COMMAND}
            -DTANTO_VENDOR_ROOT=${tanto_omarchy_root}
            -DTANTO_VENDOR_TRACKED=qs/*
            -DTANTO_VENDOR_NAME=Omarchy\ kit
            "-DTANTO_VENDOR_ADVICE=Adapt in src/ui, or run scripts/sync_omarchy_ui.py --sync to restore them."
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/check_vendored_tree.cmake
    )

    add_test(NAME tanto-vendored-ubo-scriptlets
        COMMAND ${CMAKE_COMMAND}
            -DTANTO_VENDOR_ROOT=${tanto_ubo_scriptlets_root}
            -DTANTO_VENDOR_TRACKED=src/*
            -DTANTO_VENDOR_NAME=uBO\ scriptlets
            "-DTANTO_VENDOR_ADVICE=Run scripts/sync_ubo_scriptlets.py --sync to restore them."
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/check_vendored_tree.cmake
    )

    # The drift report and the issue it turns into, with GitHub stubbed out.
    # Python is the sync script's language, not a build dependency, so the test
    # only exists where an interpreter is already present.
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME tanto-omarchy-drift
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/scripts/tst_omarchy_drift.py
        )
    endif()

    qt_add_executable(tanto-core-tests tests/core/tst_browsercontroller.cpp)
    target_link_libraries(tanto-core-tests PRIVATE tanto-core Qt6::Test)
    tanto_strict_target(tanto-core-tests)
    add_test(NAME tanto-core COMMAND tanto-core-tests)

    qt_add_executable(tanto-keyboard-navigation-tests
        tests/core/tst_keyboardnavigation.cpp
    )
    target_link_libraries(tanto-keyboard-navigation-tests PRIVATE tanto-core Qt6::Test)
    tanto_strict_target(tanto-keyboard-navigation-tests)
    add_test(NAME tanto-keyboard-navigation COMMAND tanto-keyboard-navigation-tests)

    qt_add_executable(tanto-development-launch-tests
        tests/core/tst_developmentlaunch.cpp
    )
    target_link_libraries(tanto-development-launch-tests PRIVATE tanto-core Qt6::Test)
    tanto_strict_target(tanto-development-launch-tests)
    add_test(NAME tanto-development-launch COMMAND tanto-development-launch-tests)

    qt_add_executable(tanto-content-blocker-tests
        tests/content-blocking/tst_contentmatcher.cpp
    )
    target_link_libraries(tanto-content-blocker-tests PRIVATE
        tanto-content-blocking
        Qt6::Test
    )
    target_compile_definitions(tanto-content-blocker-tests PRIVATE
        TANTO_CONTENT_BLOCKER_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/content-blocking/fixtures.json"
    )
    tanto_strict_target(tanto-content-blocker-tests)
    add_test(NAME tanto-content-blocker COMMAND tanto-content-blocker-tests)

    qt_add_executable(tanto-content-blocker-service-tests
        tests/content-blocking/tst_contentblocker.cpp
    )
    target_link_libraries(tanto-content-blocker-service-tests PRIVATE
        tanto-core
        Qt6::Test
    )
    tanto_strict_target(tanto-content-blocker-service-tests)
    add_test(NAME tanto-content-blocker-service COMMAND tanto-content-blocker-service-tests)

    if(TANTO_BUILD_BROWSER)
        qt_add_executable(tanto-content-blocker-report
            tests/content-blocking/content_blocking_report.cpp
        )
        target_link_libraries(tanto-content-blocker-report PRIVATE tanto-engine-qt)
        target_compile_definitions(tanto-content-blocker-report PRIVATE
            TANTO_CONTENT_BLOCKER_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/content-blocking/fixtures.json"
            TANTO_LADYBIRD_REVISION="${tanto_ladybird_revision}"
        )
        tanto_strict_target(tanto-content-blocker-report)
        add_test(NAME tanto-content-blocker-report
            COMMAND tanto-content-blocker-report
                "${CMAKE_CURRENT_BINARY_DIR}/content-blocking-compatibility.json"
        )
    endif()

    qt_add_executable(tanto-favicon-tint-tests tests/ui/tst_favicontint.cpp)
    target_link_libraries(tanto-favicon-tint-tests PRIVATE tanto-ui Qt6::Gui Qt6::Test)
    tanto_strict_target(tanto-favicon-tint-tests)
    add_test(NAME tanto-favicon-tint COMMAND tanto-favicon-tint-tests)
    set_tests_properties(tanto-favicon-tint PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    qt_add_executable(tanto-theme-tests tests/ui/tst_themecontroller.cpp)
    target_link_libraries(tanto-theme-tests PRIVATE tanto-ui Qt6::Gui Qt6::Test)
    tanto_strict_target(tanto-theme-tests)
    add_test(NAME tanto-theme COMMAND tanto-theme-tests)
    # Resolving the theme's type family reads the host's font database, which
    # needs a GUI application and therefore a platform plugin.
    set_tests_properties(tanto-theme PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    if(APPLE)
        qt_add_executable(tanto-window-chrome-tests tests/platform/tst_windowchrome.mm)
        target_link_libraries(tanto-window-chrome-tests PRIVATE
            tanto-platform
            Qt6::Quick
            Qt6::Test
            ${TANTO_APPKIT_FRAMEWORK}
        )
        tanto_strict_target(tanto-window-chrome-tests)
        add_test(NAME tanto-window-chrome COMMAND tanto-window-chrome-tests)
    endif()

    qt_add_executable(tanto-qml-smoke src/ui-lab/main.cpp)
    target_link_libraries(tanto-qml-smoke PRIVATE
        tanto-core
        tanto-content-blocking
        tanto-ui
        tanto-quickshell-shim
        tanto-engine-api
        tanto-platform
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
    )
    tanto_configure_qml(tanto-qml-smoke "src/ui-lab/MockEngineView.qml"
        "src/ui-lab/MockEngineProfile.qml" "src/ui-lab")
    tanto_strict_target(tanto-qml-smoke)
    add_test(NAME tanto-qml-load COMMAND tanto-qml-smoke --validate-qml)
    set_tests_properties(tanto-qml-load PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    if(TANTO_BUILD_UI_LAB)
        qt_add_executable(tanto-ui-tests tests/ui/tst_ui.cpp)
        target_link_libraries(tanto-ui-tests PRIVATE
            tanto-core
            tanto-content-blocking
            tanto-ui
            tanto-platform
            tanto-quickshell-shim
            Qt6::Gui
            Qt6::Qml
            Qt6::Quick
            Qt6::QuickTest
        )
        target_compile_definitions(tanto-ui-tests PRIVATE
            TANTO_DEFAULT_KEYBINDINGS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/keybindings/default.json"
            QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/ui"
            TANTO_MOCK_ENGINE_VIEW_URL="file://${CMAKE_CURRENT_SOURCE_DIR}/src/ui-lab/MockEngineView.qml"
            TANTO_MOCK_ENGINE_PROFILE_URL="file://${CMAKE_CURRENT_SOURCE_DIR}/src/ui-lab/MockEngineProfile.qml"
            TANTO_THEME_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/themes/default.json"
            TANTO_ICON_FONT_URL="file://${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/material-symbols-rounded.ttf"
            TANTO_UI_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}/src/ui"
            TANTO_OMARCHY_IMPORT_PATH="${tanto_omarchy_root}"
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
            tanto-content-blocking
            tanto-platform
            Qt6::Gui
            Qt6::Network
            Qt6::Qml
            Qt6::Quick
            Qt6::Test
        )
        target_compile_definitions(tanto-qt-engine-contract-tests PRIVATE
            TANTO_MOCK_ENGINE_VIEW_PATH="${CMAKE_CURRENT_SOURCE_DIR}/src/ui-lab/MockEngineView.qml"
            TANTO_QT_ENGINE_VIEW_PATH="${CMAKE_CURRENT_SOURCE_DIR}/src/engine/qt/EngineView.qml"
            TANTO_QT_ENGINE_PROFILE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/src/engine/qt/EngineProfile.qml"
            TANTO_KEYBOARD_NAVIGATION_SCRIPT_PATH="${CMAKE_CURRENT_SOURCE_DIR}/src/engine/api/keyboard-navigation.js"
        )
        tanto_strict_target(tanto-qt-engine-contract-tests)
        add_test(NAME tanto-qt-engine-contract COMMAND tanto-qt-engine-contract-tests)
        set_tests_properties(tanto-qt-engine-contract PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        )
    endif()
endif()
