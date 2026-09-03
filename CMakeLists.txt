cmake_minimum_required(VERSION 3.30)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(OmawebVersion)
omaweb_resolve_version()

project(Omaweb VERSION ${OMAWEB_VERSION} LANGUAGES CXX)

omaweb_watch_git_head()
message(STATUS "Omaweb version ${OMAWEB_VERSION_STRING} (build ${OMAWEB_BUILD_NUMBER})")

include(CTest)

option(OMAWEB_BUILD_BROWSER "Build the QtWebEngine application" ON)
option(OMAWEB_BUILD_UI_LAB "Build the engine-free QML UI runner" ON)
option(OMAWEB_SOURCE_QML "Load QML from the source tree for fast iteration" ON)

set(OMAWEB_CONTENT_BLOCKER_CACHE "${CMAKE_CURRENT_SOURCE_DIR}/.cache/content-blocker/release"
    CACHE PATH "Directory containing the bootstrapped content-blocker shared library")
if(APPLE)
    set(omaweb_content_blocker_library
        "${OMAWEB_CONTENT_BLOCKER_CACHE}/libomaweb_content_blocker.dylib")
elseif(UNIX)
    set(omaweb_content_blocker_library
        "${OMAWEB_CONTENT_BLOCKER_CACHE}/libomaweb_content_blocker.so")
else()
    message(FATAL_ERROR "The content-blocker bootstrap currently supports macOS and Linux")
endif()
if(NOT EXISTS "${omaweb_content_blocker_library}")
    message(FATAL_ERROR
        "Missing bootstrapped content blocker. Run scripts/bootstrap_content_blocker.sh")
endif()
if(NOT EXISTS "${omaweb_content_blocker_library}.sha256")
    message(FATAL_ERROR "Missing checksum for the bootstrapped content blocker")
endif()
file(STRINGS "${omaweb_content_blocker_library}.sha256" omaweb_content_blocker_checksum_line
    LIMIT_COUNT 1)
string(REGEX MATCH "^[0-9a-fA-F]+" omaweb_content_blocker_expected_checksum
    "${omaweb_content_blocker_checksum_line}")
file(SHA256 "${omaweb_content_blocker_library}" omaweb_content_blocker_actual_checksum)
if(NOT omaweb_content_blocker_expected_checksum STREQUAL omaweb_content_blocker_actual_checksum)
    message(FATAL_ERROR "The bootstrapped content-blocker checksum does not match")
endif()
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/content-blocker/LADYBIRD_REVISION"
    omaweb_ladybird_revision LIMIT_COUNT 1)

# uBlock Origin's scriptlet library and substitute resources. The content
# blocker builds their descriptor files into its own binary, so the pin belongs
# to the bootstrap rather than to this build; the integrity test below is what
# keeps the copies honest.
set(omaweb_ubo_scriptlets_root "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ubo-scriptlets")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(omaweb_qt_components Concurrent Core Gui Network Qml Quick QuickControls2 Sql)
if(OMAWEB_BUILD_BROWSER)
    list(APPEND omaweb_qt_components WebEngineCore WebEngineQuick)
endif()
if(BUILD_TESTING)
    list(APPEND omaweb_qt_components Test QuickTest)
endif()

find_package(Qt6 6.11 REQUIRED COMPONENTS ${omaweb_qt_components})
qt_standard_project_setup(REQUIRES 6.11)

add_library(omaweb-adblock-rust SHARED IMPORTED GLOBAL)
set_target_properties(omaweb-adblock-rust PROPERTIES
    IMPORTED_LOCATION "${omaweb_content_blocker_library}"
)

add_library(omaweb-content-blocking STATIC
    src/content-blocking/ContentMatcher.cpp
    src/content-blocking/ContentMatcher.h
    src/content-blocking/omaweb_blocker.h
)
target_include_directories(omaweb-content-blocking PUBLIC src/content-blocking)
target_link_libraries(omaweb-content-blocking PUBLIC
    Qt6::Core
    Qt6::Network
    PRIVATE Qt6::Concurrent omaweb-adblock-rust
)

