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

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/security/baseline.json" omaweb_security_baseline)
string(JSON omaweb_approved_qtwebengine GET "${omaweb_security_baseline}" qtwebengine)
string(JSON omaweb_approved_chromium GET "${omaweb_security_baseline}" chromium)
string(JSON omaweb_approved_chromium_security_patch
    GET "${omaweb_security_baseline}" chromiumSecurityPatch)
message(STATUS "Omaweb approved engine baseline: QtWebEngine "
    "${omaweb_approved_qtwebengine} / Chromium ${omaweb_approved_chromium_security_patch}")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(omaweb_qt_components Concurrent Core Gui Network Qml Quick QuickControls2 Sql)
if(UNIX AND NOT APPLE)
    # The desktop's notification service is a session-bus service, so the Linux
    # platform layer needs D-Bus. macOS reaches its own through AppKit.
    #
    # `GuiPrivate` is one call in `LinuxPortalWindow.cpp`, which says why it is
    # the only route to the name a portal takes for a parent window.
    list(APPEND omaweb_qt_components DBus GuiPrivate)
endif()
if(OMAWEB_BUILD_BROWSER)
    list(APPEND omaweb_qt_components WebEngineCore WebEngineQuick)
endif()
if(BUILD_TESTING)
    list(APPEND omaweb_qt_components Test QuickTest)
endif()

# Qt warns on every configure that a private module ties the build to one Qt
# version and may crash against another. The warning is right about what it
# describes and does not describe how Omaweb is built: it is compiled from
# source against the Qt it links, and the package depends on that Qt, so the
# two cannot come apart in a distributed build. Left on, it is a warning nobody
# can act on, printed every time, which is how people learn to read past CMake
# warnings that do matter. It is turned off for this call and turned back on
# after it, so a private module added later is warned about the same way this
# one was.
set(QT_NO_PRIVATE_MODULE_WARNING ON)
find_package(Qt6 6.11 REQUIRED COMPONENTS ${omaweb_qt_components})
set(QT_NO_PRIVATE_MODULE_WARNING OFF)
qt_standard_project_setup(REQUIRES 6.11)

