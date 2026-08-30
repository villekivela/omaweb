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
    color: "transparent"
    flags: Qt.platform.os === "osx"
        ? Qt.Window | Qt.ExpandedClientAreaHint | Qt.NoTitleBarBackgroundHint
        : Qt.Window | Qt.FramelessWindowHint
    topPadding: 0
    visible: true
    title: window.privateWindow ? "Private — Tanto" : window.windowBrowser.activeTitle + " — Tanto"

    property var windowBrowser: browser
    // The native backdrop reads this to mask its blur to the same rounded rect,
    // so the shell and the platform chrome cannot drift apart.
    property real cornerRadius: 14
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
        palette.windowOpaque = source.privateWindowOpaque
        palette.sidebar = source.privateSidebar
        palette.sidebarOpaque = source.privateSidebarOpaque
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

    Typography {
        id: typography
        palette: window.colors
    }

    KeyMap {
        id: keymap
        configuration: keyboardNavigation
    }

    readonly property var commands: browserCommands

    BrowserCommands {
        id: browserCommands
        window: window
        browser: window.windowBrowser
        keymap: keymap
    }

    function openCommandPanel() {
        commandPanel.beginCommand()
        omnibarOpen = true
    }

    function requestMoveTab() {
        if (!privateWindow) moveTabDialog.open()
    }

    function requestNewSpace() {
        if (!privateWindow) newSpaceDialog.open()
    }

    function requestSettings() {
        settingsDialog.open()
    }

    function stepTab(delta) {
        const tabs = window.windowBrowser.tabs
        const count = tabs.rowCount()
        if (count === 0) return
        let current = 0
        for (let row = 0; row < count; ++row) {
            if (tabs.data(tabs.index(row, 0), Qt.UserRole + 6)) {
                current = row
                break
            }
        }
        const next = (current + delta + count) % count
        window.windowBrowser.activateTab(tabs.data(tabs.index(next, 0), Qt.UserRole + 1))
    }

    function activateTabAt(position) {
        const tabs = window.windowBrowser.tabs
        if (position < 0 || position >= tabs.rowCount()) return
        window.windowBrowser.activateTab(tabs.data(tabs.index(position, 0), Qt.UserRole + 1))
    }

    function stepSpace(delta) {
        if (privateWindow) return
        const spaces = window.windowBrowser.spaces
        const count = spaces.rowCount()
        if (count === 0) return
        let current = 0
        for (let row = 0; row < count; ++row) {
            if (spaces.data(spaces.index(row, 0), Qt.UserRole + 4)) {
                current = row
                break
            }
        }
        const next = (current + delta + count) % count
        window.windowBrowser.switchSpace(spaces.data(spaces.index(next, 0), Qt.UserRole + 1))
    }

    function activateSpaceAt(position) {
        if (privateWindow) return
        const spaces = window.windowBrowser.spaces
        if (position < 0 || position >= spaces.rowCount()) return
        window.windowBrowser.switchSpace(spaces.data(spaces.index(position, 0), Qt.UserRole + 1))
    }

    function openOmnibar(forNewTab) {
        newTabIntent = forNewTab
        const preset = forNewTab ? "" : window.windowBrowser.activeUrl.toString()
        omnibarSuggestions = window.privateWindow
            ? [] : window.windowBrowser.historySuggestions(preset)
        commandPanel.beginAddress(preset, forNewTab)
        omnibarOpen = true
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
        engineLoader.focusPage()
    }

    // Every binding — chord, single key, or sequence — comes from the keyboard
    // configuration, so rebinding is editing assets/keybindings/default.json.
    // Chords are always live. Single keys follow the Keyboard navigation
    // setting, because only they can be confused with typing on a page.
    Repeater {
        model: Object.keys(keymap.browserBindings)

        Item {
            required property string modelData

            Shortcut {
                sequence: keymap.keySequence(modelData)
                enabled: keymap.isChord(modelData) || keymap.pageCommandsEnabled
                context: Qt.WindowShortcut
                onActivated: browserCommands.run(
                    keymap.commandFor(modelData),
                    parseInt(modelData.slice(-1), 10) - 1)
            }
        }
    }

    Rectangle {
        id: shell
        anchors.fill: parent
        radius: window.cornerRadius
        color: window.colors.window
        border.width: 1
        border.color: window.colors.border
        clip: true

        RowLayout {
            anchors.fill: parent
            spacing: 0

            SpaceOutline {
                id: sidebar
                objectName: "sidebar"
                Layout.fillHeight: true
                Layout.preferredWidth: window.sidebarCollapsed ? 0 : 292
                visible: Layout.preferredWidth > 0
                colors: window.colors
                typography: typography
                iconFontFamily: materialSymbols.name
                browser: window.windowBrowser
                privateWindow: window.privateWindow
                collapsed: window.sidebarCollapsed
                blockedRequestCount: window.visibleBlockedRequestCount

                Behavior on Layout.preferredWidth {
                    NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
                }

                onAddressRequested: window.openOmnibar(false)
                onNewTabRequested: window.openOmnibar(true)
                onTabActivated: function(tabId) { window.windowBrowser.activateTab(tabId) }
                onSpaceActivated: function(spaceId) { window.windowBrowser.switchSpace(spaceId) }
                onSpacesMenuRequested: spacesMenu.popup()
                onSettingsRequested: settingsDialog.open()
                onWindowMoveRequested: window.startSystemMove()

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
                    MenuItem {
                        text: "Move this tab to a Space"
                        onTriggered: moveTabDialog.open()
                    }
                }
            }

            Item {
                objectName: "engineViewport"
                Layout.fillWidth: true
                Layout.fillHeight: true

                // The shell around it is translucent by theme; a webpage viewport
                // never is, so it gets its own opaque backing rather than
                // inheriting whatever the desktop is showing.
                Rectangle {
                    objectName: "engineBacking"
                    anchors.fill: parent
                    color: window.colors.windowOpaque
                }

                TabEngineHost {
                    id: engineLoader
                    anchors.fill: parent
                    focus: true
                    browserController: window.windowBrowser
                    engineSource: engineViewSource
                    profilePath: window.profilePathOverride.length > 0
                        ? window.profilePathOverride
                        : window.windowBrowser.activeProfilePath
                    sharedProfile: window.privateWindow
                        ? window.sharedEngineProfile
                        : (window.spaceProfileHost ? window.spaceProfileHost.profile : null)
                    permissionController: window.windowBrowser
                    blocker: contentBlocker
                    engineBlocker: engineContentBlocker
                    keyboardManager: keyboardNavigation

                    onAuxiliaryWindowRequested: function(engine, request, requestedUrl) {
                        auxiliaryWindowComponent.createObject(window, {
                            "openerEngine": engine,
                            "request": request,
                            "requestedUrl": requestedUrl
                        })
                    }

                    onNewTabRequested: function(engine, request, requestedUrl) {
                        const destination = requestedUrl.toString().length > 0
                            ? requestedUrl.toString()
                            : "about:blank"
                        window.windowBrowser.openInput(request ? "about:blank" : destination, true)
                        if (request) {
                            if (engineLoader.item) {
                                engineLoader.item.acceptNewWindowRequest(request)
                            } else {
                                Qt.callLater(function() {
                                    if (engineLoader.item)
                                        engineLoader.item.acceptNewWindowRequest(request)
                                })
                            }
                        }
                    }

                    onBackgroundTabRequested: function(requestedUrl) {
                        window.windowBrowser.openInputInBackground(requestedUrl)
                    }

                    onSitePermissionRequested: function(engine, requestId, origin, permission) {
                        window.pendingPermissionRequest = requestId
                        window.pendingPermissionResponder = engine
                        window.pendingPermissionOrigin = origin
                        window.pendingPermissionType = permission
                        permissionDialog.open()
                    }
                }

                NavigationCluster {
                    anchors.left: parent.left
                    anchors.leftMargin: 16
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 20
                    z: 5
                    colors: window.colors
                    typography: typography
                    iconFontFamily: materialSymbols.name
                    canGoBack: engineLoader.item ? engineLoader.item.canGoBack : false
                    canGoForward: engineLoader.item ? engineLoader.item.canGoForward : false
                    sidebarCollapsed: window.sidebarCollapsed

                    onBackRequested: window.windowBrowser.requestBack()
                    onForwardRequested: window.windowBrowser.requestForward()
                    onReloadRequested: window.windowBrowser.requestReload()
                    onSidebarToggled: window.sidebarCollapsed = !window.sidebarCollapsed
                    onCommandPanelRequested: window.openCommandPanel()
                }

                Rectangle {
                    objectName: "spaceEdge"
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 3
                    visible: window.sidebarCollapsed
                    color: window.privateWindow ? window.colors.privateAccent : window.colors.accent
                    opacity: 0.8
                    z: 6
                }

                Connections {
                    target: window.windowBrowser

                    function onTabMoveConfirmationRequested(tabId, destinationSpaceId) {
                        window.pendingMoveTabId = tabId
                        window.pendingMoveSpaceId = destinationSpaceId
                        confirmTabMoveDialog.open()
                    }

                    function onSpaceSuspended(spaceId) {
                        engineLoader.suspend()
                        if (window.spaceProfileHost) {
                            window.spaceProfileHost.retire()
                            window.spaceProfileHost = null
                        }
                    }

                    function onSpaceRestored(spaceId) {
                        if (spaceId === window.windowBrowser.activeSpaceId) {
                            Qt.callLater(function() {
                                window.createSpaceProfile()
                                engineLoader.resume()
                            })
                        }
                    }

                    function onCloseWindowRequested() {
                        if (window.privateWindow) window.close()
                    }
                }

                Rectangle {
                    anchors.fill: parent
                    visible: window.windowBrowser.activeRendererFailed
                    color: window.colors.windowOpaque
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

    Component.onCompleted: {
        window.createSpaceProfile()
        engineLoader.resume()
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
        title: "Browser settings"
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
                text: "Keyboard navigation"
                font.weight: Font.DemiBold
            }
            CheckBox {
                objectName: "keyboardNavigationEnabled"
                text: "Enable Keyboard navigation"
                checked: keyboardNavigation.enabled
                onClicked: keyboardNavigation.setEnabled(checked)
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
                        engineLoader.checkForEditedFormState(function(hasEditedFormState) {
                            window.windowBrowser.requestTabMoveToSpace(
                                tabId, destinationSpaceId, hasEditedFormState)
                        })
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

    CommandPanel {
        id: commandPanel
        anchors.fill: parent
        z: 50
        colors: window.colors
        typography: typography
        commands: browserCommands
        open: window.omnibarOpen
        suggestions: window.omnibarSuggestions

        onDismissed: window.closeOmnibar()
        onQueryChanged: function(text) {
            if (commandPanel.commandMode) return
            window.omnibarSuggestions = window.privateWindow
                ? [] : window.windowBrowser.historySuggestions(text)
        }
        onCommitted: function(text) {
            window.windowBrowser.openInput(text, window.newTabIntent)
            window.closeOmnibar()
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