function(omaweb_strict_target target)
    target_compile_options(${target} PRIVATE
        $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wall;-Wextra;-Wpedantic>
    )
endfunction()

omaweb_strict_target(omaweb-content-blocking)

add_library(omaweb-core STATIC
    src/content-blocking/ContentBlocker.cpp
    src/content-blocking/ContentBlocker.h
    src/core/BrowserController.cpp
    src/core/BrowserController.h
    src/core/DevelopmentLaunch.cpp
    src/core/DevelopmentLaunch.h
    src/core/KeyboardNavigation.cpp
    src/core/KeyboardNavigation.h
    src/core/RetainedTab.h
    src/core/SessionSiteState.cpp
    src/core/SessionSiteState.h
    src/core/SessionStore.cpp
    src/core/SessionStore.h
    src/core/SpaceListModel.cpp
    src/core/SpaceListModel.h
    src/core/TabListModel.cpp
    src/core/TabListModel.h
    src/core/WindowManager.cpp
    src/core/WindowManager.h
)
target_include_directories(omaweb-core PUBLIC src/core src/content-blocking)
target_link_libraries(omaweb-core PUBLIC Qt6::Core Qt6::Network Qt6::Sql
    PRIVATE Qt6::Concurrent omaweb-content-blocking)
omaweb_strict_target(omaweb-core)

add_library(omaweb-ui STATIC
    src/ui/FaviconTint.cpp
    src/ui/FaviconTint.h
    src/ui/KitTheme.cpp
    src/ui/KitTheme.h
    src/ui/SystemClipboard.cpp
    src/ui/SystemClipboard.h
    src/ui/ThemeController.cpp
    src/ui/ThemeController.h
)
target_include_directories(omaweb-ui PUBLIC src/ui)
target_link_libraries(omaweb-ui PUBLIC Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick)
omaweb_strict_target(omaweb-ui)

# The vendored Omarchy component kit imports Quickshell types. Omaweb supplies
# the small subset the kit's shared components need instead of patching the
# copies, so a sync stays a review of upstream's diff. See
# third_party/omarchy-shell/README.md.
add_library(omaweb-quickshell-shim STATIC
    src/ui/quickshell-shim/Quickshell.cpp
    src/ui/quickshell-shim/Quickshell.h
    src/ui/quickshell-shim/QuickshellIo.cpp
    src/ui/quickshell-shim/QuickshellIo.h
)
target_include_directories(omaweb-quickshell-shim PUBLIC src/ui/quickshell-shim)
target_link_libraries(omaweb-quickshell-shim PUBLIC Qt6::Core Qt6::Qml Qt6::QuickControls2)
set_target_properties(omaweb-quickshell-shim PROPERTIES AUTOMOC ON)

# The shim's qmldir files ride in the shim's own resource, so every target that
# links it resolves `import Quickshell` the same way whether the rest of the QML
# comes from the source tree or from a resource.
qt_add_resources(omaweb-quickshell-shim "omaweb_quickshell_shim_qml"
    PREFIX "/omaweb/quickshell-shim"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/quickshell-shim/qml"
    FILES
        src/ui/quickshell-shim/qml/Quickshell/qmldir
        src/ui/quickshell-shim/qml/Quickshell/Io/qmldir
)
omaweb_strict_target(omaweb-quickshell-shim)

# The manifest is the single list of vendored files: the resource build, the
# integrity test, and scripts/sync_omarchy_ui.py all read it.
set(omaweb_omarchy_root "${CMAKE_CURRENT_SOURCE_DIR}/third_party/omarchy-shell")
file(READ "${omaweb_omarchy_root}/MANIFEST.json" omaweb_omarchy_manifest)
string(JSON omaweb_omarchy_manifest_files GET "${omaweb_omarchy_manifest}" files)
string(JSON omaweb_omarchy_file_count LENGTH "${omaweb_omarchy_manifest_files}")
set(omaweb_omarchy_files "")
if(omaweb_omarchy_file_count GREATER 0)
    math(EXPR omaweb_omarchy_last_file "${omaweb_omarchy_file_count} - 1")
    foreach(index RANGE 0 ${omaweb_omarchy_last_file})
        string(JSON omaweb_omarchy_file MEMBER "${omaweb_omarchy_manifest_files}" ${index})
        # Only the QML module tree goes into the resource; the manifest also
        # tracks upstream's license text.
        if(omaweb_omarchy_file MATCHES "^qs/")
            list(APPEND omaweb_omarchy_files "third_party/omarchy-shell/${omaweb_omarchy_file}")
        endif()
    endforeach()
