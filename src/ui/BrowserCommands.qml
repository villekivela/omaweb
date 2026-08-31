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
        case "open-address": window.openOmnibar(false); return true
        case "command-panel": window.openCommandPanel(); return true
        case "new-tab": window.openOmnibar(true); return true
        case "close-tab": browser.closeActiveTab(); return true
        case "reopen-tab": browser.reopenClosedTab(); return true
        case "next-tab": window.stepTab(1); return true
        case "previous-tab": window.stepTab(-1); return true
        case "select-tab": window.activateTabAt(argument); return true
        case "pin-tab": browser.toggleActivePinned(); return true
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
        case "settings": window.requestSettings(); return true
        case "private-window": windowManager.openPrivateWindow(); return true
        case "minimize-window": window.showMinimized(); return true
        }
        return false
    }

    readonly property var descriptions: ({
        "back": { group: "navigation", title: "Back" },
        "forward": { group: "navigation", title: "Forward" },
        "reload": { group: "navigation", title: "Reload" },
        "open-address": { group: "navigation", title: "Open address" },
        "command-panel": { group: "interface", title: "Command panel" },
        "new-tab": { group: "tabs", title: "New tab" },
        "close-tab": { group: "tabs", title: "Close tab" },
        "reopen-tab": { group: "tabs", title: "Reopen closed tab" },
        "next-tab": { group: "tabs", title: "Next tab" },
        "previous-tab": { group: "tabs", title: "Previous tab" },
        "select-tab": { group: "tabs", title: "Jump to tab by number" },
        "pin-tab": { group: "tabs", title: "Pin or unpin this tab" },
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
        "settings": { group: "interface", title: "Settings and downloads" },
        "private-window": { group: "interface", title: "New Private window" },
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

    function actions() {
        const list = []

        for (const command in descriptions) {
            const description = descriptions[command]
            if (window.privateWindow
                && (command === "pin-tab" || command === "move-tab"
                    || command === "select-space" || command === "next-space"
                    || command === "new-space")) {
                continue
            }
            list.push({
                group: description.group,
                title: description.title,
                keys: keymap.keysFor(command),
                enabled: true,
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
