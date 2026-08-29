import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window

    width: 1360
    height: 860
    minimumWidth: 840
    minimumHeight: 560
    visible: true
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint
    title: browser.activeTitle + " — Tanto"

    property var colors: theme.palette
    property bool sidebarCollapsed: false
    property bool omnibarOpen: false
    property bool newTabIntent: false
    property string pendingMoveTabId: ""
    property string pendingMoveSpaceId: ""

    FontLoader {
        id: materialSymbols
        objectName: "materialSymbolsFont"
        source: iconFontSource
    }

    function openOmnibar(forNewTab) {
        newTabIntent = forNewTab
        omnibarInput.text = forNewTab ? "" : browser.activeUrl.toString()
        omnibarOpen = true
        Qt.callLater(function() {
            omnibarInput.forceActiveFocus()
            omnibarInput.selectAll()
        })
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
        onActivated: browser.closeActiveTab()
    }

    Shortcut {
        sequence: Qt.platform.os === "osx" ? "Meta+Shift+T" : "Ctrl+Shift+T"
        onActivated: browser.reopenClosedTab()
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
                            onClicked: browser.requestBack()
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
                            onClicked: browser.requestForward()
                        }

                        ChromeButton {
                            objectName: "reloadButton"
                            Layout.fillWidth: true
                            label: "refresh"
                            accessibleName: "Reload"
                            fontFamily: materialSymbols.name
                            foreground: window.colors.text
                            hoverBackground: window.colors.surfaceHover
                            onClicked: browser.requestReload()
                        }

                        ChromeButton {
                            objectName: "pinButton"
                            Layout.fillWidth: true
                            label: browser.activeTabPinned ? "bookmark" : "bookmark_border"
                            accessibleName: browser.activeTabPinned ? "Unpin tab" : "Pin tab"
                            fontFamily: materialSymbols.name
                            foreground: browser.activeTabPinned ? window.colors.accent : window.colors.text
                            hoverBackground: window.colors.surfaceHover
                            onClicked: browser.toggleActivePinned()
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
                            text: browser.activeUrl.toString() === "about:blank"
                                ? "Search or enter address"
                                : browser.activeUrl.toString()
                            color: browser.activeUrl.toString() === "about:blank"
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
                        columnSpacing: 6
                        rowSpacing: 6

                        Repeater {
                            model: browser.tabs

                            PinnedTabDelegate {
                                width: window.sidebarCollapsed
                                    ? 38
                                    : (sidebar.width - 20 - 18) / 4
                                visible: pinned
                                height: visible ? (window.sidebarCollapsed ? 38 : 54) : 0
                                colors: window.colors
                                iconFontFamily: materialSymbols.name
                                onActivated: function(id) { browser.activateTab(id) }
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
                            visible: !window.sidebarCollapsed
                            model: browser.spaces
                            textRole: "spaceName"
                            valueRole: "spaceId"
                            displayText: browser.activeSpaceName
                            Accessible.name: "Active Space"
                            onActivated: browser.switchSpace(currentValue)
                        }

                        ChromeButton {
                            id: manageSpacesButton
                            objectName: "manageSpacesButton"
                            Layout.preferredWidth: 32
                            Layout.fillHeight: true
                            visible: !window.sidebarCollapsed
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
                                    text: "Rename " + browser.activeSpaceName
                                    onTriggered: renameSpaceDialog.open()
                                }
                                MenuItem {
                                    text: "Delete " + browser.activeSpaceName
                                    onTriggered: deleteSpaceDialog.open()
                                }
                            }
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
                                model: browser.tabs

                                TabDelegate {
                                    width: parent.width
                                    visible: !pinned
                                    height: visible ? 40 : 0
                                    colors: window.colors
                                    onActivated: function(id) { browser.activateTab(id) }
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
                        setSource(engineViewSource, {
                            "profilePath": browser.activeProfilePath,
                            "currentUrl": browser.activeUrl
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
                    value: browser.activeProfilePath
                    when: engineLoader.item !== null
                }

                Connections {
                    target: engineLoader.item
                    ignoreUnknownSignals: true

                    function onCurrentUrlChanged() {
                        browser.updateActiveTab(engineLoader.item.currentUrl, engineLoader.item.pageTitle)
                    }

                    function onPageTitleChanged() {
                        browser.updateActiveTab(engineLoader.item.currentUrl, engineLoader.item.pageTitle)
                    }

                    function onLoadingChanged() {
                        browser.setActiveLoading(engineLoader.item.loading)
                    }

                    function onRendererFailed(reason) {
                        browser.reportRendererFailure(reason)
                    }
                }

                Connections {
                    target: browser

                    function onTabMoveConfirmationRequested(tabId, destinationSpaceId) {
                        window.pendingMoveTabId = tabId
                        window.pendingMoveSpaceId = destinationSpaceId
                        confirmTabMoveDialog.open()
                    }

                    function onSpaceSuspended(spaceId) {
                        engineLoader.source = ""
                    }

                    function onSpaceRestored(spaceId) {
                        if (spaceId === browser.activeSpaceId)
                            Qt.callLater(function() { engineLoader.loadActiveSpace() })
                    }

                    function onActiveTabChanged() {
                        if (engineLoader.item && engineLoader.item.currentUrl !== browser.activeUrl) {
                            engineLoader.item.currentUrl = browser.activeUrl
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
                }

                Rectangle {
                    anchors.fill: parent
                    visible: browser.activeRendererFailed
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
                            text: browser.activeRendererFailureReason
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
                            onClicked: browser.recoverActiveTab()
                        }
                    }
                }
            }
        }
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
            const spaceId = browser.createSpace(newSpaceName.text)
            if (spaceId.length > 0) browser.switchSpace(spaceId)
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
        id: renameSpaceDialog
        objectName: "renameSpaceDialog"
        anchors.centerIn: parent
        modal: true
        title: "Rename Space"
        standardButtons: Dialog.Ok | Dialog.Cancel
        onOpened: {
            renamedSpaceName.text = browser.activeSpaceName
            renamedSpaceName.forceActiveFocus()
            renamedSpaceName.selectAll()
        }
        onAccepted: browser.renameSpace(browser.activeSpaceId, renamedSpaceName.text)

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
        title: "Delete " + browser.activeSpaceName
        standardButtons: Dialog.Ok | Dialog.Cancel
        onOpened: {
            deleteSpaceConfirmation.text = ""
            deleteSpaceConfirmation.forceActiveFocus()
        }
        onAccepted: browser.deleteSpace(
            browser.activeSpaceId, deleteSpaceConfirmation.text)

        ColumnLayout {
            width: 320
            spacing: 10

            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: "Type " + browser.activeSpaceName
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
                model: browser.spaces

                Button {
                    Layout.fillWidth: true
                    visible: spaceId !== browser.activeSpaceId
                    text: spaceName
                    Accessible.name: "Move tab to " + spaceName
                    onClicked: {
                        const tabId = browser.activeTabId
                        const destinationSpaceId = spaceId
                        moveTabDialog.close()
                        if (engineLoader.item) {
                            engineLoader.item.checkForEditedFormState(function(hasEditedFormState) {
                                browser.requestTabMoveToSpace(
                                    tabId, destinationSpaceId, hasEditedFormState)
                            })
                        } else {
                            browser.requestTabMoveToSpace(tabId, destinationSpaceId, true)
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
        onAccepted: browser.confirmTabMoveToSpace(
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
            height: 70
            anchors.centerIn: parent
            radius: 16
            color: window.colors.overlay
            border.width: 1
            border.color: window.colors.border

            TextField {
                id: omnibarInput
                objectName: "omnibarInput"
                anchors.fill: parent
                anchors.margins: 12
                background: null
                color: window.colors.text
                placeholderText: window.newTabIntent ? "Open in new tab" : "Search or enter address"
                placeholderTextColor: window.colors.mutedText
                font.pixelSize: 19
                selectByMouse: true
                Accessible.name: "Omnibar"

                onAccepted: {
                    if (text.trim().length === 0) return
                    browser.openInput(text, window.newTabIntent)
                    window.closeOmnibar()
                }

                Keys.onEscapePressed: function(event) {
                    window.closeOmnibar()
                    event.accepted = true
                }
            }
        }
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