endif()

add_library(omaweb-engine-api STATIC
    src/engine/api/EngineCapabilities.h
    src/engine/api/EngineViewContract.cpp
    src/engine/api/EngineViewContract.h
)
target_include_directories(omaweb-engine-api PUBLIC src/engine/api)
target_link_libraries(omaweb-engine-api PUBLIC Qt6::Core)
omaweb_strict_target(omaweb-engine-api)

if(OMAWEB_BUILD_BROWSER)
    add_library(omaweb-engine-qt STATIC
        src/engine/qt/QtContentBlocker.cpp
        src/engine/qt/QtContentBlocker.h
        src/engine/qt/QtCookiePolicy.cpp
        src/engine/qt/QtCookiePolicy.h
    )
    target_sources(omaweb-engine-qt PUBLIC src/engine/qt/EngineView.qml)
    target_include_directories(omaweb-engine-qt PUBLIC src/engine/qt)
    target_link_libraries(omaweb-engine-qt PUBLIC
        omaweb-core
        omaweb-engine-api
        Qt6::WebEngineCore
        Qt6::WebEngineQuick
    )
    omaweb_strict_target(omaweb-engine-qt)
endif()

add_library(omaweb-platform STATIC
    src/platform/ExternalProtocolHandler.cpp
    src/platform/ExternalProtocolHandler.h
    src/platform/OmarchyTheme.cpp
    src/platform/OmarchyTheme.h
    src/platform/PagePrinter.cpp
    src/platform/PagePrinter.h
    src/platform/ProcessResources.cpp
    src/platform/ProcessResources.h
    src/platform/SystemNotifier.cpp
    src/platform/SystemNotifier.h
    src/platform/WindowChrome.h
)
target_include_directories(omaweb-platform PUBLIC src/platform)
target_link_libraries(omaweb-platform PUBLIC Qt6::Gui Qt6::Qml Qt6::Quick)
if(APPLE)
    enable_language(OBJCXX)
    find_library(OMAWEB_APPKIT_FRAMEWORK AppKit REQUIRED)
    find_library(OMAWEB_QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
    # Quartz is the umbrella that carries PDFKit, which draws and prints the
    # PDF the engine renders a page into.
    find_library(OMAWEB_QUARTZ_FRAMEWORK Quartz REQUIRED)
    # The desktop's notification service, which is a bundle service: it
    # identifies the notifying application by its bundle identifier.
    find_library(OMAWEB_USER_NOTIFICATIONS_FRAMEWORK UserNotifications REQUIRED)
    target_sources(omaweb-platform PRIVATE
        src/platform/MacExternalProtocolHandler.mm
        src/platform/MacPagePrinter.mm
        src/platform/MacSystemNotifier.mm
        src/platform/MacWindowChrome.mm
    )
    target_link_libraries(omaweb-platform PRIVATE
        ${OMAWEB_APPKIT_FRAMEWORK}
        ${OMAWEB_QUARTZCORE_FRAMEWORK}
        ${OMAWEB_QUARTZ_FRAMEWORK}
        ${OMAWEB_USER_NOTIFICATIONS_FRAMEWORK}
    )
else()
    target_sources(omaweb-platform PRIVATE
        src/platform/WindowChrome.cpp
    )
endif()
omaweb_strict_target(omaweb-platform)

set(omaweb_ui_files
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
    src/ui/RetainedEngines.qml
    src/ui/SectionLabel.qml
    src/ui/SiteInformationPanel.qml
    src/ui/MultilineField.qml
    src/ui/SettingDropdown.qml
    src/ui/SettingField.qml
    src/ui/SettingRow.qml
    src/ui/SettingToggle.qml
    src/ui/SettingsPage.qml
    src/ui/PanelResizer.qml
    src/ui/SiteNotifications.qml
    src/ui/SiteTile.qml
    src/ui/SpaceOutline.qml
    src/ui/SpaceProfiles.qml
    src/ui/StartPage.qml
    src/ui/TabEngineHost.qml
    src/ui/TabRow.qml
)

function(omaweb_configure_qml target engine_view engine_profile engine_directory)
    target_compile_definitions(${target} PRIVATE
        OMAWEB_VERSION="${OMAWEB_VERSION_STRING}"
        OMAWEB_UI_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}/src/ui"
    )

    if(OMAWEB_SOURCE_QML)
        target_compile_definitions(${target} PRIVATE
            OMAWEB_OMARCHY_IMPORT_PATH="${omaweb_omarchy_root}"
            OMAWEB_MAIN_QML_URL="file://${CMAKE_CURRENT_SOURCE_DIR}/src/ui/Main.qml"
            OMAWEB_ENGINE_VIEW_URL="file://${CMAKE_CURRENT_SOURCE_DIR}/${engine_view}"
            OMAWEB_ENGINE_PROFILE_URL="file://${CMAKE_CURRENT_SOURCE_DIR}/${engine_profile}"
            OMAWEB_THEME_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/themes/default.json"
            OMAWEB_OMARCHY_TEMPLATE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/integrations/omarchy/omaweb.json.tpl"
            OMAWEB_ICON_FONT_URL="file://${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/material-symbols-rounded.ttf"
            OMAWEB_DEFAULT_KEYBINDINGS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/keybindings/default.json"
            OMAWEB_KEYBOARD_NAVIGATION_SCRIPT_PATH="${CMAKE_CURRENT_SOURCE_DIR}/src/engine/api/keyboard-navigation.js"
        )
    else()
        set_source_files_properties("${engine_view}" PROPERTIES QT_RESOURCE_ALIAS "EngineView.qml")
        set_source_files_properties("${engine_profile}" PROPERTIES QT_RESOURCE_ALIAS "EngineProfile.qml")
        qt_add_resources(${target} "omaweb_ui"
            PREFIX "/omaweb/ui"
            BASE "${CMAKE_CURRENT_SOURCE_DIR}/src/ui"
            FILES ${omaweb_ui_files}
        )
        # `/qt/qml` is already on the engine's import path, so the vendored
        # `qs.Commons` / `qs.Ui` modules resolve from the resource unchanged.
        qt_add_resources(${target} "omaweb_omarchy_ui"
            PREFIX "/qt/qml"
            BASE "${omaweb_omarchy_root}"
            FILES ${omaweb_omarchy_files}
        )
        qt_add_resources(${target} "omaweb_engine_view"
            PREFIX "/omaweb/engine"
            BASE "${CMAKE_CURRENT_SOURCE_DIR}/${engine_directory}"
            FILES "${engine_view}"
                "${engine_profile}"
        )
        qt_add_resources(${target} "omaweb_theme"
            PREFIX "/omaweb/themes"
            BASE "${CMAKE_CURRENT_SOURCE_DIR}/assets/themes"
            FILES assets/themes/default.json
        )
        # The Omarchy theme template travels in the binary, because the reader's
        # copy of it is written on the first start of a new install.
        qt_add_resources(${target} "omaweb_omarchy_template"
            PREFIX "/omaweb/omarchy"
            BASE "${CMAKE_CURRENT_SOURCE_DIR}/integrations/omarchy"
            FILES integrations/omarchy/omaweb.json.tpl
        )
        qt_add_resources(${target} "omaweb_fonts"
            PREFIX "/omaweb/fonts"
            BASE "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts"
            FILES assets/fonts/material-symbols-rounded.ttf
        )
        qt_add_resources(${target} "omaweb_keybindings"
            PREFIX "/omaweb/keybindings"
            BASE "${CMAKE_CURRENT_SOURCE_DIR}/assets/keybindings"
            FILES assets/keybindings/default.json
        )
        qt_add_resources(${target} "omaweb_keyboard_navigation_script"
            PREFIX "/omaweb/engine-api"
            BASE "${CMAKE_CURRENT_SOURCE_DIR}/src/engine/api"
            FILES src/engine/api/keyboard-navigation.js
        )
        target_compile_definitions(${target} PRIVATE
            OMAWEB_OMARCHY_IMPORT_PATH="qrc:/qt/qml"
            OMAWEB_MAIN_QML_URL="qrc:/omaweb/ui/Main.qml"
            OMAWEB_ENGINE_VIEW_URL="qrc:/omaweb/engine/EngineView.qml"
            OMAWEB_ENGINE_PROFILE_URL="qrc:/omaweb/engine/EngineProfile.qml"
            OMAWEB_THEME_PATH=":/omaweb/themes/default.json"
            OMAWEB_OMARCHY_TEMPLATE_PATH=":/omaweb/omarchy/omaweb.json.tpl"
            OMAWEB_ICON_FONT_URL="qrc:/omaweb/fonts/material-symbols-rounded.ttf"
            OMAWEB_DEFAULT_KEYBINDINGS_PATH=":/omaweb/keybindings/default.json"
            OMAWEB_KEYBOARD_NAVIGATION_SCRIPT_PATH=":/omaweb/engine-api/keyboard-navigation.js"
        )
    endif()
