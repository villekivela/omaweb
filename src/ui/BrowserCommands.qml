import QtQuick

QtObject {
    id: root

    // The single registry of everything Omaweb can do. The Omnibar reads it,
    // the keymap dispatches into it, and nothing else may define an action:
    // a command missing here is unreachable by keyboard and unsearchable.
    property var window
    property var browser
    property var engineHost
    property var keymap

    // A tab's id, where a command was given one rather than a position.
    function tabArgument(argument) {
        return typeof argument === "string" ? argument : "";
    }

    function run(command, argument) {
        switch (command) {
        case "back":
            engineHost.goBack();
            return true;
        case "forward":
            engineHost.goForward();
            return true;
        case "reload":
            engineHost.reloadPage();
            return true;
        case "reload-bypassing-cache":
            window.reloadBypassingCache();
            return true;
        case "stop-loading":
            window.stopLoading();
            return true;
        case "open-address":
            window.openOmnibar(false);
            return true;
        case "command-scope":
            window.openCommandScope();
            return true;
        case "new-tab":
            window.openOmnibar(true);
            return true;
        case "close-tab":
            window.closeActiveTab();
            return true;
        case "extension-popup":
            window.openExtensionMenu(Qt.rect(0, 0, 0, 0));
            return true;
        case "glance-to-tab":
            window.openGlanceAsTab();
            return true;
        case "reopen-tab":
            browser.reopenClosedTab();
            return true;
        case "next-tab":
            window.stepTab(1);
            return true;
        case "previous-tab":
            window.stepTab(-1);
            return true;
            // Run even with nowhere to go, as back is on a page without history:
            // the key is the reader's either way, and nothing is shown.
        case "jump-back":
            browser.jumpBack();
            return true;
        case "jump-forward":
            browser.jumpForward();
            return true;
        case "select-tab":
            window.activateTabAt(argument);
            return true;
        case "pin-tab":
            browser.toggleActivePinned();
            return true;
        case "keep-tab-active":
            browser.toggleActiveKeepActive();
            return true;
        case "duplicate-tab":
            browser.duplicateTab(browser.activeTabId);
            return true;
        case "move-tab-up":
            browser.moveTabBy(browser.activeTabId, -1);
            return true;
        case "move-tab-down":
            browser.moveTabBy(browser.activeTabId, 1);
            return true;
        case "close-other-tabs":
            browser.closeOtherTabs(browser.activeTabId);
            return true;
        case "close-tabs-below":
            browser.closeTabsBelow(browser.activeTabId);
            return true;
        case "tab-menu":
            window.openActiveTabMenu();
            return true;
        case "move-tab":
            window.requestMoveTab();
            return true;
        case "add-split":
            window.requestAddSplit();
            return true;
        case "separate-split":
            browser.separateSplit();
            return true;
        case "focus-split-partner":
            browser.focusSplitPartner();
            return true;
            // A tab named by its id, from `omaweb run`, or the tab on show.
        case "pop-out-tab":
            return window.popOutTab(root.tabArgument(argument));
            // A named tab, or the Tab window the reader was in last.
        case "put-back-tab":
            return window.putBackTab(root.tabArgument(argument));
        case "toggle-tab-window-strip":
            return window.toggleTabWindowStrip(root.tabArgument(argument));
        case "next-space":
            window.stepSpace(1);
            return true;
        case "select-space":
            window.activateSpaceAt(argument);
            return true;
        case "new-space":
            window.requestNewSpace();
            return true;
        case "take-over-space":
            return window.takeOverSpace();
        case "toggle-sidebar":
            window.sidebarCollapsed = !window.sidebarCollapsed;
            return true;
        case "widen-sidebar":
            window.nudgeSidebar(24);
            return true;
        case "narrow-sidebar":
            window.nudgeSidebar(-24);
            return true;
        case "reset-sidebar":
            window.setSidebarWidth(window.sidebarDefaultWidth);
            return true;
        case "focus-sidebar":
            window.focusSidebar();
            return true;
        case "focus-page":
            window.focusPage();
            return true;
        case "move-focus-left":
            window.moveFocus(-1, 0);
            return true;
        case "move-focus-down":
            window.moveFocus(0, 1);
            return true;
        case "move-focus-up":
            window.moveFocus(0, -1);
            return true;
        case "move-focus-right":
            window.moveFocus(1, 0);
            return true;
        case "copy-address":
            window.copyAddress();
            return true;
        case "site-information":
            window.openSiteInformation("");
            return true;
        case "find":
            window.openFind();
            return true;
        case "find-next":
            window.stepFind(true);
            return true;
        case "find-previous":
            window.stepFind(false);
            return true;
        case "zoom-in":
            window.stepZoom(1);
            return true;
        case "zoom-out":
            window.stepZoom(-1);
            return true;
        case "zoom-reset":
            window.resetZoom();
            return true;
        case "print":
            window.printPage();
            return true;
        case "screenshot-page":
            window.screenshotPage(root.descriptions[command].title, false, false);
            return true;
        case "copy-screenshot":
            window.screenshotPage(root.descriptions[command].title, true, false);
            return true;
        case "screenshot-full-page":
            window.screenshotPage(root.descriptions[command].title, false, true);
            return true;
        case "copy-full-page-screenshot":
            window.screenshotPage(root.descriptions[command].title, true, true);
            return true;
        case "fullscreen":
            window.toggleBrowserFullscreen();
            return true;
        case "developer-tools":
            window.toggleDeveloperTools();
            return true;
        case "inspect-element":
            window.inspectElement();
            return true;
        case "open-page-context-menu":
            window.openPageContextMenu();
            return true;
        case "open-file":
            window.requestOpenFile();
            return true;
        case "shortcuts":
            window.requestShortcuts();
            return true;
        case "history":
            window.requestHistory();
            return true;
        case "agent-activity":
            return browser.openAgentActivity();
        case "ask":
            return window.askAgent(typeof argument === "string" ? argument : "");
        case "settings":
            window.requestSettings();
            return true;
        case "downloads":
            window.requestDownloads();
            return true;
        case "private-window":
            windowManager.openPrivateWindow();
            return true;
        case "minimize-window":
            window.showMinimized();
            return true;
        }
        return false;
    }

    // Every command names what it needs to run here, so availability is read
    // off the same table as the title rather than off a cascade beside it: a
    // command added without a requirement runs everywhere, and one that needs
    // something says which thing in the row that declares it.
    readonly property var descriptions: ({
                                             "back": {
                                                 group: "navigation",
                                                 title: qsTr("Back")
                                             },
                                             "forward": {
                                                 group: "navigation",
                                                 title: qsTr("Forward")
                                             },
                                             "reload": {
                                                 group: "navigation",
                                                 title: qsTr("Reload")
                                             },
                                             "reload-bypassing-cache": {
                                                 group: "navigation",
                                                 title: qsTr("Reload bypassing cache"),
                                                 requires: "page"
                                             },
                                             "stop-loading": {
                                                 group: "navigation",
                                                 title: qsTr("Stop loading"),
                                                 requires: "page"
                                             },
                                             "open-address": {
                                                 group: "navigation",
                                                 title: qsTr("Open address")
                                             },
                                             "command-scope": {
                                                 group: "interface",
                                                 title: qsTr("Search commands")
                                             },
                                             "new-tab": {
                                                 group: "tabs",
                                                 title: qsTr("New tab")
                                             },
                                             "close-tab": {
                                                 group: "tabs",
                                                 title: qsTr("Close tab")
                                             },
                                             "reopen-tab": {
                                                 group: "tabs",
                                                 title: qsTr("Reopen closed tab")
                                             },
                                             "next-tab": {
                                                 group: "tabs",
                                                 title: qsTr("Next tab")
                                             },
                                             "previous-tab": {
                                                 group: "tabs",
                                                 title: qsTr("Previous tab")
                                             },
                                             "jump-back": {
                                                 group: "tabs",
                                                 title: qsTr("Jump back to the previous tab")
                                             },
                                             "jump-forward": {
                                                 group: "tabs",
                                                 title: qsTr("Jump forward again",
                                                             "along the tabs, after jumping back")
                                             },
                                             "select-tab": {
                                                 group: "tabs",
                                                 title: qsTr("Jump to tab by number")
                                             },
                                             "pin-tab": {
                                                 group: "tabs",
                                                 title: qsTr("Pin or unpin this tab"),
                                                 requires: "unpaired-tab"
                                             },
                                             "keep-tab-active": {
                                                 group: "tabs",
                                                 title: qsTr("Keep this Pinned tab active"),
                                                 requires: "pinned-tab"
                                             },
                                             "extension-popup": {
                                                 group: "interface",
                                                 title: qsTr("Show the extensions"),
                                                 requires: "extension"
                                             },
                                             "glance-to-tab": {
                                                 group: "tabs",
                                                 title: qsTr("Open the Glance as a tab"),
                                                 requires: "glance"
                                             },
                                             "duplicate-tab": {
                                                 group: "tabs",
                                                 title: qsTr("Duplicate tab"),
                                                 requires: "page"
                                             },
                                             "move-tab-up": {
                                                 group: "tabs",
                                                 title: qsTr("Move tab up")
                                             },
                                             "move-tab-down": {
                                                 group: "tabs",
                                                 title: qsTr("Move tab down")
                                             },
                                             "close-other-tabs": {
                                                 group: "tabs",
                                                 title: qsTr("Close other tabs"),
                                                 requires: "ordinary-tab"
                                             },
                                             "close-tabs-below": {
                                                 group: "tabs",
                                                 title: qsTr("Close tabs below"),
                                                 requires: "ordinary-tab"
                                             },
                                             "tab-menu": {
                                                 group: "tabs",
                                                 title: qsTr("Open tab menu")
                                             },
                                             "move-tab": {
                                                 group: "tabs",
                                                 title: qsTr("Move tab to another Space")
                                             },
                                             "add-split": {
                                                 group: "tabs",
                                                 title: qsTr("Add split view"),
                                                 requires: "unpaired-ordinary-tab"
                                             },
                                             "separate-split": {
                                                 group: "tabs",
                                                 title: qsTr("Separate split view"),
                                                 requires: "split"
                                             },
                                             "focus-split-partner": {
                                                 group: "tabs",
                                                 title: qsTr("Focus the tab beside"),
                                                 requires: "split"
                                             },
                                             "pop-out-tab": {
                                                 group: "tabs",
                                                 title: qsTr("Pop the tab out into a window"),
                                                 requires: "tab-to-pop-out"
                                             },
                                             "put-back-tab": {
                                                 group: "tabs",
                                                 title: qsTr("Put the tab back in the sidebar"),
                                                 requires: "tab-window"
                                             },
                                             "toggle-tab-window-strip": {
                                                 group: "tabs",
                                                 title: qsTr("Hide or show the Tab window's strip"),
                                                 requires: "tab-window"
                                             },
                                             "next-space": {
                                                 group: "spaces",
                                                 title: qsTr("Next Space")
                                             },
                                             "select-space": {
                                                 group: "spaces",
                                                 title: qsTr("Switch Space by number")
                                             },
                                             "new-space": {
                                                 group: "spaces",
                                                 title: qsTr("New Space")
                                             },
                                             "take-over-space": {
                                                 group: "spaces",
                                                 title: qsTr("Take over Space"),
                                                 requires: "agent-space"
                                             },
                                             "toggle-sidebar": {
                                                 group: "interface",
                                                 title: qsTr("Hide or show the sidebar")
                                             },
                                             "widen-sidebar": {
                                                 group: "interface",
                                                 title: qsTr("Widen the sidebar")
                                             },
                                             "narrow-sidebar": {
                                                 group: "interface",
                                                 title: qsTr("Narrow the sidebar")
                                             },
                                             "reset-sidebar": {
                                                 group: "interface",
                                                 title: qsTr("Reset the sidebar width")
                                             },
                                             "focus-sidebar": {
                                                 group: "interface",
                                                 title: qsTr("Focus the sidebar")
                                             },
                                             "focus-page": {
                                                 group: "interface",
                                                 title: qsTr("Focus the page")
                                             },
                                             "move-focus-left": {
                                                 group: "interface",
                                                 title: qsTr("Move focus left")
                                             },
                                             "move-focus-down": {
                                                 group: "interface",
                                                 title: qsTr("Move focus down")
                                             },
                                             "move-focus-up": {
                                                 group: "interface",
                                                 title: qsTr("Move focus up")
                                             },
                                             "move-focus-right": {
                                                 group: "interface",
                                                 title: qsTr("Move focus right")
                                             },
                                             "copy-address": {
                                                 group: "navigation",
                                                 title: qsTr("Copy address"),
                                                 requires: "page"
                                             },
                                             "site-information": {
                                                 group: "page",
                                                 title: qsTr("Site information")
                                             },
                                             "find": {
                                                 group: "page",
                                                 title: qsTr("Find in page"),
                                                 requires: "find"
                                             },
                                             "find-next": {
                                                 group: "page",
                                                 title: qsTr("Next match"),
                                                 requires: "find"
                                             },
                                             "find-previous": {
                                                 group: "page",
                                                 title: qsTr("Previous match"),
                                                 requires: "find"
                                             },
                                             "zoom-in": {
                                                 group: "page",
                                                 title: qsTr("Zoom in"),
                                                 requires: "zoom"
                                             },
                                             "zoom-out": {
                                                 group: "page",
                                                 title: qsTr("Zoom out"),
                                                 requires: "zoom"
                                             },
                                             "zoom-reset": {
                                                 group: "page",
                                                 title: qsTr("Reset zoom"),
                                                 requires: "zoom"
                                             },
                                             "print": {
                                                 group: "page",
                                                 title: qsTr("Print"),
                                                 requires: "printing"
                                             },
                                             "screenshot-page": {
                                                 group: "page",
                                                 title: qsTr("Screenshot page")
                                             },
                                             "copy-screenshot": {
                                                 group: "page",
                                                 title: qsTr("Copy screenshot")
                                             },
                                             "screenshot-full-page": {
                                                 group: "page",
                                                 title: qsTr("Screenshot full page")
                                             },
                                             "copy-full-page-screenshot": {
                                                 group: "page",
                                                 title: qsTr("Copy full-page screenshot")
                                             },
                                             "fullscreen": {
                                                 group: "interface",
                                                 title: qsTr("Fullscreen")
                                             },
                                             "developer-tools": {
                                                 group: "developer",
                                                 title: qsTr("Developer tools"),
                                                 requires: "inspector"
                                             },
                                             "inspect-element": {
                                                 group: "developer",
                                                 title: qsTr("Inspect element"),
                                                 requires: "inspector"
                                             },
                                             "open-page-context-menu": {
                                                 group: "page",
                                                 title: qsTr("Open page context menu"),
                                                 requires: "page"
                                             },
                                             "open-file": {
                                                 group: "navigation",
                                                 title: qsTr("Open file")
                                             },
                                             "shortcuts": {
                                                 group: "interface",
                                                 title: qsTr("Keyboard shortcuts")
                                             },
                                             "history": {
                                                 group: "interface",
                                                 title: qsTr("History"),
                                                 requires: "ordinary-window"
                                             },
                                             "agent-activity": {
                                                 group: "interface",
                                                 title: qsTr("Show agent activity"),
                                                 requires: "ordinary-window"
                                             },
                                             // `line` takes the rest of what was typed after
                                             // the command's name as its argument.
                                             "ask": {
                                                 group: "page",
                                                 title: qsTr("Ask your agent about this tab"),
                                                 requires: "ordinary-window",
                                                 line: true
                                             },
                                             "settings": {
                                                 group: "interface",
                                                 title: qsTr("Settings and downloads")
                                             },
                                             "downloads": {
                                                 group: "interface",
                                                 title: qsTr("Downloads")
                                             },
                                             "private-window": {
                                                 group: "interface",
                                                 title: qsTr("New Private window"),
                                                 requires: "private-windows"
                                             },
                                             "minimize-window": {
                                                 group: "interface",
                                                 title: qsTr("Minimize window")
                                             }
                                         })

    // The Material Symbol each group's rows lead with in the Omnibar: one per
    // group rather than one per command, so the picture says where a command
    // belongs and the title says what it does.
    readonly property var groupSymbols: ({
                                             "navigation": "explore",
                                             "tabs": "tab",
                                             "page": "article",
                                             "spaces": "workspaces",
                                             "interface": "web_asset",
                                             "developer": "code"
                                         })

    readonly property var pageDescriptions: ({
                                                 "scroll-down": qsTr("Scroll down"),
                                                 "scroll-up": qsTr("Scroll up"),
                                                 "scroll-half-page-down": qsTr(
                                                                              "Scroll down half a page"),
                                                 "scroll-half-page-up": qsTr(
                                                                            "Scroll up half a page"),
                                                 "scroll-top": qsTr("Top of page"),
                                                 "scroll-bottom": qsTr("Bottom of page"),
                                                 "open-link": qsTr("Follow link hint"),
                                                 "open-link-background": qsTr(
                                                                             "Follow link hint in a background tab")
                                             })

    // A command the current engine or window cannot carry out is listed and
    // unavailable rather than missing: the reader learns it exists, and why it
    // is not on offer here.
    function available(command) {
        // A Private window's browser is destroyed before the window is, and a
        // window with no browser left has nothing on offer.
        if (!browser)
            return false;
        const description = descriptions[command];
        switch (description ? description.requires : "") {
        case "page":
            return !browser.activeTabBlank;
        case "extension":
            // The window answers for what it hosts. A profile host that keeps
            // no extensions at all, which is every window of an engine that
            // cannot host one, is none hosted rather than a missing condition.
            return window.hostedExtensions.length > 0;
        case "glance":
            // An extension's popup is drawn as a Glance but is not a page to
            // keep, so the command that keeps one is not on offer for it.
            return window.glanceOpen && !window.glanceIsExtension;
        case "find":
            return window.findAvailable;
        case "zoom":
            return window.zoomAvailable;
        case "printing":
            return window.printingAvailable;
        case "inspector":
            return window.developerToolsAvailable;
        case "private-windows":
            return windowManager.privateWindowsAvailable;
        case "ordinary-window":
            return !window.privateWindow;
            // Keep active is a Pinned tab's setting, and the rows below a tab are
            // the ordinary list's.
        case "pinned-tab":
            return browser.activeTabPinned && !window.privateWindow;
        case "ordinary-tab":
            return !browser.activeTabPinned;
            // A split takes ordinary tabs that are in none, and parts only
            // where there is one; a tab in a split is separated before it is
            // pinned.
        case "unpaired-tab":
            return !browser.splitOnShow;
        case "unpaired-ordinary-tab":
            return !browser.activeTabPinned && !browser.splitOnShow;
        case "split":
            return browser.splitOnShow;
            // A Tab window shows a page, and a Glance is kept as a tab first.
            // A Private window has none.
        case "tab-to-pop-out":
            return !window.privateWindow && browser.activeTabCanPopOut && !window.glanceOpen;
        case "tab-window":
            return !window.privateWindow && window.tabWindowCount > 0;
        case "agent-space":
            return window.agentSpaceOnShow;
        }
        return true;
    }

    // What `omaweb commands` and `omaweb run` ask of the ordinary window over
    // the Agent socket. The core has refused every command outside
    // `request.commands`, the public ones, and this answers the rest from
    // `available`, as the command scope does.
    function answerAgent(request) {
        const allowed = request.commands || [];
        if (request.verb === "commands") {
            const list = [];
            for (let index = 0; index < allowed.length; ++index) {
                const command = allowed[index];
                const description = descriptions[command];
                if (description && root.available(command)) {
                    list.push({
                                  command: command,
                                  title: description.title,
                                  group: description.group
                              });
                }
            }
            return {
                ok: true,
                commands: list
            };
        }
        const command = String(request.command);
        if (allowed.indexOf(command) < 0 || !descriptions[command]) {
            return {
                ok: false,
                code: "refused",
                error: "Omaweb has no command \"" + command + "\" to run from outside its window."
            };
        }
        // A command given a tab is about that tab rather than the one on
        // show, and whether it ran says whether it could.
        if (typeof request.argument !== "string" && !root.available(command)) {
            return {
                ok: false,
                code: "unavailable",
                error: "\"" + command + "\" is not available now."
            };
        }
        if (!root.run(command, request.argument)) {
            return {
                ok: false,
                code: "failed",
                error: "\"" + command + "\" did not run."
            };
        }
        return {
            ok: true
        };
    }

    function actions() {
        const list = [];

        for (const command in descriptions) {
            const description = descriptions[command];
            if (window.privateWindow && (command === "pin-tab" || command === "move-tab" || command
                                         === "keep-tab-active" || command === "select-space"
                                         || command === "next-space" || command === "new-space"
                                         || command === "take-over-space" || command === "ask"
                                         || command === "pop-out-tab" || command === "put-back-tab"
                                         || command === "toggle-tab-window-strip")) {
                continue;
            }
            list.push({
                          group: description.group,
                          title: description.title,
                          keys: keymap.keysFor(command),
                          enabled: root.available(command),
                          command: command,
                          argument: -1
                      });
        }

        for (const pageCommand in pageDescriptions) {
            list.push({
                          group: "page",
                          title: pageDescriptions[pageCommand],
                          keys: keymap.pageKeysFor(pageCommand),
                          enabled: keymap.pageCommandsEnabled,
                          command: "",
                          argument: -1
                      });
        }

        return list;
    }

    // Where the Omnibar can switch to without opening anything: every open
    // tab but the one on show, then every other Space's tabs, then every Space
    // but the active one. A Private window has no Spaces to offer. None of
    // these rows carries its number keys: the sidebar and the Shortcut sheet
    // teach those.
    function destinations(awayTabs) {
        const list = [];
        const tabs = browser.tabs;
        for (let row = 0; row < tabs.rowCount(); ++row) {
            const index = tabs.index(row, 0);
            // The active tab and, in a split, the tab beside it are on show
            // already.
            if (tabs.data(index, Qt.UserRole + 6) || tabs.data(index, Qt.UserRole + 15)) {
                continue;
            }
            list.push({
                          kind: "tab",
                          title: tabs.data(index, Qt.UserRole + 4),
                          url: String(tabs.data(index, Qt.UserRole + 3)),
                          icon: String(tabs.data(index, Qt.UserRole + 8)),
                          enabled: true,
                          command: "activate-tab",
                          argument: tabs.data(index, Qt.UserRole + 1)
                      });
        }

        if (!window.privateWindow) {
            // What the session keeps of each other Space, so listing them
            // wakes none of their pages.
            for (let row = 0; row < awayTabs.length; ++row) {
                list.push({
                              kind: "tab",
                              title: awayTabs[row].title,
                              url: String(awayTabs[row].url),
                              icon: String(awayTabs[row].iconUrl),
                              spaceId: awayTabs[row].spaceId,
                              spaceName: awayTabs[row].spaceName,
                              spaceColor: awayTabs[row].spaceColor,
                              enabled: true,
                              command: "activate-tab",
                              argument: awayTabs[row].tabId
                          });
            }

            const spaces = browser.spaces;
            for (let row = 0; row < spaces.rowCount(); ++row) {
                const index = spaces.index(row, 0);
                if (spaces.data(index, Qt.UserRole + 4)) {
                    continue;
                }
                list.push({
                              kind: "space",
                              title: spaces.data(index, Qt.UserRole + 2),
                              color: spaces.data(index, Qt.UserRole + 3),
                              enabled: true,
                              command: "switch-space",
                              argument: spaces.data(index, Qt.UserRole + 1)
                          });
            }
        }
        return list;
    }

    // The icon each open tab shows, by host, for rows that name a site
    // without being its tab. It reads only the window's own tabs, so a row
    // never shows artwork another Space or window has.
    function siteIcons() {
        const icons = {};
        const tabs = browser.tabs;
        for (let row = 0; row < tabs.rowCount(); ++row) {
            const index = tabs.index(row, 0);
            const icon = String(tabs.data(index, Qt.UserRole + 8) || "");
            const site = host(String(tabs.data(index, Qt.UserRole + 3)));
            // A tab whose page has not reported an icon carries its stored
            // one, which is the row's own fallback rather than a live icon.
            const stored = icon.startsWith("image://omaweb-favicon/");
            if (icon.length > 0 && !stored && site.length > 0 && icons[site] === undefined)
                icons[site] = icon;
        }
        return icons;
    }

    function invoke(action) {
        if (action.command === "activate-tab") {
            if (action.spaceId)
                browser.activateTabInSpace(action.spaceId, action.argument);
            else
                browser.activateTab(action.argument);
            return;
        }
        if (action.command === "switch-space") {
            browser.switchSpace(action.argument);
            return;
        }
        run(action.command, action.argument);
    }

    function score(text, query) {
        if (query.length === 0) {
            return 1;
        }
        const haystack = text.toLowerCase();
        const needle = query.toLowerCase();
        let cursor = 0;
        let points = 0;
        let previous = -2;
        for (let index = 0; index < needle.length; ++index) {
            const found = haystack.indexOf(needle.charAt(index), cursor);
            if (found === -1) {
                return 0;
            }
            points += 1;
            if (found === previous + 1) {
                points += 3;
            }
            if (found === 0 || haystack.charAt(found - 1) === " ") {
                points += 2;
            }
            previous = found;
            cursor = found + 1;
        }
        return points;
    }

    // How strongly a row holds the typed text, for the Omnibar: 3 where one of
    // its fields starts with it, 2 where a word inside one does, 1 where it
    // appears anywhere, 0 where it does not. Stricter than `score`, because
    // letters scattered across every tab, Space and command would bury the
    // row the reader meant.
    function tier(fields, query) {
        const needle = query.toLowerCase();
        let best = 0;
        for (let field = 0; field < fields.length; ++field) {
            const haystack = String(fields[field]).toLowerCase();
            let at = haystack.indexOf(needle);
            while (at >= 0) {
                if (at === 0) {
                    return 3;
                }
                best = Math.max(best, /[a-z0-9]/.test(haystack.charAt(at - 1)) ? 1 : 2);
                at = haystack.indexOf(needle, at + 1);
            }
        }
        return best;
    }

    // The host an address names, without the `www.` a reader never types.
    function host(address) {
        const found = /^[a-z][a-z0-9+.-]*:\/\/([^/?#:]+)/i.exec(address);
        return found === null ? "" : found[1].replace(/^www\./i, "");
    }

    function highlight(text, query) {
        if (query.length === 0) {
            return text;
        }
        const haystack = text.toLowerCase();
        const needle = query.toLowerCase();
        let cursor = 0;
        let out = "";
        for (let index = 0; index < needle.length; ++index) {
            const found = haystack.indexOf(needle.charAt(index), cursor);
            if (found === -1) {
                return text;
            }
            out += text.substring(cursor, found) + "<b>" + text.charAt(found) + "</b>";
            cursor = found + 1;
        }
        return out + text.substring(cursor);
    }

    // A command that takes the rest of the line is named by its first word,
    // and what follows the name is its argument rather than letters to look
    // for: `ask summarize this` is `ask` with `summarize this`. The name is
    // read as the other rows' letters are, whatever their case.
    function lineCommand(text) {
        const found = /^\s*([A-Za-z-]+)(?:\s+([\s\S]*))?$/.exec(text);
        const command = found === null ? "" : found[1].toLowerCase();
        if (!descriptions[command] || !descriptions[command].line)
            return null;
        return {
            command: command,
            words: found[2] === undefined ? "" : found[2]
        };
    }

    function search(query) {
        const all = actions();
        const line = lineCommand(query);
        for (let index = 0; line !== null && index < all.length; ++index) {
            if (all[index].command === line.command)
                return [Object.assign({}, all[index], {
                                          argument: line.words
                                      })];
        }
        const matched = [];
        for (let index = 0; index < all.length; ++index) {
            const points = score(all[index].title, query);
            if (points > 0) {
                matched.push({
                                 action: all[index],
                                 points: points,
                                 order: index
                             });
            }
        }
        matched.sort(function (left, right) {
            return right.points - left.points || left.order - right.order;
        });
        const result = [];
        for (let index = 0; index < matched.length; ++index) {
            result.push(matched[index].action);
        }
        return result;
    }
}
