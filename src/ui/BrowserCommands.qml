import QtQuick

QtObject {
    id: root

    // The single registry of everything Tanto can do. The command panel reads
    // it, the keymap dispatches into it, and nothing else may define an action:
    // a command missing here is unreachable by keyboard and unsearchable.
    property var window
    property var browser
    property var keymap

    function run(command, argument) {
        switch (command) {
        case "back": browser.requestBack(); return true
        case "forward": browser.requestForward(); return true
        case "reload": browser.requestReload(); return true
        case "reload-bypassing-cache": window.reloadBypassingCache(); return true
        case "stop-loading": window.stopLoading(); return true
        case "open-address": window.openOmnibar(false); return true
        case "command-panel": window.openCommandPanel(); return true
        case "new-tab": window.openOmnibar(true); return true
        case "close-tab": browser.closeActiveTab(); return true
        case "reopen-tab": browser.reopenClosedTab(); return true
        case "next-tab": window.stepTab(1); return true
        case "previous-tab": window.stepTab(-1); return true
        case "select-tab": window.activateTabAt(argument); return true
        case "pin-tab": browser.toggleActivePinned(); return true
        case "keep-tab-active": browser.toggleActiveKeepActive(); return true
        case "duplicate-tab": browser.duplicateTab(browser.activeTabId); return true
        case "move-tab-up": browser.moveTabBy(browser.activeTabId, -1); return true
        case "move-tab-down": browser.moveTabBy(browser.activeTabId, 1); return true
        case "close-other-tabs": browser.closeOtherTabs(browser.activeTabId); return true
        case "close-tabs-below": browser.closeTabsBelow(browser.activeTabId); return true
        case "tab-menu": window.openActiveTabMenu(); return true
        case "move-tab": window.requestMoveTab(); return true
        case "next-space": window.stepSpace(1); return true
        case "select-space": window.activateSpaceAt(argument); return true
        case "new-space": window.requestNewSpace(); return true
        case "toggle-sidebar": window.sidebarCollapsed = !window.sidebarCollapsed; return true
        case "widen-sidebar": window.nudgeSidebar(24); return true
        case "narrow-sidebar": window.nudgeSidebar(-24); return true
        case "reset-sidebar": window.setSidebarWidth(window.sidebarDefaultWidth); return true
        case "focus-sidebar": window.focusSidebar(); return true
        case "focus-page": window.focusPage(); return true
        case "copy-address": window.copyAddress(); return true
        case "find": window.openFind(); return true
        case "find-next": window.stepFind(true); return true
        case "find-previous": window.stepFind(false); return true
        case "zoom-in": window.stepZoom(1); return true
        case "zoom-out": window.stepZoom(-1); return true
        case "zoom-reset": window.resetZoom(); return true
        case "print": window.printPage(); return true
        case "fullscreen": window.toggleBrowserFullscreen(); return true
        case "developer-tools": window.toggleDeveloperTools(); return true
        case "inspect-element": window.inspectElement(); return true
        case "open-page-context-menu": window.openPageContextMenu(); return true
        case "open-file": window.requestOpenFile(); return true
        case "shortcuts": window.requestShortcuts(); return true
        case "history": window.requestHistory(); return true
        case "settings": window.requestSettings(); return true
        case "private-window": windowManager.openPrivateWindow(); return true
        case "minimize-window": window.showMinimized(); return true
        }
        return false
    }

    // Every command names what it needs to run here, so availability is read
    // off the same table as the title rather than off a cascade beside it: a
    // command added without a requirement runs everywhere, and one that needs
    // something says which thing in the row that declares it.
    readonly property var descriptions: ({
        "back": { group: "navigation", title: "Back" },
        "forward": { group: "navigation", title: "Forward" },
        "reload": { group: "navigation", title: "Reload" },
        "reload-bypassing-cache": { group: "navigation", title: "Reload bypassing cache",
            requires: "page" },
        "stop-loading": { group: "navigation", title: "Stop loading", requires: "page" },
        "open-address": { group: "navigation", title: "Open address" },
        "command-panel": { group: "interface", title: "Command panel" },
        "new-tab": { group: "tabs", title: "New tab" },
        "close-tab": { group: "tabs", title: "Close tab" },
        "reopen-tab": { group: "tabs", title: "Reopen closed tab" },
        "next-tab": { group: "tabs", title: "Next tab" },
        "previous-tab": { group: "tabs", title: "Previous tab" },
        "select-tab": { group: "tabs", title: "Jump to tab by number" },
        "pin-tab": { group: "tabs", title: "Pin or unpin this tab" },
        "keep-tab-active": { group: "tabs", title: "Keep this Pinned tab active",
            requires: "pinned-tab" },
        "duplicate-tab": { group: "tabs", title: "Duplicate tab", requires: "page" },
        "move-tab-up": { group: "tabs", title: "Move tab up" },
        "move-tab-down": { group: "tabs", title: "Move tab down" },
        "close-other-tabs": { group: "tabs", title: "Close other tabs" },
        "close-tabs-below": { group: "tabs", title: "Close tabs below",
            requires: "ordinary-tab" },
        "tab-menu": { group: "tabs", title: "Open tab menu" },
        "move-tab": { group: "tabs", title: "Move tab to another Space" },
        "next-space": { group: "spaces", title: "Next Space" },
        "select-space": { group: "spaces", title: "Switch Space by number" },
        "new-space": { group: "spaces", title: "New Space" },
        "toggle-sidebar": { group: "interface", title: "Hide or show the sidebar" },
        "widen-sidebar": { group: "interface", title: "Widen the sidebar" },
        "narrow-sidebar": { group: "interface", title: "Narrow the sidebar" },
        "reset-sidebar": { group: "interface", title: "Reset the sidebar width" },
        "focus-sidebar": { group: "interface", title: "Focus the sidebar" },
        "focus-page": { group: "interface", title: "Focus the page" },
        "copy-address": { group: "navigation", title: "Copy address", requires: "page" },
        "find": { group: "page", title: "Find in page", requires: "find" },
        "find-next": { group: "page", title: "Next match", requires: "find" },
        "find-previous": { group: "page", title: "Previous match", requires: "find" },
        "zoom-in": { group: "page", title: "Zoom in", requires: "zoom" },
        "zoom-out": { group: "page", title: "Zoom out", requires: "zoom" },
        "zoom-reset": { group: "page", title: "Reset zoom", requires: "zoom" },
        "print": { group: "page", title: "Print", requires: "printing" },
        "fullscreen": { group: "interface", title: "Fullscreen" },
        "developer-tools": { group: "developer", title: "Developer tools",
            requires: "inspector" },
        "inspect-element": { group: "developer", title: "Inspect element",
            requires: "inspector" },
        "open-page-context-menu": { group: "page", title: "Open page context menu",
            requires: "page" },
        "open-file": { group: "navigation", title: "Open file" },
        "shortcuts": { group: "interface", title: "Keyboard shortcuts" },
        "history": { group: "interface", title: "History", requires: "ordinary-window" },
        "settings": { group: "interface", title: "Settings and downloads" },
        "private-window": { group: "interface", title: "New Private window",
            requires: "private-windows" },
        "minimize-window": { group: "interface", title: "Minimize window" }
    })

    readonly property var pageDescriptions: ({
        "scroll-down": "Scroll down",
        "scroll-up": "Scroll up",
        "scroll-half-page-down": "Scroll down half a page",
        "scroll-half-page-up": "Scroll up half a page",
        "scroll-top": "Top of page",
        "scroll-bottom": "Bottom of page",
        "open-link": "Follow link hint",
        "open-link-background": "Follow link hint in a background tab"
    })

    // A command the current engine or window cannot carry out is listed and
    // unavailable rather than missing: the reader learns it exists, and why it
    // is not on offer here.
    function available(command) {
        const description = descriptions[command]
        switch (description ? description.requires : "") {
        case "page": return !browser.activeTabBlank
        case "find": return window.findAvailable
        case "zoom": return window.zoomAvailable
        case "printing": return window.printingAvailable
        case "inspector": return window.developerToolsAvailable
        case "private-windows": return windowManager.privateWindowsAvailable
        case "ordinary-window": return !window.privateWindow
        // Keep active is a Pinned tab's setting, and the rows below a tab are
        // the ordinary list's.
        case "pinned-tab": return browser.activeTabPinned && !window.privateWindow
        case "ordinary-tab": return !browser.activeTabPinned
        }
        return true
    }

    function actions() {
        const list = []

        for (const command in descriptions) {
            const description = descriptions[command]
            if (window.privateWindow
                && (command === "pin-tab" || command === "move-tab"
                    || command === "keep-tab-active"
                    || command === "select-space" || command === "next-space"
                    || command === "new-space")) {
                continue
            }
            list.push({
                group: description.group,
                title: description.title,
                keys: keymap.keysFor(command),
                enabled: root.available(command),
                command: command,
                argument: -1
            })
        }

        const tabs = browser.tabs
        for (let row = 0; row < tabs.rowCount(); ++row) {
            const index = tabs.index(row, 0)
            if (tabs.data(index, Qt.UserRole + 6)) {
                continue
            }
            const tabId = tabs.data(index, Qt.UserRole + 1)
            list.push({
                group: "open tabs",
                title: tabs.data(index, Qt.UserRole + 4),
                keys: tabs.data(index, Qt.UserRole + 5)
                    ? "pinned"
                    : (row < 9 ? keymap.displayFor(String(row + 1)) : ""),
                enabled: true,
                command: "activate-tab",
                argument: tabId
            })
        }

        if (!window.privateWindow) {
            const spaces = browser.spaces
            for (let row = 0; row < spaces.rowCount(); ++row) {
                const index = spaces.index(row, 0)
                if (spaces.data(index, Qt.UserRole + 4)) {
                    continue
                }
                list.push({
                    group: "spaces",
                    title: "Switch to " + spaces.data(index, Qt.UserRole + 2),
                    keys: row < 9 ? keymap.displayFor("Primary+" + (row + 1)) : "",
                    enabled: true,
                    command: "switch-space",
                    argument: spaces.data(index, Qt.UserRole + 1)
                })
            }
        }

        for (const pageCommand in pageDescriptions) {
            list.push({
                group: "page",
                title: pageDescriptions[pageCommand],
                keys: keymap.pageKeysFor(pageCommand),
                enabled: keymap.pageCommandsEnabled,
                command: "",
                argument: -1
            })
        }

        return list
    }

    function invoke(action) {
        if (action.command === "activate-tab") {
            browser.activateTab(action.argument)
            return
        }
        if (action.command === "switch-space") {
            browser.switchSpace(action.argument)
            return
        }
        run(action.command, action.argument)
    }

    function score(text, query) {
        if (query.length === 0) {
            return 1
        }
        const haystack = text.toLowerCase()
        const needle = query.toLowerCase()
        let cursor = 0
        let points = 0
        let previous = -2
        for (let index = 0; index < needle.length; ++index) {
            const found = haystack.indexOf(needle.charAt(index), cursor)
            if (found === -1) {
                return 0
            }
            points += 1
            if (found === previous + 1) {
                points += 3
            }
            if (found === 0 || haystack.charAt(found - 1) === " ") {
                points += 2
            }
            previous = found
            cursor = found + 1
        }
        return points
    }

    function highlight(text, query) {
        if (query.length === 0) {
            return text
        }
        const haystack = text.toLowerCase()
        const needle = query.toLowerCase()
        let cursor = 0
        let out = ""
        for (let index = 0; index < needle.length; ++index) {
            const found = haystack.indexOf(needle.charAt(index), cursor)
            if (found === -1) {
                return text
            }
            out += text.substring(cursor, found) + "<b>" + text.charAt(found) + "</b>"
            cursor = found + 1
        }
        return out + text.substring(cursor)
    }

    function search(query) {
        const all = actions()
        const matched = []
        for (let index = 0; index < all.length; ++index) {
            const points = score(all[index].title, query)
            if (points > 0) {
                matched.push({ action: all[index], points: points, order: index })
            }
        }
        matched.sort(function(left, right) {
            return right.points - left.points || left.order - right.order
        })
        const result = []
        for (let index = 0; index < matched.length; ++index) {
            result.push(matched[index].action)
        }
        return result
    }
}