endfunction()

if(OMAWEB_BUILD_BROWSER)
    qt_add_executable(omaweb MACOSX_BUNDLE src/main.cpp)
    target_link_libraries(omaweb PRIVATE
        omaweb-core
        omaweb-ui
        omaweb-quickshell-shim
        omaweb-engine-qt
        omaweb-platform
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
    )
    omaweb_configure_qml(omaweb "src/engine/qt/EngineView.qml"
        "src/engine/qt/EngineProfile.qml" "src/engine/qt")
    omaweb_strict_target(omaweb)

    set_target_properties(omaweb PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME "Omaweb"
        MACOSX_BUNDLE_GUI_IDENTIFIER "dev.omaweb.browser"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${OMAWEB_BUILD_NUMBER}"
        WIN32_EXECUTABLE TRUE
    )
endif()

if(OMAWEB_BUILD_UI_LAB)
    qt_add_executable(omaweb-ui-lab MACOSX_BUNDLE src/ui-lab/main.cpp)
    target_link_libraries(omaweb-ui-lab PRIVATE
        omaweb-core
        omaweb-content-blocking
        omaweb-ui
        omaweb-quickshell-shim
        omaweb-engine-api
        omaweb-platform
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
    )
    omaweb_configure_qml(omaweb-ui-lab "src/ui-lab/MockEngineView.qml"
        "src/ui-lab/MockEngineProfile.qml" "src/ui-lab")
    omaweb_strict_target(omaweb-ui-lab)
    set_target_properties(omaweb-ui-lab PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME "Omaweb UI Lab"
        MACOSX_BUNDLE_GUI_IDENTIFIER "dev.omaweb.browser.ui-lab"
    )
