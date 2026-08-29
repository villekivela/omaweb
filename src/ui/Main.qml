import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    objectName: privateWindow ? "privateBrowserWindow" : "mainBrowserWindow"

    width: 1360
    height: 860
    minimumWidth: 840
    minimumHeight: 560
    visible: true
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint
    title: window.privateWindow ? "Private — Tanto" : window.windowBrowser.activeTitle + " — Tanto"

    property var windowBrowser: browser
    property bool privateWindow: false
    property string profilePathOverride: ""
    property var sharedEngineProfile: null
    property var colors: privateWindow ? privatePalette(theme.palette) : theme.palette
    property bool sidebarCollapsed: false
    property bool omnibarOpen: false
    property bool newTabIntent: false
    property string pendingMoveTabId: ""
    property string pendingMoveSpaceId: ""
    property var privateProfileHost: null
    property var spaceProfileHost: null
    property var omnibarSuggestions: []
    property var visibleDownloads: []
    property var visibleSubscriptions: []
    property int visibleBlockedRequestCount: 0
    property var pendingPermissionRequest: null
    property string pendingPermissionOrigin: ""
    property string pendingPermissionType: ""
    property var pendingPermissionResponder: null
    property var downloadRecordIds: ({})

    function privatePalette(source) {
        const palette = Object.assign({}, source)
        palette.window = source.privateWindow
        palette.sidebar = source.privateSidebar
        palette.surface = source.privateSurface
        palette.surfaceHover = source.privateSurfaceHover
        palette.accent = source.privateAccent
        return palette
    }

    FontLoader {
        id: materialSymbols
        objectName: "materialSymbolsFont"
        source: iconFontSource
    }

    function openOmnibar(forNewTab) {
        newTabIntent = forNewTab
        omnibarInput.text = forNewTab ? "" : window.windowBrowser.activeUrl.toString()
        omnibarOpen = true
        omnibarSuggestions = window.privateWindow
            ? [] : window.windowBrowser.historySuggestions(omnibarInput.text)
        Qt.callLater(function() {
            omnibarInput.forceActiveFocus()
            omnibarInput.selectAll()
        })
    }

    function createSpaceProfile() {
        if (window.privateWindow || window.spaceProfileHost) return
        const component = Qt.createComponent(engineProfileSource)
        window.spaceProfileHost = component.createObject(window, {
            "profilePath": window.windowBrowser.activeProfilePath,
            "downloadDirectory": window.windowBrowser.downloadDirectory,
            "acceptDownloads": window.windowBrowser.acceptDownloads,
            "privateBrowsing": false,
            "downloadNamespace": window.windowBrowser.activeSpaceId,
            "engineContentBlocker": engineContentBlocker
        })
        window.spaceProfileHost.downloadStarted.connect(window.handleDownloadStarted)
        window.spaceProfileHost.downloadUpdated.connect(window.handleDownloadUpdated)
    }

    function handleDownloadStarted(runtimeId, sourceUrl, path, state, receivedBytes, totalBytes) {
        const recordId = window.windowBrowser.recordDownload(runtimeId, sourceUrl, path,
            state, receivedBytes, totalBytes)
        if (recordId.length > 0) window.downloadRecordIds[runtimeId] = recordId
    }

    function handleDownloadUpdated(runtimeId, state, receivedBytes, totalBytes, error) {
        const recordId = window.downloadRecordIds[runtimeId]
        if (recordId) window.windowBrowser.updateDownload(recordId, state,
            receivedBytes, totalBytes, error)
    }

    function closeOmnibar() {
        omnibarOpen = false
        newTabIntent = false
        if (engineLoader.item) engineLoader.item.focusPage()
    }

    Shortcut {
        sequence: Qt.platform.os === "osx" ? "Meta+L" : "Ctrl+L"
        onActivated: window.openOmnibar(false)
    }

    Shortcut {
        sequence: Qt.platform.os === "osx" ? "Meta+T" : "Ctrl+T"
        onActivated: window.openOmnibar(true)
    }

    Shortcut {
        sequence: Qt.platform.os === "osx" ? "Meta+W" : "Ctrl+W"
        onActivated: window.windowBrowser.closeActiveTab()
    }

    Shortcut {
        sequence: Qt.platform.os === "osx" ? "Meta+Shift+T" : "Ctrl+Shift+T"
        onActivated: window.windowBrowser.reopenClosedTab()
    }

    Shortcut {
        sequence: Qt.platform.os === "osx" ? "Meta+Shift+N" : "Ctrl+Shift+N"
        onActivated: windowManager.openPrivateWindow()
    }

    Shortcut {
        sequence: Qt.platform.os === "osx" ? "Meta+M" : "Ctrl+M"
        onActivated: window.showMinimized()
    }

    Rectangle {
        id: shell
        anchors.fill: parent
        radius: 14
        color: window.colors.window
        border.width: 1
        border.color: window.colors.border
        clip: true

        RowLayout {
            anchors.fill: parent
            spacing: 0

            Rectangle {
                id: sidebar
                Layout.fillHeight: true
                Layout.preferredWidth: window.sidebarCollapsed ? 58 : 238
                color: window.colors.sidebar

                Behavior on Layout.preferredWidth {
                    NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 8

                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 28

                        Text {
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            visible: !window.sidebarCollapsed
                            text: "Tanto"
                            color: window.colors.text
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                        }

                        DragHandler {
                            target: null
                            onActiveChanged: if (active) window.startSystemMove()
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        ChromeButton {
                            objectName: "backButton"
                            Layout.fillWidth: true
                            label: "arrow_back"
                            accessibleName: "Back"
                            fontFamily: materialSymbols.name
                            foreground: window.colors.text
                            hoverBackground: window.colors.surfaceHover
                            enabled: engineLoader.item ? engineLoader.item.canGoBack : false
                            onClicked: window.windowBrowser.requestBack()
                        }

                        ChromeButton {
                            objectName: "forwardButton"
                            Layout.fillWidth: true
                            label: "arrow_forward"
                            accessibleName: "Forward"
                            fontFamily: materialSymbols.name
                            foreground: window.colors.text
                            hoverBackground: window.colors.surfaceHover
                            enabled: engineLoader.item ? engineLoader.item.canGoForward : false
                            onClicked: window.windowBrowser.requestForward()
                        }

                        ChromeButton {
                            objectName: "reloadButton"
                            Layout.fillWidth: true
                            label: "refresh"
                            accessibleName: "Reload"
                            fontFamily: materialSymbols.name
                            foreground: window.colors.text
                            hoverBackground: window.colors.surfaceHover
                            onClicked: window.windowBrowser.requestReload()
                        }

                        ChromeButton {
                            objectName: "pinButton"
                            Layout.fillWidth: true
                            label: window.windowBrowser.activeTabPinned ? "bookmark" : "bookmark_border"
                            accessibleName: window.windowBrowser.activeTabPinned ? "Unpin tab" : "Pin tab"
                            fontFamily: materialSymbols.name
                            foreground: window.windowBrowser.activeTabPinned ? window.colors.accent : window.colors.text
                            hoverBackground: window.colors.surfaceHover
                            onClicked: window.windowBrowser.toggleActivePinned()
                            visible: !window.privateWindow
                        }

                        ChromeButton {
                            objectName: "moveTabButton"
                            Layout.fillWidth: true
                            label: "drive_file_move"
                            accessibleName: "Move tab to Space"
                            fontFamily: materialSymbols.name
                            foreground: window.colors.text
                            hoverBackground: window.colors.surfaceHover
                            onClicked: moveTabDialog.open()
                            visible: !window.privateWindow
                        }
                    }

                    Rectangle {
                        id: addressButton
                        objectName: "addressButton"
                        property string accessibleName: "Search or enter address"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 38
                        radius: 10
                        color: sidebarAddressMouse.containsMouse
                            ? window.colors.surfaceHover
                            : window.colors.surface
                        activeFocusOnTab: true
                        Accessible.role: Accessible.Button
                        Accessible.name: addressButton.accessibleName
                        Accessible.onPressAction: window.openOmnibar(false)

                        Keys.onPressed: function(event) {
                            if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
                                window.openOmnibar(false)
                                event.accepted = true
                            }
                        }

                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 12
                            anchors.right: parent.right
                            anchors.rightMargin: 12
                            anchors.verticalCenter: parent.verticalCenter
                            visible: !window.sidebarCollapsed
                            text: window.windowBrowser.activeUrl.toString() === "about:blank"
                                ? "Search or enter address"
                                : window.windowBrowser.activeUrl.toString()
                            color: window.windowBrowser.activeUrl.toString() === "about:blank"
                                ? window.colors.mutedText
                                : window.colors.text
                            elide: Text.ElideMiddle
                            font.pixelSize: 13
                        }

                        Text {
                            anchors.centerIn: parent
                            visible: window.sidebarCollapsed
                            text: "search"
                            color: window.colors.text
                            font.family: materialSymbols.name
                            font.pixelSize: 20
                        }

                        MouseArea {
                            id: sidebarAddressMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.IBeamCursor
                            onClicked: {
                                addressButton.forceActiveFocus()
                                window.openOmnibar(false)
                            }
                        }
                    }

                    Grid {
                        objectName: "pinnedGrid"
                        Layout.fillWidth: true
                        columns: window.sidebarCollapsed ? 1 : 4
                        visible: !window.privateWindow
                        columnSpacing: 6
                        rowSpacing: 6

                        Repeater {
                            model: window.windowBrowser.tabs

                            PinnedTabDelegate {
                                width: window.sidebarCollapsed
                                    ? 38
                                    : (sidebar.width - 20 - 18) / 4
                                visible: pinned
                                height: visible ? (window.sidebarCollapsed ? 38 : 54) : 0
                                colors: window.colors
                                iconFontFamily: materialSymbols.name
                                onActivated: function(id) { window.windowBrowser.activateTab(id) }
                            }
                        }
                    }

                    RowLayout {
                        objectName: "spaceHeading"
                        Layout.fillWidth: true
                        Layout.preferredHeight: window.sidebarCollapsed ? 16 : 34
                        spacing: 4

                        ComboBox {
                            id: spaceSelector
                            objectName: "spaceSelector"
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            visible: !window.sidebarCollapsed && !window.privateWindow
                            model: window.windowBrowser.spaces
                            textRole: "spaceName"
                            valueRole: "spaceId"
                            displayText: window.windowBrowser.activeSpaceName
                            Accessible.name: "Active Space"
                            onActivated: window.windowBrowser.switchSpace(currentValue)
                        }

                        ChromeButton {
                            id: manageSpacesButton
                            objectName: "manageSpacesButton"
                            Layout.preferredWidth: 32
                            Layout.fillHeight: true
                            visible: !window.sidebarCollapsed && !window.privateWindow
                            label: "more_horiz"
                            accessibleName: "Manage Spaces"
                            fontFamily: materialSymbols.name
                            foreground: window.colors.mutedText
                            hoverBackground: window.colors.surfaceHover
                            onClicked: spacesMenu.popup()

                            Menu {
                                id: spacesMenu

                                MenuItem {
                                    text: "New Space"
                                    onTriggered: newSpaceDialog.open()
                                }
                                MenuItem {
                                    text: "Rename " + window.windowBrowser.activeSpaceName
                                    onTriggered: renameSpaceDialog.open()
                                }
                                MenuItem {
                                    text: "Delete " + window.windowBrowser.activeSpaceName
                                    onTriggered: deleteSpaceDialog.open()
                                }
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: !window.sidebarCollapsed && window.privateWindow
                            text: "Private"
                            color: window.colors.privateAccent
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            Accessible.role: Accessible.Heading
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: window.colors.border
                    }

                    ScrollView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true

                        Column {
                            width: parent.width
                            spacing: 3

                            Repeater {
                                model: window.windowBrowser.tabs

                                TabDelegate {
                                    width: parent.width
                                    visible: !pinned
                                    height: visible ? 40 : 0
                                    colors: window.colors
                                    onActivated: function(id) { window.windowBrowser.activateTab(id) }
                                }
                            }
                        }
                    }

                    Rectangle {
                        id: newTabButton
                        objectName: "newTabButton"
                        property string accessibleName: "New tab"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 34
                        radius: 9
                        color: newTabMouse.containsMouse ? window.colors.surfaceHover : window.colors.surface
                        activeFocusOnTab: true
                        Accessible.role: Accessible.Button
                        Accessible.name: newTabButton.accessibleName
                        Accessible.onPressAction: window.openOmnibar(true)

                        Keys.onPressed: function(event) {
                            if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
                                window.openOmnibar(true)
                                event.accepted = true
                            }
                        }

                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: window.sidebarCollapsed ? 0 : 10
                            anchors.verticalCenter: parent.verticalCenter
                            width: window.sidebarCollapsed ? parent.width : 20
                            horizontalAlignment: Text.AlignHCenter
                            text: "add"
                            color: window.colors.text
                            font.family: materialSymbols.name
                            font.pixelSize: 19
                        }

                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 34
                            anchors.verticalCenter: parent.verticalCenter
                            visible: !window.sidebarCollapsed
                            text: "New tab"
                            color: window.colors.text
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                        }

                        MouseArea {
                            id: newTabMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                newTabButton.forceActiveFocus()
                                window.openOmnibar(true)
                            }
                        }
                    }


                    ChromeButton {
                        objectName: "settingsButton"
                        Layout.alignment: Qt.AlignHCenter
                        label: "settings"
                        accessibleName: "Browsing settings and downloads"
                        fontFamily: materialSymbols.name
                        foreground: window.colors.mutedText
                        hoverBackground: window.colors.surfaceHover
                        onClicked: settingsDialog.open()
                    }

                    ChromeButton {
                        objectName: "collapseButton"
                        Layout.alignment: Qt.AlignHCenter
                        label: window.sidebarCollapsed ? "chevron_right" : "chevron_left"
                        accessibleName: window.sidebarCollapsed ? "Expand sidebar" : "Collapse sidebar"
                        fontFamily: materialSymbols.name
                        foreground: window.colors.mutedText
                        hoverBackground: window.colors.surfaceHover
                        onClicked: window.sidebarCollapsed = !window.sidebarCollapsed
                    }
                }
            }

            Item {
                objectName: "engineViewport"
                Layout.fillWidth: true
                Layout.fillHeight: true

                Loader {
                    id: engineLoader
                    objectName: "engineLoader"
                    anchors.fill: parent
                    focus: true

                    function loadActiveSpace() {
                        window.createSpaceProfile()
                        setSource(engineViewSource, {
                            "profilePath": window.profilePathOverride.length > 0
                                ? window.profilePathOverride
                                : window.windowBrowser.activeProfilePath,
                            "currentUrl": window.windowBrowser.activeUrl,
                            "sharedProfile": window.privateWindow
                                ? window.sharedEngineProfile
                                : (window.spaceProfileHost ? window.spaceProfileHost.profile : null),
                            "permissionController": window.windowBrowser,
                            "contentBlocker": contentBlocker,
                            "engineContentBlocker": engineContentBlocker
                        })
                    }

                    Component.onCompleted: loadActiveSpace()

                    onLoaded: {
                        item.focusPage()
                    }
                }

                Binding {
                    target: engineLoader.item
                    property: "profilePath"
                    value: window.profilePathOverride.length > 0
                        ? window.profilePathOverride
                        : window.windowBrowser.activeProfilePath
                    when: engineLoader.item !== null
                }

                Connections {
                    target: engineLoader.item
                    ignoreUnknownSignals: true

                    function onCurrentUrlChanged() {
                        window.windowBrowser.updateActiveTab(engineLoader.item.currentUrl, engineLoader.item.pageTitle)
                    }

                    function onPageTitleChanged() {
                        window.windowBrowser.updateActiveTab(engineLoader.item.currentUrl, engineLoader.item.pageTitle)
                    }

                    function onLoadingChanged() {
                        window.windowBrowser.setActiveLoading(engineLoader.item.loading)
                        if (!engineLoader.item.loading) {
                            window.windowBrowser.recordVisit(
                                engineLoader.item.currentUrl, engineLoader.item.pageTitle)
                        }
                    }

                    function onRendererFailed(reason) {
                        window.windowBrowser.reportRendererFailure(reason)
                    }

                    function onAuxiliaryWindowRequested(request, requestedUrl) {
                        auxiliaryWindowComponent.createObject(window, {
                            "openerEngine": engineLoader.item,
                            "request": request,
                            "requestedUrl": requestedUrl
                        })
                    }

                    function onNewTabRequested(request, requestedUrl) {
                        const destination = requestedUrl.toString().length > 0
                            ? requestedUrl.toString()
                            : "about:blank"
                        window.windowBrowser.openInput(destination, true)
                        if (request) engineLoader.item.acceptNewWindowRequest(request)
                    }

                    function onSitePermissionRequested(requestId, origin, permission) {
                        window.pendingPermissionRequest = requestId
                        window.pendingPermissionResponder = engineLoader.item
                        window.pendingPermissionOrigin = origin
                        window.pendingPermissionType = permission
                        permissionDialog.open()
                    }
                }

                Connections {
                    target: window.windowBrowser

                    function onTabMoveConfirmationRequested(tabId, destinationSpaceId) {
                        window.pendingMoveTabId = tabId
                        window.pendingMoveSpaceId = destinationSpaceId
                        confirmTabMoveDialog.open()
                    }

                    function onSpaceSuspended(spaceId) {
                        engineLoader.source = ""
                        if (window.spaceProfileHost) {
                            window.spaceProfileHost.retire()
                            window.spaceProfileHost = null
                        }
                    }

                    function onSpaceRestored(spaceId) {
                        if (spaceId === window.windowBrowser.activeSpaceId)
                            Qt.callLater(function() { engineLoader.loadActiveSpace() })
                    }

                    function onActiveTabChanged() {
                        if (engineLoader.item && engineLoader.item.currentUrl !== window.windowBrowser.activeUrl) {
                            engineLoader.item.currentUrl = window.windowBrowser.activeUrl
                        }
                    }

                    function onBackRequested() {
                        if (engineLoader.item) engineLoader.item.goBack()
                    }

                    function onForwardRequested() {
                        if (engineLoader.item) engineLoader.item.goForward()
                    }

                    function onReloadRequested() {
                        if (engineLoader.item) engineLoader.item.reloadPage()
                    }

                    function onCloseWindowRequested() {
                        if (window.privateWindow) window.close()
                    }
                }

                Rectangle {
                    anchors.fill: parent
                    visible: window.windowBrowser.activeRendererFailed
                    color: window.colors.window
                    z: 10

                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 12

                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: "This page stopped working"
                            color: window.colors.text
                            font.pixelSize: 20
                            font.weight: Font.DemiBold
                            Accessible.role: Accessible.Heading
                        }

                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: window.windowBrowser.activeRendererFailureReason
                            color: window.colors.mutedText
                            font.pixelSize: 13
                        }

                        ChromeButton {
                            objectName: "recoverButton"
                            Layout.alignment: Qt.AlignHCenter
                            implicitWidth: 96
                            label: "Reload"
                            accessibleName: "Reload crashed page"
                            foreground: window.colors.text
                            background: window.colors.surface
                            hoverBackground: window.colors.surfaceHover
                            onClicked: window.windowBrowser.recoverActiveTab()
                        }
                    }
                }


                Rectangle {
                    objectName: "browserErrorBanner"
                    anchors.top: parent.top
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: Math.min(620, parent.width - 40)
                    height: window.windowBrowser.errorMessage.length > 0 ? 52 : 0
                    visible: height > 0
                    radius: 10
                    color: window.colors.surface
                    border.width: 1
                    border.color: window.colors.border
                    z: 20

                    Text {
                        anchors.fill: parent
                        anchors.margins: 12
                        text: window.windowBrowser.errorMessage
                        color: window.colors.text
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }
    }

    Component {
        id: auxiliaryWindowComponent

        AuxiliaryWindow {
            engineSource: engineViewSource
            permissionController: window.windowBrowser
            contentBlocker: contentBlocker
            engineContentBlocker: engineContentBlocker
            onSitePermissionRequested: function(responder, requestId, origin, permission) {
                window.pendingPermissionRequest = requestId
                window.pendingPermissionResponder = responder
                window.pendingPermissionOrigin = origin
                window.pendingPermissionType = permission
                permissionDialog.open()
            }
        }
    }

    Connections {
        target: windowManager

        function onPrivateWindowRequested(controller, profilePath) {
            if (window.privateWindow) return
            if (!window.privateProfileHost) {
                const profileComponent = Qt.createComponent(engineProfileSource)
                window.privateProfileHost = profileComponent.createObject(window, {
                    "profilePath": profilePath,
                    "downloadDirectory": windowManager.privateDownloadDirectory,
                    "acceptDownloads": windowManager.acceptPrivateDownloads,
                    "privateBrowsing": true,
                    "engineContentBlocker": engineContentBlocker
                })
            }
            const component = Qt.createComponent(Qt.resolvedUrl("Main.qml"))
            component.createObject(window, {
                "windowBrowser": controller,
                "privateWindow": true,
                "profilePathOverride": profilePath,
                "sharedEngineProfile": window.privateProfileHost.profile
            })
        }

        function onPrivateSessionEnding() {
            if (window.privateWindow || windowManager.privateWindowCount > 0
                    || !window.privateProfileHost) return
            window.privateProfileHost.retire()
            window.privateProfileHost = null
        }
    }

    Connections {
        target: contentBlocker

        function refreshContentBlockingState() {
            window.visibleSubscriptions = contentBlocker.subscriptions
            window.visibleBlockedRequestCount = contentBlocker.blockedRequestCount(
                window.windowBrowser.activeUrl)
        }

        function onSubscriptionsChanged() { refreshContentBlockingState() }
        function onBlockedRequestCountChanged(siteUrl) { refreshContentBlockingState() }
        function onRulesChanged() { refreshContentBlockingState() }
    }

    onClosing: function(close) {
        if (!window.privateWindow || !window.windowBrowser) return
        const controller = window.windowBrowser
        Qt.callLater(function() {
            window.destroy()
            windowManager.releasePrivateWindow(controller)
        })
    }

    Dialog {
        id: newSpaceDialog
        objectName: "newSpaceDialog"
        anchors.centerIn: parent
        modal: true
        title: "New Space"
        standardButtons: Dialog.Ok | Dialog.Cancel
        onOpened: {
            newSpaceName.text = ""
            newSpaceName.forceActiveFocus()
        }
        onAccepted: {
            const spaceId = window.windowBrowser.createSpace(newSpaceName.text)
            if (spaceId.length > 0) window.windowBrowser.switchSpace(spaceId)
        }

        TextField {
            id: newSpaceName
            objectName: "newSpaceName"
            width: 280
            placeholderText: "Space name"
            Accessible.name: "New Space name"
        }
    }


    Dialog {
        id: permissionDialog
        objectName: "permissionDialog"
        anchors.centerIn: parent
        modal: true
        title: "Site permission"
        standardButtons: Dialog.NoButton

        ColumnLayout {
            width: 380
            spacing: 10

            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: window.pendingPermissionOrigin + " requested a protected browser capability."
            }

            RowLayout {
                Layout.fillWidth: true

                Button {
                    text: "Allow once"
                    onClicked: {
                        window.windowBrowser.setPermissionDecision(window.pendingPermissionOrigin,
                            window.pendingPermissionType, 1)
                        if (window.pendingPermissionResponder) window.pendingPermissionResponder.respondToPermission(
                            window.pendingPermissionRequest, 1)
                        permissionDialog.close()
                    }
                }
                Button {
                    text: "Always allow"
                    enabled: !window.privateWindow
                    onClicked: {
                        window.windowBrowser.setPermissionDecision(window.pendingPermissionOrigin,
                            window.pendingPermissionType, 2)
                        if (window.pendingPermissionResponder) window.pendingPermissionResponder.respondToPermission(
                            window.pendingPermissionRequest, 2)
                        permissionDialog.close()
                    }
                }
                Button {
                    text: "Block"
                    onClicked: {
                        window.windowBrowser.setPermissionDecision(window.pendingPermissionOrigin,
                            window.pendingPermissionType, 3)
                        if (window.pendingPermissionResponder) window.pendingPermissionResponder.respondToPermission(
                            window.pendingPermissionRequest, 3)
                        permissionDialog.close()
                    }
                }
            }
        }
    }

    Dialog {
        id: settingsDialog
        objectName: "settingsDialog"
        anchors.centerIn: parent
        modal: true
        title: "Content blocking and downloads"
        standardButtons: Dialog.Close
        onOpened: {
            window.visibleDownloads = window.windowBrowser.downloadHistory()
            window.visibleSubscriptions = contentBlocker.subscriptions
            window.visibleBlockedRequestCount = contentBlocker.blockedRequestCount(
                window.windowBrowser.activeUrl)
            userRulesInput.text = contentBlocker.userRules
        }

        ScrollView {
            id: settingsScroll
            width: 500
            height: Math.min(620, window.height - 100)
            contentWidth: availableWidth

            ColumnLayout {
                width: settingsScroll.availableWidth
                spacing: 10

            Label {
                objectName: "remoteSuggestionsStatus"
                text: "Remote search suggestions: Off"
            }
            Label {
                objectName: "automaticRequestsStatus"
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: "Enabled filter-list subscriptions make automatic network requests to their displayed update address when Tanto starts. Remote search suggestions remain off."
            }
            Label {
                text: "Content blocking"
                font.weight: Font.DemiBold
            }
            Label {
                objectName: "blockedRequestCount"
                text: window.visibleBlockedRequestCount + " requests blocked for this site"
            }
            CheckBox {
                objectName: "siteBlockingEnabled"
                text: "Enable for " + window.windowBrowser.activeUrl.host
                checked: contentBlocker.siteEnabled(window.windowBrowser.activeUrl)
                onClicked: contentBlocker.setSiteEnabled(
                    window.windowBrowser.activeUrl, checked)
            }
            TextArea {
                id: userRulesInput
                objectName: "userRulesInput"
                Layout.fillWidth: true
                Layout.preferredHeight: 90
                placeholderText: "One user rule per line"
                wrapMode: TextEdit.NoWrap
            }
            Button {
                text: contentBlocker.compiling ? "Compiling rules..." : "Save user rules"
                enabled: !contentBlocker.compiling
                onClicked: contentBlocker.userRules = userRulesInput.text
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: "Supported: network rules and plain CSS cosmetic rules. "
                    + "Scriptlets, procedural selectors, response rewriting, HTML filtering, "
                    + "dynamic rules, CNAME uncloaking, redirects, and resource replacement are unsupported."
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                visible: contentBlocker.compilationReport.unsupported !== undefined
                    && Object.keys(contentBlocker.compilationReport.unsupported).length > 0
                text: "Unsupported rules in active lists: "
                    + JSON.stringify(contentBlocker.compilationReport.unsupported)
            }
            ListView {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(150, contentHeight)
                model: window.visibleSubscriptions
                visible: count > 0

                delegate: Column {
                    required property var modelData
                    width: ListView.view.width
                    spacing: 2

                    RowLayout {
                        width: parent.width
                        CheckBox {
                            checked: modelData.enabled
                            text: modelData.title
                            onClicked: contentBlocker.setSubscriptionEnabled(
                                modelData.id, checked)
                        }
                        Label { text: modelData.updateStatus }
                    }
                    Label {
                        width: parent.width
                        text: "Source: " + modelData.source + " · License: " + modelData.license
                        elide: Text.ElideMiddle
                    }
                    Label {
                        width: parent.width
                        text: "Updates: " + modelData.updateAddress
                        elide: Text.ElideMiddle
                    }
                }
            }
            GridLayout {
                columns: 2
                Layout.fillWidth: true

                TextField { id: subscriptionTitle; placeholderText: "List name" }
                TextField { id: subscriptionLicense; placeholderText: "License" }
                TextField { id: subscriptionSource; placeholderText: "Source page" }
                TextField { id: subscriptionUpdate; placeholderText: "Update address" }
            }
            Button {
                text: "Add subscription"
                onClicked: contentBlocker.addSubscription(subscriptionTitle.text,
                    subscriptionSource.text, subscriptionLicense.text, subscriptionUpdate.text)
            }
            Label {
                text: "Download history"
                font.weight: Font.DemiBold
            }
            ListView {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(180, contentHeight)
                model: window.visibleDownloads
                visible: count > 0

                delegate: Label {
                    required property var modelData
                    width: ListView.view.width
                    text: modelData.path + " · " + modelData.state
                        + (modelData.error.length > 0 ? " · " + modelData.error : "")
                    elide: Text.ElideMiddle
                }
            }
            Label {
                visible: window.visibleDownloads.length === 0
                text: "No recorded downloads"
                color: window.colors.mutedText
            }
            }
        }
    }

    Dialog {
        id: renameSpaceDialog
        objectName: "renameSpaceDialog"
        anchors.centerIn: parent
        modal: true
        title: "Rename Space"
        standardButtons: Dialog.Ok | Dialog.Cancel
        onOpened: {
            renamedSpaceName.text = window.windowBrowser.activeSpaceName
            renamedSpaceName.forceActiveFocus()
            renamedSpaceName.selectAll()
        }
        onAccepted: window.windowBrowser.renameSpace(window.windowBrowser.activeSpaceId, renamedSpaceName.text)

        TextField {
            id: renamedSpaceName
            objectName: "renamedSpaceName"
            width: 280
            Accessible.name: "Renamed Space name"
        }
    }

    Dialog {
        id: deleteSpaceDialog
        objectName: "deleteSpaceDialog"
        anchors.centerIn: parent
        modal: true
        title: "Delete " + window.windowBrowser.activeSpaceName
        standardButtons: Dialog.Ok | Dialog.Cancel
        onOpened: {
            deleteSpaceConfirmation.text = ""
            deleteSpaceConfirmation.forceActiveFocus()
        }
        onAccepted: window.windowBrowser.deleteSpace(
            window.windowBrowser.activeSpaceId, deleteSpaceConfirmation.text)

        ColumnLayout {
            width: 320
            spacing: 10

            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: "Type " + window.windowBrowser.activeSpaceName
                    + " to delete its tabs, session, and engine data."
            }

            TextField {
                id: deleteSpaceConfirmation
                objectName: "deleteSpaceConfirmation"
                Layout.fillWidth: true
                Accessible.name: "Space deletion confirmation"
            }
        }
    }

    Dialog {
        id: moveTabDialog
        objectName: "moveTabDialog"
        anchors.centerIn: parent
        modal: true
        title: "Choose destination Space"
        standardButtons: Dialog.Cancel

        ColumnLayout {
            width: 340
            spacing: 8

            Repeater {
                model: window.windowBrowser.spaces

                Button {
                    Layout.fillWidth: true
                    visible: spaceId !== window.windowBrowser.activeSpaceId
                    text: spaceName
                    Accessible.name: "Move tab to " + spaceName
                    onClicked: {
                        const tabId = window.windowBrowser.activeTabId
                        const destinationSpaceId = spaceId
                        moveTabDialog.close()
                        if (engineLoader.item) {
                            engineLoader.item.checkForEditedFormState(function(hasEditedFormState) {
                                window.windowBrowser.requestTabMoveToSpace(
                                    tabId, destinationSpaceId, hasEditedFormState)
                            })
                        } else {
                            window.windowBrowser.requestTabMoveToSpace(tabId, destinationSpaceId, true)
                        }
                    }
                }
            }
        }
    }

    Dialog {
        id: confirmTabMoveDialog
        objectName: "confirmTabMoveDialog"
        anchors.centerIn: parent
        modal: true
        title: "Discard edited form state?"
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: window.windowBrowser.confirmTabMoveToSpace(
            window.pendingMoveTabId, window.pendingMoveSpaceId)

        Label {
            width: 340
            wrapMode: Text.WordWrap
            text: "This page has edited form state. Moving it will reload the page under the destination identity and discard those edits."
        }
    }

    Rectangle {
        anchors.fill: parent
        visible: window.omnibarOpen
        color: "#66000000"
        z: 50

        MouseArea {
            anchors.fill: parent
            onClicked: window.closeOmnibar()
        }

        Rectangle {
            width: Math.min(720, parent.width - 80)
            height: 70 + Math.min(280, historySuggestionList.contentHeight)
            anchors.centerIn: parent
            radius: 16
            color: window.colors.overlay
            border.width: 1
            border.color: window.colors.border

            TextField {
                id: omnibarInput
                objectName: "omnibarInput"
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: 70
                anchors.margins: 12
                background: null
                color: window.colors.text
                placeholderText: window.newTabIntent ? "Open in new tab" : "Search or enter address"
                placeholderTextColor: window.colors.mutedText
                font.pixelSize: 19
                selectByMouse: true
                Accessible.name: "Omnibar"

                onTextChanged: {
                    window.omnibarSuggestions = window.privateWindow
                        ? [] : window.windowBrowser.historySuggestions(text)
                }

                onAccepted: {
                    if (text.trim().length === 0) return
                    window.windowBrowser.openInput(text, window.newTabIntent)
                    window.closeOmnibar()
                }

                Keys.onEscapePressed: function(event) {
                    window.closeOmnibar()
                    event.accepted = true
                }
            }

            ListView {
                id: historySuggestionList
                objectName: "historySuggestionList"
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: omnibarInput.bottom
                anchors.bottom: parent.bottom
                anchors.margins: 12
                clip: true
                model: window.omnibarSuggestions

                delegate: ItemDelegate {
                    required property var modelData
                    width: ListView.view.width
                    height: 52
                    text: modelData.title + "\n" + modelData.url
                    Accessible.name: "Open history result " + modelData.title
                    onClicked: {
                        window.windowBrowser.openInput(modelData.url.toString(), window.newTabIntent)
                        window.closeOmnibar()
                    }
                }
            }
        }
    }

    Rectangle {
        objectName: "privateIndicator"
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 4
        visible: window.privateWindow
        color: window.colors.privateAccent
        z: 90
    }

    MouseArea {
        width: 5
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        cursorShape: Qt.SizeHorCursor
        onPressed: window.startSystemResize(Qt.LeftEdge)
        z: 100
    }

    MouseArea {
        width: 5
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        cursorShape: Qt.SizeHorCursor
        onPressed: window.startSystemResize(Qt.RightEdge)
        z: 100
    }

    MouseArea {
        height: 5
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        cursorShape: Qt.SizeVerCursor
        onPressed: window.startSystemResize(Qt.TopEdge)
        z: 100
    }

    MouseArea {
        height: 5
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        cursorShape: Qt.SizeVerCursor
        onPressed: window.startSystemResize(Qt.BottomEdge)
        z: 100
    }
}