add_library(omaweb-adblock-rust SHARED IMPORTED GLOBAL)
set_target_properties(omaweb-adblock-rust PROPERTIES
    IMPORTED_LOCATION "${omaweb_content_blocker_library}"
    # The bootstrapped library carries no soname, so linking it by path would
    # record that path as the dependency itself and no runpath would ever be
    # consulted. An installed copy would then look for the library in whatever
    # directory happened to build it. Saying so makes CMake link it by name and
    # leave finding it to the runpath.
    IMPORTED_NO_SONAME TRUE
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

function(omaweb_declarative_fixture_target target)
    # Fixture declarations intentionally omit fields whose domain defaults are under test.
    target_compile_options(${target} PRIVATE
        $<$<CXX_COMPILER_ID:Clang>:-Wno-missing-designated-field-initializers>
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
    src/core/DownloadPolicy.cpp
    src/core/DownloadPolicy.h
    src/core/Downloads.cpp
    src/core/Downloads.h
    src/core/HardwareVideoDecode.cpp
    src/core/HardwareVideoDecode.h
    src/core/HistoryQuery.cpp
    src/core/HistoryQuery.h
    src/core/HistorySearch.cpp
    src/core/HistorySearch.h
    src/core/KeyboardNavigation.cpp
    src/core/KeyboardNavigation.h
    src/core/LaunchRequest.cpp
    src/core/LaunchRequest.h
    src/core/RetainedTab.h
    src/core/SessionSiteState.cpp
    src/core/SessionSiteState.h
    src/core/PrivateSessionStore.cpp
    src/core/PrivateSessionStore.h
    src/core/SessionStore.h
    src/core/SqliteSessionStore.cpp
    src/core/SqliteSessionStore.h
    src/core/SpaceListModel.cpp
    src/core/SpaceListModel.h
    src/core/SpaceStorage.cpp
    src/core/SpaceStorage.h
    src/core/TabListModel.cpp
    src/core/TabListModel.h
    src/core/WindowCapabilities.h
    src/core/WindowManager.cpp
    src/core/WindowManager.h
)
target_include_directories(omaweb-core PUBLIC src/core src/content-blocking)
target_link_libraries(omaweb-core PUBLIC Qt6::Core Qt6::Network Qt6::Qml Qt6::Sql
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
set(omaweb_omarchy_commons_files "")
set(omaweb_omarchy_ui_files "")
if(omaweb_omarchy_file_count GREATER 0)
    math(EXPR omaweb_omarchy_last_file "${omaweb_omarchy_file_count} - 1")
    foreach(index RANGE 0 ${omaweb_omarchy_last_file})
        string(JSON omaweb_omarchy_file MEMBER "${omaweb_omarchy_manifest_files}" ${index})
        # Only the QML module tree is built, one module per directory; the
        # manifest also tracks upstream's license text. The qmldir files are
        # the module boundary and travel with their module separately.
        if(omaweb_omarchy_file MATCHES "^qs/Commons/.*\\.(qml|js)$")
            list(APPEND omaweb_omarchy_commons_files
                "third_party/omarchy-shell/${omaweb_omarchy_file}")
        elseif(omaweb_omarchy_file MATCHES "^qs/Ui/.*\\.(qml|js)$")
            list(APPEND omaweb_omarchy_ui_files "third_party/omarchy-shell/${omaweb_omarchy_file}")
        endif()
    endforeach()
endif()

add_library(omaweb-engine-api STATIC
    "src/engine/api/EngineCapabilities.cpp"
    src/engine/api/ContentBlockerContract.cpp
    src/engine/api/ContentBlockerContract.h
    src/engine/api/EngineCapabilities.h
    src/engine/api/EngineViewContract.cpp
    src/engine/api/EngineViewContract.h
    src/engine/api/QmlContract.cpp
    src/engine/api/QmlContract.h
)
target_include_directories(omaweb-engine-api PUBLIC src/engine/api)
target_link_libraries(omaweb-engine-api PUBLIC Qt6::Core Qt6::Qml)
omaweb_strict_target(omaweb-engine-api)

if(OMAWEB_BUILD_BROWSER)
    add_library(omaweb-engine-qt STATIC
        src/engine/qt/QtContentBlocker.cpp
        src/engine/qt/QtContentBlocker.h
        src/engine/qt/QtCookiePolicy.cpp
        src/engine/qt/QtCookiePolicy.h
        src/engine/qt/QtHeldDownloads.cpp
        src/engine/qt/QtHeldDownloads.h
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
    src/platform/DefaultBrowser.cpp
    src/platform/DefaultBrowser.h
    src/platform/ExternalProtocolHandler.cpp
    src/platform/ExternalProtocolHandler.h
    src/platform/InputMethod.cpp
    src/platform/InputMethod.h
    src/platform/OmarchyTheme.cpp
    src/platform/OmarchyTheme.h
    src/platform/PagePrinter.cpp
    src/platform/PagePrinter.h
    src/platform/PortalWindow.h
    src/platform/ProcessResources.cpp
    src/platform/ProcessResources.h
    src/platform/RunningBrowser.h
    src/platform/RuntimeSecurity.cpp
    src/platform/RuntimeSecurity.h
    src/platform/SavedDownload.cpp
    src/platform/SavedDownload.h
    src/platform/SystemNotifier.cpp
    src/platform/SystemNotifier.h
    src/platform/WindowChrome.h
)
target_include_directories(omaweb-platform PUBLIC src/platform)
target_link_libraries(omaweb-platform PUBLIC Qt6::Gui Qt6::Qml Qt6::Quick)
target_compile_definitions(omaweb-platform PUBLIC
    OMAWEB_APPROVED_QTWEBENGINE="${omaweb_approved_qtwebengine}"
    OMAWEB_APPROVED_CHROMIUM="${omaweb_approved_chromium}"
    OMAWEB_APPROVED_CHROMIUM_SECURITY_PATCH="${omaweb_approved_chromium_security_patch}"
)
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
        src/platform/RunningBrowser.cpp
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
        src/platform/LinuxDefaultBrowser.cpp
        src/platform/LinuxPagePrinter.cpp
        src/platform/LinuxPortalWindow.cpp
        src/platform/LinuxRunningBrowser.cpp
        src/platform/LinuxSystemNotifier.cpp
        src/platform/WindowChrome.cpp
    )
    # `Qt6::GuiPrivate` is here for one call, and `LinuxPortalWindow.cpp` says
    # why: the name a portal takes for a parent window is Qt's to give and Qt
    # does not offer it publicly.
    #
    # A Qt that renames or removes it breaks the build, which is the harmless
    # case. A Qt that keeps the name and moves it in the vtable does not, and
    # printing would then call whatever took its place. That is the real cost
    # of the private call, it is confined to printing, and the answer to it is
    # a rebuild against the Qt in front of it rather than anything in the code.
    target_link_libraries(omaweb-platform PRIVATE Qt6::DBus Qt6::GuiPrivate)
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
    src/ui/DownloadMark.qml
    src/ui/FindBar.qml
    src/ui/HistoryPage.qml
    src/ui/KeyHint.qml
    src/ui/KeyMap.qml
    src/ui/NavigationCluster.qml
    src/ui/NoticeBox.qml
    src/ui/ActionButton.qml
    src/ui/CommandDialog.qml
    src/ui/DialogPanel.qml
    src/ui/PageBackdrop.qml
    src/ui/PageLoadingIndicator.qml
    src/ui/PageNotice.qml
    src/ui/PageQuestionBar.qml
    src/ui/PagePromptBar.qml
    src/ui/RefusalTally.qml
    src/ui/RetainedEngines.qml
    src/ui/SectionLabel.qml
    src/ui/SheetInsets.qml
    src/ui/SiteInformationPanel.qml
    src/ui/MultilineField.qml
    src/ui/ClearBrowsingDataDialog.qml
    src/ui/SettingCheckbox.qml
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

# One QML module compiled ahead of time into a static library named after its
# URI. Qt binds a module to a single target, so every module is a library of
# its own. The value handed back is the module's plugin: the plugin's
# constructor names the compiled units and the resource, which is what keeps
# an archive's otherwise unreferenced objects in the executable's link.
#
# A module that carries its own qmldir hands it in as QMLDIR and it rides
# along unchanged; the others get one generated from their file list. Nothing
# in either case is written back to the source tree.
function(omaweb_compiled_qml_module out_target uri)
    cmake_parse_arguments(PARSE_ARGV 2 arg "" "QMLDIR" "QML_FILES")
    string(TOLOWER "${uri}" module_name)
    string(REPLACE "." "-" module_name "${module_name}")
    set(target "omaweb-qml-${module_name}")
    set("${out_target}" "${target}plugin" PARENT_SCOPE)
    # Every executable that embeds QML asks for the same modules, and a module
    # is built once. A second request has to be the same request, or one
    # executable would silently ship the other's file list.
    if(TARGET "${target}")
        get_target_property(existing_files "${target}" OMAWEB_QML_FILES)
        if(NOT existing_files STREQUAL arg_QML_FILES)
            message(FATAL_ERROR "QML module ${uri} was already defined with different files")
        endif()
        return()
    endif()

    # Every file sits at the module root in the resource, whatever its source
    # path, and the module's qmldir refers to each by that name.
    foreach(file IN LISTS arg_QML_FILES arg_QMLDIR)
        get_filename_component(file_name "${file}" NAME)
        set_source_files_properties("${file}" PROPERTIES QT_RESOURCE_ALIAS "${file_name}")
    endforeach()
    set(qmldir_arguments "")
    if(arg_QMLDIR)
        set(qmldir_arguments NO_GENERATE_QMLDIR RESOURCES "${arg_QMLDIR}")
    endif()

    # Qt places a library's module at the binary directory itself, and two
    # modules cannot share one. The tooling copies live under `qml/` instead,
    # laid out by URI, so the compiler resolves one module's imports of another.
    string(REPLACE "." "/" module_path "${uri}")
    add_library("${target}" STATIC)
    qt_add_qml_module("${target}"
        URI "${uri}"
        VERSION 1.0
        OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/qml/${module_path}"
        NO_LINT
        QML_FILES ${arg_QML_FILES}
        ${qmldir_arguments}
    )
    set_target_properties("${target}" PROPERTIES OMAWEB_QML_FILES "${arg_QML_FILES}")
endfunction()

function(omaweb_configure_qml target engine_view engine_profile engine_module)
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
        # The QML ships compiled: bytecode for every file, and C++ for the
        # bindings the compiler can type. A module lands under `/qt/qml`, which
        # is already on the engine's import path, so the vendored `qs.Commons`
        # and `qs.Ui` resolve by name and the shell and engine view load by URL.
        omaweb_compiled_qml_module(omaweb_ui_module "Omaweb.Ui" QML_FILES ${omaweb_ui_files})
        omaweb_compiled_qml_module(omaweb_commons_module "qs.Commons"
            QMLDIR "${omaweb_omarchy_root}/qs/Commons/qmldir"
            QML_FILES ${omaweb_omarchy_commons_files}
        )
        omaweb_compiled_qml_module(omaweb_kit_module "qs.Ui"
            QMLDIR "${omaweb_omarchy_root}/qs/Ui/qmldir"
            QML_FILES ${omaweb_omarchy_ui_files}
        )
        omaweb_compiled_qml_module(omaweb_engine_module "${engine_module}"
            QML_FILES "${engine_view}" "${engine_profile}"
        )
        string(REPLACE "." "/" engine_module_path "${engine_module}")
        target_link_libraries(${target} PRIVATE
            ${omaweb_ui_module}
            ${omaweb_commons_module}
            ${omaweb_kit_module}
            ${omaweb_engine_module}
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
            OMAWEB_MAIN_QML_URL="qrc:/qt/qml/Omaweb/Ui/Main.qml"
            OMAWEB_ENGINE_VIEW_URL="qrc:/qt/qml/${engine_module_path}/EngineView.qml"
            OMAWEB_ENGINE_PROFILE_URL="qrc:/qt/qml/${engine_module_path}/EngineProfile.qml"
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
        "src/engine/qt/EngineProfile.qml" "Omaweb.Engine.Qt")
    omaweb_strict_target(omaweb)

    set_target_properties(omaweb PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME "Omaweb"
        MACOSX_BUNDLE_GUI_IDENTIFIER "dev.omaweb.browser"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${OMAWEB_BUILD_NUMBER}"
        WIN32_EXECUTABLE TRUE
    )

    if(UNIX AND NOT APPLE)
        include(GNUInstallDirs)

        # The content blocker is a shared library the build resolves out of the
        # bootstrap cache, so the binary's build-tree runpath names a directory
        # on the machine that compiled it. An installed copy has to find it
        # where the package puts it instead, or it loads nothing anywhere but
        # here.
        # Relative to the binary rather than to a prefix. `CMAKE_INSTALL_FULL_*`
        # bakes in whatever prefix configuring used, and a package sets its own
        # when it installs, which would leave the runpath naming a directory the
        # package never wrote to.
        set_target_properties(omaweb PROPERTIES
            INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}/omaweb")

        install(TARGETS omaweb RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
        install(FILES "${omaweb_content_blocker_library}"
            DESTINATION "${CMAKE_INSTALL_LIBDIR}/omaweb")
        # The entry's own name is the Wayland app id and X11 window class, which
        # a desktop's window rules are written against, so it stays `omaweb`
        # rather than taking the bus name the way a `DBusActivatable` entry
        # would have to. See src/platform/LinuxRunningBrowser.cpp.
        install(FILES packaging/omaweb.desktop
            DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
        install(FILES assets/icons/omaweb.svg
            DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps")
        install(FILES LICENSE THIRD_PARTY_NOTICES.md
            DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/omaweb")
    endif()
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
        "src/ui-lab/MockEngineProfile.qml" "Omaweb.Engine.Mock")
    omaweb_strict_target(omaweb-ui-lab)
    set_target_properties(omaweb-ui-lab PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME "Omaweb UI Lab"
        MACOSX_BUNDLE_GUI_IDENTIFIER "dev.omaweb.browser.ui-lab"
    )
endif()

if(BUILD_TESTING)
    qt_add_executable(omaweb-engine-contract-tests
        "tests/engine/tst_enginecontract.cpp"
    )
    target_link_libraries(omaweb-engine-contract-tests PRIVATE
        omaweb-engine-api
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::Test
    )
    target_compile_definitions(omaweb-engine-contract-tests PRIVATE
        OMAWEB_MOCK_ENGINE_VIEW_PATH="${CMAKE_CURRENT_SOURCE_DIR}/src/ui-lab/MockEngineView.qml"
    )
    omaweb_strict_target(omaweb-engine-contract-tests)
    add_test(NAME omaweb-engine-contract COMMAND omaweb-engine-contract-tests)
    set_tests_properties(omaweb-engine-contract PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    add_test(NAME omaweb-shared-qml-boundary
        COMMAND ${CMAKE_COMMAND}
            -DOMAWEB_UI_DIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}/src/ui
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/check_shared_qml.cmake
    )

    # The release build ships compiled QML and the source-tree builds ship
    # none, so both configurations check their own build graph.
    if(OMAWEB_SOURCE_QML)
        set(omaweb_expected_qml_build "source")
    else()
        set(omaweb_expected_qml_build "compiled")
    endif()
    add_test(NAME omaweb-qml-build-graph
        COMMAND ${CMAKE_COMMAND}
            -DOMAWEB_BUILD_GRAPH=${CMAKE_BINARY_DIR}/build.ninja
            -DOMAWEB_EXPECTED=${omaweb_expected_qml_build}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/check_qml_aot.cmake
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
        add_test(NAME omaweb-security-baseline
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/scripts/tst_security_baseline.py
        )
        # Which construct each terminal colour is read as -- the same claim the
        # Omarchy template makes for a desktop that renders the palette itself.
        add_test(NAME omaweb-terminal-theme-import
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/scripts/tst_terminal_theme_import.py
        )
    endif()

    qt_add_executable(omaweb-core-tests
        "tests/core/SessionFixture.cpp"
        "tests/core/SessionFixture.h"
        "tests/core/tst_browsercontroller.cpp"
    )
    target_link_libraries(omaweb-core-tests PRIVATE omaweb-core Qt6::Test)
    omaweb_strict_target(omaweb-core-tests)
    omaweb_declarative_fixture_target(omaweb-core-tests)
    add_test(NAME omaweb-core COMMAND omaweb-core-tests)

    qt_add_executable(omaweb-session-store-tests tests/core/tst_sessionstore.cpp)
    target_link_libraries(omaweb-session-store-tests PRIVATE omaweb-core Qt6::Test)
    omaweb_strict_target(omaweb-session-store-tests)
    add_test(NAME omaweb-session-store COMMAND omaweb-session-store-tests)

    qt_add_executable(omaweb-session-fixture-tests
        "tests/core/SessionFixture.cpp"
        "tests/core/SessionFixture.h"
        "tests/core/tst_sessionfixture.cpp"
    )
    target_link_libraries(omaweb-session-fixture-tests PRIVATE omaweb-core Qt6::Test)
    omaweb_strict_target(omaweb-session-fixture-tests)
    omaweb_declarative_fixture_target(omaweb-session-fixture-tests)
    add_test(NAME omaweb-session-fixture COMMAND omaweb-session-fixture-tests)

    qt_add_executable(omaweb-space-storage-tests tests/core/tst_spacestorage.cpp)
    target_link_libraries(omaweb-space-storage-tests PRIVATE omaweb-core Qt6::Test)
    omaweb_strict_target(omaweb-space-storage-tests)
    add_test(NAME omaweb-space-storage COMMAND omaweb-space-storage-tests)

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

    qt_add_executable(omaweb-hardware-video-decode-tests
        tests/core/tst_hardwarevideodecode.cpp
    )
    target_link_libraries(omaweb-hardware-video-decode-tests PRIVATE omaweb-core Qt6::Test)
    # The feature name Omaweb asks Chromium for is a claim about one engine, so
    # the test reads the approved baseline rather than a copy of its version.
    target_compile_definitions(omaweb-hardware-video-decode-tests PRIVATE
        OMAWEB_SECURITY_BASELINE="${CMAKE_CURRENT_SOURCE_DIR}/security/baseline.json"
    )
    omaweb_strict_target(omaweb-hardware-video-decode-tests)
    add_test(NAME omaweb-hardware-video-decode COMMAND omaweb-hardware-video-decode-tests)

    qt_add_executable(omaweb-launch-request-tests
        tests/core/tst_launchrequest.cpp
    )
    target_link_libraries(omaweb-launch-request-tests PRIVATE omaweb-core Qt6::Test)
    omaweb_strict_target(omaweb-launch-request-tests)
    add_test(NAME omaweb-launch-request COMMAND omaweb-launch-request-tests)

    qt_add_executable(omaweb-downloads-tests
        tests/core/tst_downloads.cpp
    )
    target_link_libraries(omaweb-downloads-tests PRIVATE omaweb-core Qt6::Test)
    omaweb_strict_target(omaweb-downloads-tests)
    add_test(NAME omaweb-downloads COMMAND omaweb-downloads-tests)

    qt_add_executable(omaweb-download-policy-tests
        tests/core/tst_downloadpolicy.cpp
    )
    target_link_libraries(omaweb-download-policy-tests PRIVATE omaweb-core Qt6::Test)
    omaweb_strict_target(omaweb-download-policy-tests)
    add_test(NAME omaweb-download-policy COMMAND omaweb-download-policy-tests)

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
    # The seam's contract is what it does on a desktop that offers no
    # notification service, so the test is given one. Which service is missing
    # differs by platform: macOS asks a window server, Linux asks the session
    # bus, so both are taken away rather than only the one this host uses.
    set_tests_properties(omaweb-platform-services PROPERTIES
        ENVIRONMENT
            "QT_QPA_PLATFORM=offscreen;DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent/omaweb"
    )

    if(UNIX AND NOT APPLE)
        # The Linux notification seam talks to a session-bus service, so the
        # test supplies one of its own and drives the answers a reader would
        # give. `dbus-run-session` puts it on a private bus, which keeps the
        # test off the developer's desktop and out of their notification
        # history. Qt itself depends on D-Bus, so the binary is present
        # wherever these tests build.
        qt_add_executable(omaweb-notification-service-tests
            tests/platform/tst_notificationservice.cpp
        )
        target_link_libraries(omaweb-notification-service-tests PRIVATE
            omaweb-platform
            Qt6::DBus
            Qt6::Test
        )
        omaweb_strict_target(omaweb-notification-service-tests)
        add_test(NAME omaweb-notification-service
            COMMAND dbus-run-session -- $<TARGET_FILE:omaweb-notification-service-tests>
        )
        set_tests_properties(omaweb-notification-service PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        )

        # Printing goes out through the desktop portal, so the test stands one
        # up on the same kind of private bus. It is what covers the document
        # surviving the spooled copy being taken away, which is the part of the
        # exchange a live portal would only reveal by printing something.
        qt_add_executable(omaweb-print-portal-tests tests/platform/tst_printportal.cpp)
        target_link_libraries(omaweb-print-portal-tests PRIVATE
            omaweb-platform
            Qt6::DBus
            Qt6::Test
        )
        omaweb_strict_target(omaweb-print-portal-tests)
        add_test(NAME omaweb-print-portal
            COMMAND dbus-run-session -- $<TARGET_FILE:omaweb-print-portal-tests>
        )
        # Qt reaching for the desktop portal itself would start the real one on
        # the private bus, racing the stub for the name it needs to own.
        set_tests_properties(omaweb-print-portal PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_NO_XDG_DESKTOP_PORTAL=1"
        )
    endif()

    # What the desktop asked Qt for by way of an input method against what it
    # installed to answer with. Both halves are the host's, so the seam takes
    # them as arguments and the test states the pairs rather than the machine
    # it happens to run on.
    qt_add_executable(omaweb-input-method-tests tests/platform/tst_inputmethod.cpp)
    target_link_libraries(omaweb-input-method-tests PRIVATE omaweb-platform Qt6::Test)
    omaweb_strict_target(omaweb-input-method-tests)
    add_test(NAME omaweb-input-method COMMAND omaweb-input-method-tests)
    # The GTK platform theme a desktop may have selected opens a display of its
    # own and cannot, which is noise on a suite that states its own pairs.
    set_tests_properties(omaweb-input-method PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QPA_PLATFORMTHEME="
    )

    # Following the desktop's theme writes into the reader's Omarchy
    # configuration, so the seam takes its directories as arguments and the
    # test hands it a temporary home. That runs on macOS too, which is where
    # the shipped template is reviewed.
    qt_add_executable(omaweb-runtime-security-tests tests/platform/tst_runtimesecurity.cpp)
    target_link_libraries(omaweb-runtime-security-tests PRIVATE omaweb-platform Qt6::Test)
    omaweb_strict_target(omaweb-runtime-security-tests)
    add_test(NAME omaweb-runtime-security COMMAND omaweb-runtime-security-tests)
    set_tests_properties(omaweb-runtime-security PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QPA_PLATFORMTHEME="
    )

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
    # The palette an Omarchy desktop hands Omaweb is the shipped template
    # rendered, so the suite reads the template rather than a copy of what it
    # is believed to say.
    target_compile_definitions(omaweb-theme-tests PRIVATE
        OMAWEB_OMARCHY_TEMPLATE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/integrations/omarchy/omaweb.json.tpl"
    )
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
        "src/ui-lab/MockEngineProfile.qml" "Omaweb.Engine.Mock")
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

    # Startup and session restore are measured on the lab as a process, from
    # `main` to the frame that drew the window, so the probes launch it rather
    # than loading Main.qml into themselves.
    qt_add_executable(omaweb-startup-probes "tests/ui/tst_startup.cpp")
    target_link_libraries(omaweb-startup-probes PRIVATE
        omaweb-core
        Qt6::Test
    )
    target_include_directories(omaweb-startup-probes PRIVATE "tests")
    target_compile_definitions(omaweb-startup-probes PRIVATE
        OMAWEB_UI_LAB_EXECUTABLE="$<TARGET_FILE:omaweb-qml-smoke>"
    )
    add_dependencies(omaweb-startup-probes omaweb-qml-smoke)
    omaweb_strict_target(omaweb-startup-probes)
    add_test(NAME omaweb-startup-probes COMMAND omaweb-startup-probes)
    set_tests_properties(omaweb-startup-probes PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    if(OMAWEB_BUILD_UI_LAB)
        qt_add_executable(omaweb-ui-tests
            "tests/ui/tst_ui.cpp"
            "tests/ui/ProbeClock.cpp"
            "tests/ui/ProbeClock.h"
        )
        target_link_libraries(omaweb-ui-tests PRIVATE
            omaweb-core
            omaweb-content-blocking
            omaweb-engine-api
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
        target_include_directories(omaweb-ui-tests PRIVATE "tests")
        omaweb_strict_target(omaweb-ui-tests)
        add_test(NAME omaweb-ui COMMAND omaweb-ui-tests)
        # The chrome is checked against a desktop that offers nothing, so the
        # commands standing on a platform service report themselves unavailable
        # and the shell's own wording for that is what gets tested. On Linux
        # those services are on the session bus, so it is taken away alongside
        # the window server rather than left to whatever the host happens to
        # be running.
        set_tests_properties(omaweb-ui PROPERTIES
            ENVIRONMENT
                "QT_QPA_PLATFORM=offscreen;DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent/omaweb"
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
            OMAWEB_UI_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}/src/ui"
            OMAWEB_UNTRUSTED_CERTIFICATE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/tests/engine/certificates/untrusted-local.crt"
            OMAWEB_UNTRUSTED_KEY_PATH="${CMAKE_CURRENT_SOURCE_DIR}/tests/engine/certificates/untrusted-local.key"
        )
        target_include_directories(omaweb-qt-engine-contract-tests PRIVATE "tests")
        omaweb_strict_target(omaweb-qt-engine-contract-tests)
        add_test(NAME omaweb-qt-engine-contract COMMAND omaweb-qt-engine-contract-tests)
        set_tests_properties(omaweb-qt-engine-contract PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        )
    endif()
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmarks/CosmeticBenchmark.cmake")

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