endif()

if(BUILD_TESTING)
    add_test(NAME omaweb-shared-qml-boundary
        COMMAND ${CMAKE_COMMAND}
            -DOMAWEB_UI_DIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}/src/ui
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/check_shared_qml.cmake
    )

    add_test(NAME omaweb-vendored-omarchy-ui
        COMMAND ${CMAKE_COMMAND}
            -DOMAWEB_VENDOR_ROOT=${omaweb_omarchy_root}
            -DOMAWEB_VENDOR_TRACKED=qs/*
            -DOMAWEB_VENDOR_NAME=Omarchy\ kit
            "-DOMAWEB_VENDOR_ADVICE=Adapt in src/ui, or run scripts/sync_omarchy_ui.py --sync to restore them."
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/check_vendored_tree.cmake
    )

    add_test(NAME omaweb-vendored-ubo-scriptlets
        COMMAND ${CMAKE_COMMAND}
            -DOMAWEB_VENDOR_ROOT=${omaweb_ubo_scriptlets_root}
            -DOMAWEB_VENDOR_TRACKED=src/*
            -DOMAWEB_VENDOR_NAME=uBO\ scriptlets
            "-DOMAWEB_VENDOR_ADVICE=Run scripts/sync_ubo_scriptlets.py --sync to restore them."
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/check_vendored_tree.cmake
    )

    # The drift report and the issue it turns into, with GitHub stubbed out.
    # Python is the sync script's language, not a build dependency, so the test
    # only exists where an interpreter is already present.
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME omaweb-omarchy-drift
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/scripts/tst_omarchy_drift.py
        )
        # The release notes group Conventional Commit subjects, and a grouping
        # that drops the scoped half still produces notes that read as whole.
        add_test(NAME omaweb-release-notes
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/scripts/tst_release_notes.py
        )
    endif()

    qt_add_executable(omaweb-core-tests tests/core/tst_browsercontroller.cpp)
    target_link_libraries(omaweb-core-tests PRIVATE omaweb-core Qt6::Test)
    omaweb_strict_target(omaweb-core-tests)
    add_test(NAME omaweb-core COMMAND omaweb-core-tests)

    qt_add_executable(omaweb-keyboard-navigation-tests
        tests/core/tst_keyboardnavigation.cpp
    )
    target_link_libraries(omaweb-keyboard-navigation-tests PRIVATE omaweb-core Qt6::Test)
    omaweb_strict_target(omaweb-keyboard-navigation-tests)
    add_test(NAME omaweb-keyboard-navigation COMMAND omaweb-keyboard-navigation-tests)

    qt_add_executable(omaweb-development-launch-tests
        tests/core/tst_developmentlaunch.cpp
    )
    target_link_libraries(omaweb-development-launch-tests PRIVATE omaweb-core Qt6::Test)
    omaweb_strict_target(omaweb-development-launch-tests)
    add_test(NAME omaweb-development-launch COMMAND omaweb-development-launch-tests)

    qt_add_executable(omaweb-content-blocker-tests
        tests/content-blocking/tst_contentmatcher.cpp
    )
    target_link_libraries(omaweb-content-blocker-tests PRIVATE
        omaweb-content-blocking
        Qt6::Test
    )
    target_compile_definitions(omaweb-content-blocker-tests PRIVATE
        OMAWEB_CONTENT_BLOCKER_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/content-blocking/fixtures.json"
    )
    omaweb_strict_target(omaweb-content-blocker-tests)
    add_test(NAME omaweb-content-blocker COMMAND omaweb-content-blocker-tests)

    qt_add_executable(omaweb-content-blocker-service-tests
        tests/content-blocking/tst_contentblocker.cpp
    )
    target_link_libraries(omaweb-content-blocker-service-tests PRIVATE
        omaweb-core
        Qt6::Test
    )
    omaweb_strict_target(omaweb-content-blocker-service-tests)
    add_test(NAME omaweb-content-blocker-service COMMAND omaweb-content-blocker-service-tests)

    if(OMAWEB_BUILD_BROWSER)
        qt_add_executable(omaweb-content-blocker-report
            tests/content-blocking/content_blocking_report.cpp
        )
        target_link_libraries(omaweb-content-blocker-report PRIVATE omaweb-engine-qt)
        target_compile_definitions(omaweb-content-blocker-report PRIVATE
            OMAWEB_CONTENT_BLOCKER_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/content-blocking/fixtures.json"
            OMAWEB_LADYBIRD_REVISION="${omaweb_ladybird_revision}"
        )
        omaweb_strict_target(omaweb-content-blocker-report)
        add_test(NAME omaweb-content-blocker-report
            COMMAND omaweb-content-blocker-report
                "${CMAKE_CURRENT_BINARY_DIR}/content-blocking-compatibility.json"
        )
    endif()

    qt_add_executable(omaweb-platform-services-tests
        tests/platform/tst_platformservices.cpp
    )
    target_link_libraries(omaweb-platform-services-tests PRIVATE
        omaweb-platform
        Qt6::Gui
        Qt6::Test
    )
    omaweb_strict_target(omaweb-platform-services-tests)
    add_test(NAME omaweb-platform-services COMMAND omaweb-platform-services-tests)
    # The notification service is a window-server service, and the seam's
    # contract is what it does without one.
    set_tests_properties(omaweb-platform-services PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    # Following the desktop's theme writes into the reader's Omarchy
    # configuration, so the seam takes its directories as arguments and the
    # test hands it a temporary home. That runs on macOS too, which is where
    # the shipped template is reviewed.
    qt_add_executable(omaweb-omarchy-theme-tests tests/platform/tst_omarchytheme.cpp)
    target_link_libraries(omaweb-omarchy-theme-tests PRIVATE omaweb-platform Qt6::Test)
    target_compile_definitions(omaweb-omarchy-theme-tests PRIVATE
        OMAWEB_OMARCHY_TEMPLATE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/integrations/omarchy/omaweb.json.tpl"
    )
    omaweb_strict_target(omaweb-omarchy-theme-tests)
    add_test(NAME omaweb-omarchy-theme COMMAND omaweb-omarchy-theme-tests)

    qt_add_executable(omaweb-favicon-tint-tests tests/ui/tst_favicontint.cpp)
    target_link_libraries(omaweb-favicon-tint-tests PRIVATE omaweb-ui Qt6::Gui Qt6::Test)
    omaweb_strict_target(omaweb-favicon-tint-tests)
    add_test(NAME omaweb-favicon-tint COMMAND omaweb-favicon-tint-tests)
    set_tests_properties(omaweb-favicon-tint PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    qt_add_executable(omaweb-theme-tests tests/ui/tst_themecontroller.cpp)
    target_link_libraries(omaweb-theme-tests PRIVATE omaweb-ui Qt6::Gui Qt6::Test)
    omaweb_strict_target(omaweb-theme-tests)
    add_test(NAME omaweb-theme COMMAND omaweb-theme-tests)
    # Resolving the theme's type family reads the host's font database, which
    # needs a GUI application and therefore a platform plugin.
    set_tests_properties(omaweb-theme PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    if(APPLE)
        qt_add_executable(omaweb-window-chrome-tests tests/platform/tst_windowchrome.mm)
        target_link_libraries(omaweb-window-chrome-tests PRIVATE
            omaweb-platform
            Qt6::Quick
            Qt6::Test
            ${OMAWEB_APPKIT_FRAMEWORK}
        )
        omaweb_strict_target(omaweb-window-chrome-tests)
        add_test(NAME omaweb-window-chrome COMMAND omaweb-window-chrome-tests)
    endif()

    qt_add_executable(omaweb-qml-smoke src/ui-lab/main.cpp)
    target_link_libraries(omaweb-qml-smoke PRIVATE
        omaweb-core
        omaweb-content-blocking
        omaweb-ui
        omaweb-quickshell-shim
        omaweb-engine-api
        omaweb-platform
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
    )
    omaweb_configure_qml(omaweb-qml-smoke "src/ui-lab/MockEngineView.qml"
        "src/ui-lab/MockEngineProfile.qml" "src/ui-lab")
    omaweb_strict_target(omaweb-qml-smoke)
    add_test(NAME omaweb-qml-load COMMAND omaweb-qml-smoke --validate-qml)
    set_tests_properties(omaweb-qml-load PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    # The same load with a decoy Quickshell on the import path, which is the
    # shape of an Omarchy host: the shim's qmldir has to win over a real module
    # sitting in Qt's qml directory, or the kit's imports go looking for a
    # plugin that is not there.
    add_test(NAME omaweb-qml-load-over-quickshell COMMAND omaweb-qml-smoke --validate-qml)
    set_tests_properties(omaweb-qml-load-over-quickshell PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QML_IMPORT_PATH=${CMAKE_CURRENT_SOURCE_DIR}/tests/qml/quickshell-decoy;QML2_IMPORT_PATH=${CMAKE_CURRENT_SOURCE_DIR}/tests/qml/quickshell-decoy"
    )

    if(OMAWEB_BUILD_UI_LAB)
        qt_add_executable(omaweb-ui-tests tests/ui/tst_ui.cpp)
        target_link_libraries(omaweb-ui-tests PRIVATE
            omaweb-core
            omaweb-content-blocking
            omaweb-ui
            omaweb-platform
            omaweb-quickshell-shim
            Qt6::Gui
            Qt6::Qml
            Qt6::Quick
            Qt6::QuickTest
        )
        target_compile_definitions(omaweb-ui-tests PRIVATE
            OMAWEB_VERSION="${OMAWEB_VERSION_STRING}"
            OMAWEB_DEFAULT_KEYBINDINGS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/keybindings/default.json"
            QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/ui"
            OMAWEB_MOCK_ENGINE_VIEW_URL="file://${CMAKE_CURRENT_SOURCE_DIR}/src/ui-lab/MockEngineView.qml"
            OMAWEB_MOCK_ENGINE_PROFILE_URL="file://${CMAKE_CURRENT_SOURCE_DIR}/src/ui-lab/MockEngineProfile.qml"
            OMAWEB_THEME_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/themes/default.json"
            OMAWEB_ICON_FONT_URL="file://${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/material-symbols-rounded.ttf"
            OMAWEB_UI_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}/src/ui"
            OMAWEB_OMARCHY_IMPORT_PATH="${omaweb_omarchy_root}"
        )
        omaweb_strict_target(omaweb-ui-tests)
        add_test(NAME omaweb-ui COMMAND omaweb-ui-tests)
        set_tests_properties(omaweb-ui PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        )
    endif()

    if(OMAWEB_BUILD_BROWSER)
        qt_add_executable(omaweb-qt-engine-contract-tests
            tests/engine/tst_qtenginecontract.cpp
        )
        target_link_libraries(omaweb-qt-engine-contract-tests PRIVATE
            omaweb-engine-qt
            omaweb-content-blocking
            omaweb-platform
            Qt6::Gui
            Qt6::Network
            Qt6::Qml
            Qt6::Quick
            Qt6::Test
        )
        target_compile_definitions(omaweb-qt-engine-contract-tests PRIVATE
            OMAWEB_MOCK_ENGINE_VIEW_PATH="${CMAKE_CURRENT_SOURCE_DIR}/src/ui-lab/MockEngineView.qml"
            OMAWEB_QT_ENGINE_VIEW_PATH="${CMAKE_CURRENT_SOURCE_DIR}/src/engine/qt/EngineView.qml"
            OMAWEB_QT_ENGINE_PROFILE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/src/engine/qt/EngineProfile.qml"
            OMAWEB_MOCK_ENGINE_PROFILE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/src/ui-lab/MockEngineProfile.qml"
            OMAWEB_KEYBOARD_NAVIGATION_SCRIPT_PATH="${CMAKE_CURRENT_SOURCE_DIR}/src/engine/api/keyboard-navigation.js"
            OMAWEB_UNTRUSTED_CERTIFICATE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/tests/engine/certificates/untrusted-local.crt"
            OMAWEB_UNTRUSTED_KEY_PATH="${CMAKE_CURRENT_SOURCE_DIR}/tests/engine/certificates/untrusted-local.key"
        )
        omaweb_strict_target(omaweb-qt-engine-contract-tests)
        add_test(NAME omaweb-qt-engine-contract COMMAND omaweb-qt-engine-contract-tests)
        set_tests_properties(omaweb-qt-engine-contract PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        )
    endif()
endif()

# The vendored kit imports Quickshell, which omaweb-quickshell-shim registers
# from C++. On a host that also has the real Quickshell installed in Qt's qml
# directory, qmlimportscanner finds those qmldir files and Qt warns that the
# plugin link targets they name are missing from this scope. Only static plugin
# targets are ever linked, so the scan has nothing to do on a shared Qt build.
# Qt runs the scan from a finalizer, so setting the property here still takes
# effect for every executable declared above.
get_property(omaweb_directory_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(omaweb_target IN LISTS omaweb_directory_targets)
    get_target_property(omaweb_target_type ${omaweb_target} TYPE)
    if(omaweb_target_type STREQUAL "EXECUTABLE")
        set_target_properties(${omaweb_target} PROPERTIES QT_QML_MODULE_NO_IMPORT_SCAN TRUE)
    endif()
endforeach()
