import QtQuick
import Omaweb

Item {
    id: root
    objectName: "engineLoader"

    required property var browserController
    required property url engineSource
    // The table of Space profiles, which the window owns because it routes what
    // comes out of them. This host asks it for the profile a retained tab's
    // Space runs in, and for any Space a data-clearing command names.
    required property var spaceProfiles
    required property string profilePath
    required property var sharedProfile
    required property var permissionController
    required property var blocker
    required property var engineBlocker
    required property var cookiePolicy
    required property var keyboardManager
    property bool pageFocusAllowed: true
    property var hintTheme: ({})
    // The palette the engine draws its inspector in, so a docked inspector is
    // the same window as the chrome around it rather than the engine's own idea
    // of a colour scheme.
    property var developerToolsColors: ({})
    // The window's palette, for the divider between a split's panes.
    property var colors: null
    // The two colours the engine keeps drawing with: a box inside the page that
    // scrolls keeps the engine's bar, and only its colour is Omaweb's to say.
    readonly property color pageScrollbarThumb: root.colors ? root.colors.mutedText : "transparent"
    readonly property color pageScrollbarTrack: root.colors ? root.colors.surface : "transparent"
    property color pageBackgroundColor: "#16151d"
    // The accent a page's own controls are drawn in, which differs per window:
    // a Private window's chrome carries its own, and so should the controls on
    // its pages.
    property color pageControlAccent: "transparent"
    // The palette a page that asks for it is drawn in: the roles a page with
    // one ground spends, under the names the website's own tokens use. The
    // opaque grounds, since what a page draws is flat, and the muted text as
    // the theme named it rather than as Omaweb floored it for its own six
    // grounds, on one of which the floor lands near white.
    readonly property var pagePalette: root.colors ? {
                                                         "bg": root.colors.windowOpaque,
                                                         "sidebar": root.colors.sidebarOpaque,
                                                         "fg": root.colors.text,
                                                         "accent": root.colors.accent,
                                                         "urgent": root.colors.urgent,
                                                         "muted": root.colors.pageMutedText
                                                     } : null
    property string spaceId: ""
    // The Space the window's own profile belongs to, which is not the Space on
    // show in a Private window: that session is shared and has no Space of its
    // own. The core answers it, so the rule is not written out again here.
    readonly property string sessionSpaceId: root.browserController
                                             ? root.browserController.sessionSpaceId : ""

    readonly property alias item: root.activeEngine
    // The inspector of the tab the core says is being inspected, for the dock
    // to take as a child. Null whenever nothing is attached.
    property var developerToolsView: null
    readonly property string inspectedTabId: root.browserController
                                             ? root.browserController.developerToolsTabId : ""
    readonly property bool hintModeActive: root.activeEngine
                                           ? root.activeEngine.keyboardNavigationHintModeActive :
                                             false
    property var activeEngine: null
    // The engine of the tab beside, while a split is on show. It is drawn and
    // runs, and that is all: every command and answer above is the active
    // engine's.
    property var besideEngine: null
    // Whether the chrome moves at all, which is the reader's one switch.
    property bool ease: true
    // How far from its place the page on show stands while it arrives, in
    // pixels right and down. Measured by the sidebar, where the rows are; applied here
    // to the arriving engine and to nothing else, so the page it replaces and
    // the ground under both stay where they are.
    property real tabNudgeX: 0
    property real tabNudgeY: 0
    // The tabs whose engines came on screen in the latest turn, which are the
    // ones the nudge belongs to: a page already on show when a pane arrives
    // beside it stays where it is. A focus move between the halves shows
    // nothing new, and the sidebar starts no nudge for it.
    property var arrivals: []
    property bool arrivalsOpen: false
    function noteArrival(tabId) {
        if (!root.arrivalsOpen) {
            root.arrivals = [];
            root.arrivalsOpen = true;
            Qt.callLater(function () {
                root.arrivalsOpen = false;
            });
        }
        root.arrivals = root.arrivals.concat([tabId]);
    }
    Component {
        id: tabSlideComponent
        Translate {
            property var engine: null
            property string tabId: ""
            readonly property bool arriving: engine !== null && engine.visible
                                             && root.arrivals.indexOf(tabId) >= 0
            // The leaving pane's own travel, run by `departure` below.
            property real departureX: 0
            x: (arriving ? root.tabNudgeX : 0) + departureX
            y: arriving ? root.tabNudgeY : 0
        }
    }
    // A pane that leaves goes back the way it came, the arrival reversed and
    // quicker: to the side its row is on, and hidden once it has gone. One
    // pane leaves at a time; a second departure finishes the first.
    NumberAnimation {
        id: departure
        property string tabId: ""
        property: "departureX"
        duration: 120
        easing.type: Easing.InCubic
        onFinished: root.settleDeparture()
    }
    // The pane keeps the place and size it had for the length of the move,
    // over the page that has already taken the whole width, and is laid out
    // by the panes again once it is hidden.
    function departEngine(tabId, pane, direction) {
        const engine = root.engines[tabId];
        if (!engine || !root.ease || !engine.visible || direction === 0) {
            root.setEngineVisible(tabId, false);
            return;
        }
        if (departure.running) {
            departure.stop();
            root.settleDeparture();
        }
        engine.x = pane.x;
        engine.width = pane.width;
        engine.z = 2;
        departure.tabId = tabId;
        departure.target = engine.transform[0];
        departure.from = 0;
        departure.to = 10 * direction;
        departure.start();
    }
    function settleDeparture() {
        const tabId = departure.tabId;
        departure.tabId = "";
        const engine = root.engines[tabId];
        if (!engine)
            return;
        engine.transform[0].departureX = 0;
        engine.z = 0;
        root.bindPane(tabId, engine);
        // Hidden only if nothing has shown it again on the way out.
        if (!root.shownTabIds[tabId])
            root.setEngineVisible(tabId, false);
    }
    // Which tabs the rows say are on show, kept here so a departure can tell
    // a pane that has left from one that was called back.
    readonly property var shownTabIds: ({})
    property bool suspended: true

    // The split on show, as the core answers it. The page area lays the two
    // panes out left and right of one divider; everything else about the
    // window keeps reading the active tab.
    readonly property string splitLeftTabId: root.browserController
                                             ? root.browserController.splitLeftTabId : ""
    readonly property string splitRightTabId: root.browserController
                                              ? root.browserController.splitRightTabId : ""
    readonly property string tabBesideId: root.browserController
                                          ? root.browserController.tabBesideId : ""
    readonly property bool splitOnShow: root.splitLeftTabId.length > 0
    // Where the divider stands, as the left pane's share of the width. Held
    // per split for as long as the window lives, and never written down: a
    // restart puts every divider back in the middle.
    property real dividerFraction: 0.5
    readonly property var dividerFractions: ({})
    readonly property real paneMinimumWidth: 120
    readonly property real leftPaneWidth: Math.round(root.width * root.dividerFraction)
    readonly property real dividerWidth: 1
    onSplitLeftTabIdChanged: root.restoreDivider()
    onSplitRightTabIdChanged: root.restoreDivider()
    function splitKey() {
        return root.splitLeftTabId + "|" + root.splitRightTabId;
    }
    function restoreDivider() {
        if (!root.splitOnShow)
            return;
        const kept = root.dividerFractions[root.splitKey()];
        root.dividerFraction = kept !== undefined ? kept : 0.5;
    }
    function setLeftPaneWidth(width) {
        if (!root.splitOnShow || root.width <= 0)
            return;
        const clamped = Math.max(root.paneMinimumWidth, Math.min(root.width - root.paneMinimumWidth,
                                                                 width));
        root.dividerFraction = clamped / root.width;
        root.dividerFractions[root.splitKey()] = root.dividerFraction;
    }
    // Which pane a tab's engine is drawn in. A page that has the screen takes
    // the whole host whichever pane it sits in, and a tab in no split takes
    // it as before.
    function paneX(tabId, engine) {
        if (!root.splitOnShow || engine.siteFullscreenActive || tabId !== root.splitRightTabId)
            return 0;
        return root.leftPaneWidth + root.dividerWidth;
    }
    function paneWidth(tabId, engine) {
        if (!root.splitOnShow || engine.siteFullscreenActive)
            return root.width;
        if (tabId === root.splitLeftTabId)
            return root.leftPaneWidth;
        if (tabId === root.splitRightTabId)
            return root.width - root.leftPaneWidth - root.dividerWidth;
        return root.width;
    }
    readonly property real activePaneX: root.splitOnShow && root.tabBesideId
                                        === root.splitLeftTabId ? root.leftPaneWidth
                                                                  + root.dividerWidth : 0
    readonly property real activePaneWidth: !root.splitOnShow ? root.width : (root.tabBesideId
                                                                              === root.splitLeftTabId
                                                                              ? root.width
                                                                                - root.leftPaneWidth
                                                                                - root.dividerWidth :
                                                                                root.leftPaneWidth)
    readonly property real besidePaneX: root.tabBesideId === root.splitLeftTabId ? 0 :
                                                                                   root.leftPaneWidth
                                                                                   + root.dividerWidth
    readonly property real besidePaneWidth: !root.splitOnShow ? 0 : root.width
                                                                - root.activePaneWidth
                                                                - root.dividerWidth
    // The same two panes as they stand, left and right rather than focused and
    // beside, which is how the keyboard crosses them.
    readonly property real rightPaneX: root.leftPaneWidth + root.dividerWidth
    readonly property real rightPaneWidth: root.width - root.leftPaneWidth - root.dividerWidth

    // What the page the reader is looking at is doing with the whole screen,
    // read off the engine that draws it rather than kept beside it. Either
    // pane of a split may take the window.
    readonly property var fullscreenEngine: root.activeEngine
                                            && root.activeEngine.siteFullscreenActive
                                            ? root.activeEngine : (root.besideEngine
                                                                   && root.besideEngine.siteFullscreenActive
                                                                   ? root.besideEngine : null)
    // The origin stands before the flag: bindings answer a change in the
    // order they are declared, and the window reads the origin from the
    // flag's change handler.
    readonly property string siteFullscreenOrigin: root.fullscreenEngine
                                                   ? root.fullscreenEngine.siteFullscreenOrigin : ""
    readonly property bool siteFullscreenActive: root.fullscreenEngine !== null

    signal printFinished(string destination, bool succeeded)
    signal pageCaptured(string destination, bool succeeded, string reason)
    signal auxiliaryWindowRequested(var engine, var request, url requestedUrl)
    signal newTabRequested(var engine, var request, url requestedUrl)
    signal backgroundTabRequested(url requestedUrl)
    signal sitePermissionRequested(var engine, string requestId, string origin, string permission)
    signal certificateErrorRaised(var engine, string requestId, var failure)
    signal pageSiteDataCleared(string origin, var cleared, string error)
    signal pageContextRequested(var engine, var context)
    signal pageTooltipRequested(var engine, var tooltip)
    signal browserPromptRequested(var engine, string requestId, var prompt)
    signal fileSelectionRequested(var engine, string requestId, var selection)

    function keyboardConfiguration(url) {
        const configuration = Object.assign({}, root.keyboardManager.configurationForUrl(url));
        configuration.hintTheme = root.hintTheme;
        return configuration;
    }

    // Engines belong to the host, not to the tab row that shows them.
    // Switching Space replaces the whole tab model, so a delegate-owned engine
    // would be torn down and every page reloaded on the way back. These outlive
    // their delegate and are handed back when the Space returns.
    readonly property var engines: ({})
    // The Space each engine was opened in, so deleting a Space takes its pages
    // with it.
    readonly property var engineSpaces: ({})
    // Set while a Space is being put away: a delegate disappearing then means
    // the Space changed, not that the user closed the tab.
    property bool preservingEngines: false
    property var engineComponent: null

    // A page that opens a window completes the request against the engine of
    // the tab that window becomes. That tab starts blank and a blank tab is
    // given no engine, so it is named here: naming it builds the engine, and
    // the request then reaches that tab rather than whichever page was showing
    // when the window was asked for.
    property string adoptingTabId: ""

    // A tab with no address to load. `about:blank` is what a tab holds before
    // the reader commits a destination, what the last tab in a Space falls back
    // to, and what a page's new window starts as, so it is the one address that
    // is never worth an engine.
    function blankAddress(url) {
        const value = String(url);
        return value.length === 0 || value === "about:blank";
    }

    function adoptNewWindowRequest(tabId, request) {
        root.adoptingTabId = tabId;
        const engine = root.engines[tabId];
        if (engine) {
            root.adoptingTabId = "";
            engine.acceptNewWindowRequest(request);
            return;
        }
        Qt.callLater(function () {
            const late = root.engines[tabId];
            if (!late)
                return;
            root.adoptingTabId = "";
            late.acceptNewWindowRequest(request);
        });
    }

    function focusPage() {
        if (activeEngine)
            activeEngine.focusPage();
    }

    function requestPageContextMenu() {
        if (activeEngine)
            activeEngine.requestPageContextMenu();
    }

    // The everyday page operations, each one addressed to the engine of the tab
    // on show. A tab with no engine has no page to navigate, find in, print or
    // hand the screen back, so nothing is asked of one. The shell has already
    // said so.
    function goBack() {
        if (root.activeEngine)
            root.activeEngine.goBack();
    }

    function goForward() {
        if (root.activeEngine)
            root.activeEngine.goForward();
    }

    function reloadPage() {
        if (root.activeEngine)
            root.activeEngine.reloadPage();
    }

    function reloadPageBypassingCache() {
        if (root.activeEngine)
            root.activeEngine.reloadPageBypassingCache();
    }

    function stopLoading() {
        if (root.activeEngine)
            root.activeEngine.stopLoading();
    }

    function findText(query, forward) {
        if (root.activeEngine)
            root.activeEngine.findText(query, forward);
    }

    function printPage(destination) {
        if (root.activeEngine)
            root.activeEngine.printPage(destination);
        else
            root.printFinished(destination, false);
    }

    // In a split the active tab is the one captured: it is the one the reader
    // is working in, and the command names no other.
    function capturePage(destination) {
        if (root.activeEngine)
            root.activeEngine.capturePage(destination);
        else
            root.pageCaptured(destination, false, "");
    }

    function capturePageFully(destination) {
        if (root.activeEngine)
            root.activeEngine.capturePageFully(destination);
        else
            root.pageCaptured(destination, false, "");
    }

    function exitSiteFullscreen() {
        if (root.fullscreenEngine)
            root.fullscreenEngine.exitSiteFullscreen();
    }

    // The core owns which tab is inspected; the engines are told about it here.
    // One inspector at a time, so every engine that is not the named one gives
    // its own up — including a tab that had the inspector before this one.
    function syncDeveloperTools() {
        const wanted = root.inspectedTabId;
        for (const tabId in root.engines) {
            const engine = root.engines[tabId];
            if (tabId !== wanted && engine.developerToolsAttached)
                engine.detachDeveloperTools();
        }
        const inspected = wanted.length > 0 ? root.engines[wanted] : null;
        if (inspected && !inspected.developerToolsAttached)
            inspected.attachDeveloperTools();
        root.developerToolsView = inspected ? inspected.developerToolsView : null;
    }

    onInspectedTabIdChanged: {
        root.syncDeveloperTools();
        // The inspector is one of the two reasons a hidden page goes on
        // running, and it moves between tabs: the tab that had it has lost its
        // exemption, and the tab that takes it gains one.
        root.applyEveryPageLifecycle();
    }

    // The tabs the core retains: a Pinned tab the reader marked Keep active, and
    // the tab an inspector is attached to. Which of the two answers depends on
    // where the tab is, because the core holds one Space's tabs at a time:
    // `retainedEngines` was told at the moment its Space was put away, and the
    // core itself answers for the Space on show.
    function retains(tabId) {
        if (retainedEngines.keeps(tabId))
            return true;
        const retained = root.browserController ? root.browserController.retainedTabIds() : [];
        return retained.indexOf(tabId) >= 0;
    }

    // A page the reader cannot see has no reason to go on running. Freezing it
    // keeps the document, the process and everything the page holds, and stops
    // the timers, animations and script behind the tab that replaced it, so
    // selecting the tab continues the page rather than loading it again.
    //
    // Three hidden pages are exceptions. One is being watched through the
    // inspector, which is the whole reason it was kept running; the engine
    // refuses to freeze it in any case. One is being heard, and a page the
    // reader is listening to is not one they have finished with. The third is a
    // Pinned tab marked Keep active, which is the reader asking in as many words
    // for a page to go on running while they are looking at something else: a
    // stopped page cannot tell them that a message arrived, and this build has
    // no push service to tell them in its place.
    function applyPageLifecycle(tabId) {
        const engine = root.engines[tabId];
        if (!engine)
            return;
        const runsUnwatched = tabId === root.inspectedTabId || engine.pageAudible || root.retains(
                  tabId);
        engine.pageFrozen = !engine.visible && !runsUnwatched;
        // A stopped page answers no key. The desktop hears that it has left
        // rather than being offered controls that reach nothing.
        if (engine.pageFrozen)
            SoundingTabs.forget(tabId);
    }

    function applyEveryPageLifecycle() {
        for (const tabId in root.engines)
            root.applyPageLifecycle(tabId);
    }

    // Putting a page on screen or taking it off is the same act as deciding
    // whether it runs, so the two are written together: nothing hides an engine
    // without answering for what it goes on spending behind whatever replaced
    // it.
    function setEngineVisible(tabId, visible) {
        const engine = root.engines[tabId];
        if (!engine)
            return;
        if (visible && !engine.visible)
            root.noteArrival(tabId);
        engine.visible = visible;
        root.applyPageLifecycle(tabId);
    }

    // Asking to inspect the page is asking for the dock as well, so the core
    // hears about the attachment first and the engine is told which node to
    // select only once the core has accepted it. A tab the core refuses — a
    // blank one, which has no page — must not be handed an inspector the core
    // does not know about and would never take away again.
    function inspectElement() {
        if (!root.activeEngine)
            return;
        root.browserController.openDeveloperTools();
        if (root.inspectedTabId !== root.browserController.activeTabId)
            return;
        root.activeEngine.inspectElement();
    }

    function checkForEditedFormState(callback) {
        if (activeEngine)
            activeEngine.checkForEditedFormState(callback);
        else
            callback(false);
    }

    // Profile-wide data removal is an engine operation, and it reaches Spaces
    // that are not on show — including ones with no profile open, which the
    // table builds on being asked.
    // Returns the categories the engine could not take, so the browser can say
    // what stayed rather than claim everything went.
    function clearBrowsingData(spaceIds, dataTypes, since) {
        const untouched = [];
        for (let index = 0; index < spaceIds.length; ++index) {
            const host = root.spaceProfiles.hostFor(spaceIds[index]);
            if (!host)
                continue;
            const stayed = host.clearBrowsingData(dataTypes, since) || [];
            for (let named = 0; named < stayed.length; ++named) {
                if (untouched.indexOf(stayed[named]) === -1)
                    untouched.push(stayed[named]);
            }
        }
        return untouched;
    }

    // An origin's decisions are the browser's to keep, so the engine's own
    // record of them is dropped when the reader takes them back.
    // The page empties its own storage, which is the only removal that is
    // about one origin rather than a whole Space.
    function clearPageSiteData() {
        if (root.activeEngine)
            root.activeEngine.clearPageSiteData();
    }

    function resetOriginPermissions(spaceId, origin) {
        const host = root.spaceProfiles.hostFor(spaceId);
        if (host)
            host.resetOriginPermissions(origin);
    }

    // Putting a Space away stops its pages rather than taking them. A page the
    // reader cannot see spends nothing once it is frozen, and coming back to a
    // Space is ordinary enough that reloading every page in it is the wrong
    // trade: what the reader left open is what they expect to find.
    //
    // What this costs is a renderer per tab the reader actually opened in that
    // Space, held until the tab or the Space is closed. Tabs of a restored
    // Space that were never selected have no engine to keep, so a Space the
    // reader has read three pages in holds three.
    //
    // The tabs the core names are still retained, which is a different
    // question: retention is what keeps a page identified, listed with its
    // cost, and started again in a Space that has never been selected.
    function suspend(spaceId, retainedTabIds) {
        preservingEngines = true;
        suspended = true;
        activeEngine = null;
        const retained = retainedTabIds || [];
        for (const tabId in root.engineSpaces) {
            if (root.engineSpaces[tabId] !== spaceId)
                continue;
            if (retained.indexOf(tabId) >= 0)
                retainedEngines.keep(tabId, spaceId);
            // Retention decided that a page is identified and answered for, not
            // what it runs at: every page of a Space that is not on show
            // freezes, and the ones being watched or heard are exempt wherever
            // they are.
            root.setEngineVisible(tabId, false);
        }
    }

    function resume() {
        suspended = false;
        preservingEngines = false;
        retainedEngines.releaseVisibleSpace();
        // Retained tabs come back after the visible Space, not with it: the
        // reader is waiting for the page in front of them, and a renderer
        // started for a Space they cannot see must not be in the way of it.
        Qt.callLater(root.restoreRetainedTabs);
    }

    function restoreRetainedTabs() {
        if (root.suspended)
            return;
        retainedEngines.reconcile();
    }

    // A tab of the Space on show takes that Space's profile, which is what the
    // window handed down. A retained tab of another Space names its own: its
    // pages are that Space's browsing identity and nothing else's.
    function createEngine(tabId, tabUrl, spaceId, profilePath, sharedProfile) {
        const engine = root.buildEngine(root, tabUrl, spaceId, profilePath, sharedProfile);
        if (!engine)
            return null;
        root.registerEngine(tabId, engine, spaceId);
        return engine;
    }

    // An engine that answers for no tab: a Glance's page, drawn where the
    // Glance puts it. It is built the way a tab's engine is, on the same
    // profile and with the same configuration, so the page it shows is a
    // page of the Space on show. Nothing here keeps it: whoever asked for it
    // destroys it, or hands it to a tab with `adoptEngine`.
    function createDetachedEngine(parent, tabUrl) {
        return root.buildEngine(parent, tabUrl, undefined, undefined, undefined);
    }

    function buildEngine(parent, tabUrl, spaceId, profilePath, sharedProfile) {
        if (!engineComponent)
            engineComponent = Qt.createComponent(root.engineSource);
        // The extension belongs to the profile, and the profile is there
        // first; but its packages arrive a moment after it, and a document
        // created in that moment runs none of their scripts. So a page asked
        // for while the profile is still waiting on one starts blank and is
        // pointed at its address when the wait ends.
        const host = root.spaceProfiles ? root.spaceProfiles.hostFor(spaceId !== undefined ? spaceId :
                                                                                             root.sessionSpaceId) :
                                          null;
        const held = host && host.extensionLoadsPending > 0 && !root.blankAddress(tabUrl);
        const engine = engineComponent.createObject(parent, {
                                                        "profilePath": profilePath !== undefined
                                                                       ? profilePath :
                                                                         root.profilePath,
                                                        "currentUrl": held ? "" : tabUrl,
                                                        "sharedProfile": sharedProfile
                                                                         !== undefined
                                                                         ? sharedProfile :
                                                                           root.sharedProfile,
                                                        "permissionController":
                                                        root.permissionController,
                                                        "contentBlocker": root.blocker,
                                                        "engineContentBlocker": root.engineBlocker,
                                                        "engineCookiePolicy": root.cookiePolicy,
                                                        // The Space of the profile
                                                        // this view runs on, which is
                                                        // what Content blocking keys
                                                        // its Refusal tally by.
                                                        "spaceId": spaceId !== undefined ? spaceId :
                                                                                           root.sessionSpaceId,
                                                        "keyboardNavigationConfiguration":
                                                        root.keyboardConfiguration(tabUrl),
                                                        "keyboardNavigationScriptSource":
                                                        root.keyboardManager.pageScript,
                                                        "pageBackgroundColor":
                                                        root.pageBackgroundColor,
                                                        "pageControlAccent": root.pageControlAccent,
                                                        "pagePalette": root.pagePalette,
                                                        // What the engine goes on
                                                        // drawing: the bars inside the
                                                        // page, which stay its own.
                                                        "pageScrollbarThumb":
                                                        root.pageScrollbarThumb,
                                                        "pageScrollbarTrack":
                                                        root.pageScrollbarTrack,
                                                        "developerToolsColors":
                                                        root.developerToolsColors,
                                                        "visible": false
                                                    });
        if (held)
            root.releaseWhenExtensionsLoaded(engine, host, tabUrl);
        root.giveScrollbar(engine);
        return engine;
    }

    function releaseWhenExtensionsLoaded(engine, host, tabUrl) {
        let alive = true;
        const release = function () {
            if (host.extensionLoadsPending > 0)
                return;
            host.extensionLoadsPendingChanged.disconnect(release);
            if (alive && String(engine.currentUrl).length === 0)
                engine.currentUrl = tabUrl;
        };
        engine.Component.destruction.connect(function () {
            alive = false;
            host.extensionLoadsPendingChanged.disconnect(release);
        });
        host.extensionLoadsPendingChanged.connect(release);
    }

    // The bar the page scrolls in, drawn by the chrome rather than the engine.
    //
    // Parented into the engine, so it travels with the page wherever the page
    // is put — a pane of a split, a Glance over the tab it came from, a window
    // of its own — and is taken away when the engine is. That is also what
    // keeps the engine adapter free of chrome: the adapter reports where the
    // page stands and hides the engine's own bar; what is drawn for it is the
    // shell's, and is the same component the sidebar and Settings scroll in.
    Component {
        id: pageScrollbarComponent

        ChromeScrollBar {
            id: pageBar
            objectName: "pageScrollBar"

            required property var engine

            view: engine
            colors: root.colors
            // Derived here rather than asked of the adapter: the three numbers
            // it reports already say it, and a fourth property would be one
            // more thing every engine has to answer for to say nothing new.
            visible: engine.pageScrollLength > engine.pageViewportLength + 1
            size: engine.pageScrollLength > 0 ? engine.pageViewportLength / engine.pageScrollLength :
                                                1
            position: pageBar.pageRange > 0 ? engine.pageScrollOffset / pageBar.pageRange * (1
                                                                                             - pageBar.size) :
                                              0

            readonly property real pageRange: Math.max(engine.pageScrollLength
                                                       - engine.pageViewportLength, 0)

            // The page is driven only while the reader is driving the bar.
            // `position` is bound to what the page reported, and a drag writes
            // over that binding; asking the page to scroll whenever it changed
            // would send the page's own scrolling straight back to it.
            onPositionChanged: {
                if (!pageBar.pressed || pageBar.pageRange <= 0)
                    return;
                const travel = Math.max(1 - pageBar.size, 0.0001);
                engine.scrollPageTo(Math.max(0, Math.min(1, pageBar.position / travel))
                                    * pageBar.pageRange);

            }
        }
    }

    function giveScrollbar(engine) {
        if (!engine || !root.colors)
            return;
        pageScrollbarComponent.createObject(engine, {
                                                "engine": engine
                                            });
    }

    function bindPane(tabId, engine) {
        engine.x = Qt.binding(function () {
            return root.paneX(tabId, engine);
        });
        engine.width = Qt.binding(function () {
            return root.paneWidth(tabId, engine);
        });
    }

    // The engine becomes the named tab's: drawn in the host, keyed to the tab,
    // and from here on shown, hidden and taken away with it.
    function registerEngine(tabId, engine, spaceId) {
        engine.parent = root;
        engine.anchors.fill = undefined;
        engine.anchors.top = root.top;
        engine.anchors.bottom = root.bottom;
        root.bindPane(tabId, engine);
        engine.transform = [tabSlideComponent.createObject(engine, {
                                                               "engine": engine,
                                                               "tabId": tabId
                                                           })];
        root.engines[tabId] = engine;
        root.engineSpaces[tabId] = spaceId !== undefined ? spaceId : root.spaceId;
        // A tab can be named as the inspected one before it has an engine to
        // attach to — a Space coming back, or the tab being selected for the
        // first time — so the attachment is made as soon as there is one.
        if (tabId === root.inspectedTabId)
            root.syncDeveloperTools();
    }

    // A detached engine becomes a tab's, page and all: what the Glance was
    // showing, with its history, scroll and form state, rather than a fresh
    // load of the same address. The tab starts blank, so it is named for
    // adoption the way a new-window request's tab is, and finds the engine
    // already keyed to it rather than building one. The engine's page state is
    // reported once by hand: the tab reads the address off reports, and an
    // engine that is not navigating has none to make.
    function adoptEngine(tabId, engine) {
        engine.visible = false;
        root.registerEngine(tabId, engine, undefined);
        root.adoptingTabId = tabId;
        root.browserController.reportTabPageState(tabId, engine.currentUrl, engine.pageTitle,
                                                  engine.pageIconUrl, engine.loading,
                                                  engine.pageAudible);
        root.adoptingTabId = "";
    }

    // The desktop's media key, for the tab the core says it is for. A window
    // that does not hold that tab leaves it to the one that does.
    function invokeMediaAction(tabId, command) {
        const engine = root.engines[tabId];
        if (engine)
            engine.invokeMediaAction(command);
    }

    function discardEngine(tabId) {
        const engine = root.engines[tabId];
        if (!engine)
            return;
        // The page is going. Nothing left to play, and nothing left for a
        // media key to reach.
        SoundingTabs.forget(tabId);
        // The inspector is the engine's to destroy, and nothing else holds it:
        // the dock only borrowed it.
        if (engine.developerToolsAttached) {
            engine.detachDeveloperTools();
            if (tabId === root.inspectedTabId)
                root.developerToolsView = null;
        }
        if (root.activeEngine === engine)
            root.activeEngine = null;
        if (root.besideEngine === engine)
            root.besideEngine = null;
        if (departure.tabId === tabId) {
            departure.stop();
            departure.tabId = "";
        }
        delete root.engines[tabId];
        delete root.engineSpaces[tabId];
        delete root.shownTabIds[tabId];
        engine.destroy();
    }

    function discardEnginesForSpace(spaceId) {
        for (const tabId in root.engineSpaces) {
            if (root.engineSpaces[tabId] !== spaceId)
                continue;
            retainedEngines.forget(tabId);
            discardEngine(tabId);
        }
    }

    // A retained tab keeps its engine when its delegate goes away with the
    // Space, and takes it away for good when the tab itself is closed or the
    // core stops retaining it.
    function keepsEngineFor(tabId) {
        return retainedEngines.keeps(tabId);
    }

    // Which pages outlive their Space is a policy of its own, kept beside the
    // engines rather than inside them: this host builds and destroys engines,
    // and that decides which ones ought to exist.
    property RetainedEngines retainedEngines: RetainedEngines {
        host: root
        browser: root.browserController
        spaceProfiles: root.spaceProfiles
        visibleSpaceId: root.spaceId
    }

    function retainedTabReport() {
        return retainedEngines.report();
    }

    Connections {
        target: root.browserController

        function onRetainedTabsChanged() {
            if (!root.suspended)
                Qt.callLater(root.restoreRetainedTabs);
        }
    }

    Repeater {
        model: root.browserController ? root.browserController.tabs : null

        Item {
            id: tabSlot

            required property string tabId
            required property url tabUrl
            required property bool active
            // On show beside the active tab. The page is drawn and runs, and
            // nothing else here is asked of it: the active tab answers for
            // the window.
            required property bool tabBeside
            required property string splitPartnerId
            // The partner this tab last had, since a separation clears the
            // pairing in the same announcement that hides the pane.
            property string lastPartnerId: ""
            onSplitPartnerIdChanged: if (splitPartnerId.length > 0)
                                         lastPartnerId = splitPartnerId
            readonly property bool shown: active || tabBeside
            // The reader's standing decision about this tab's sound. The
            // engine holds it while the tab has one, and is told again
            // whenever it changes or a new engine takes the tab over.
            required property bool tabMuted
            // Whether this tab's sound is being held back until the reader has
            // dealt with its origin. The core owns it and says so for every tab
            // on that origin at once, so a gesture in one page answers for the
            // next tab showing the same site.
            required property bool tabSoundSuppressed
            // How large this tab's page is drawn. The core owns it and keeps it
            // in the session; the engine is told it whenever it changes and
            // whenever a new engine takes the tab over.
            required property real tabZoom
            // Whether the reader marked this Pinned tab Keep active. The
            // freezing rule reads the core rather than this, but the setting
            // has to be watched from here: taking it away is the moment a tab
            // becomes an ordinary background page and stops.
            required property bool tabKeepActive

            // A restored Space can hold many tabs, and each engine costs a
            // renderer process and a page load. Only a tab the user has
            // actually looked at gets one; the rest keep their saved title and
            // address until they are first selected, or first shown beside.
            property bool everActive: shown
            property var engine: null

            // A blank tab has no page, and the shortcut sheet stands in for it.
            // An engine here would spend a renderer process on an empty
            // document nobody can see, so a tab gets one once it has an address
            // to load — or once it is the tab a page's new-window request has to
            // be handed to.
            //
            // Asked as a function as well as a binding, because a change
            // handler for `tabUrl` cannot trust a binding that depends on
            // `tabUrl` to have been re-evaluated yet: QML does not order a
            // property's change handlers against the bindings that read it.
            //
            // A delegate is built before it is told which tab it stands for,
            // and an unnamed one stands for none: with no tab named for
            // adoption either, it would match the tab being adopted and be
            // given an engine of its own — one keyed to no tab, so no tab ever
            // shows it, hides it or takes it away, left on top of the page the
            // reader came back to.
            function needsEngine() {
                if (tabSlot.tabId.length === 0)
                    return false;
                return !root.blankAddress(tabSlot.tabUrl) || root.adoptingTabId === tabSlot.tabId;
            }

            readonly property bool wantsEngine: tabId.length > 0 && (!root.blankAddress(tabUrl)
                                                                     || root.adoptingTabId
                                                                     === tabId)

            // Whether this engine is on show as the tab beside, as of the last
            // time it was shown or hidden.
            property bool besideSeen: false

            function showEngine() {
                if (!engine)
                    return;
                root.shownTabIds[tabSlot.tabId] = tabSlot.shown;
                root.setEngineVisible(tabSlot.tabId, tabSlot.shown);
                engine.z = tabSlot.active ? 1 : 0;
                if (tabSlot.active) {
                    root.activeEngine = engine;
                    Qt.callLater(function () {
                        if (root.pageFocusAllowed)
                            root.focusPage();
                    });
                }
                besideSeen = tabSlot.tabBeside;
                if (tabSlot.tabBeside)
                    root.besideEngine = engine;
                else if (root.besideEngine === engine)
                    root.besideEngine = null;
            }

            // The pane goes off screen. A tab beside that has been separated
            // leaves towards its row, over the page that stays; anything else
            // is hidden where it stands. Which of the two it is can only be
            // read once the core has finished announcing the change, so the
            // pane's place is noted now and the decision waits a turn.
            function hideEngine() {
                root.shownTabIds[tabSlot.tabId] = false;
                if (root.besideEngine === engine)
                    root.besideEngine = null;
                const wasBeside = besideSeen;
                besideSeen = false;
                if (!engine)
                    return;
                if (!wasBeside || !root.ease || !engine.visible) {
                    root.setEngineVisible(tabSlot.tabId, false);
                    return;
                }
                const leaving = tabSlot.tabId;
                const partnerId = tabSlot.lastPartnerId;
                const pane = Qt.rect(engine.x, 0, engine.width, engine.height);
                const direction = pane.x > 0 ? 1 : -1;
                Qt.callLater(function () {
                    if (root.shownTabIds[leaving])
                        return;
                    const separated = !root.splitOnShow && root.activeEngine
                          && root.activeEngine.visible && root.browserController
                          && root.browserController.activeTabId === partnerId;
                    if (separated)
                        root.departEngine(leaving, pane, direction);
                    else
                        root.setEngineVisible(leaving, false);
                });
            }

            function loadEngine() {
                if (root.suspended || !everActive || !needsEngine())
                    return;
                engine = root.engines[tabId] || root.createEngine(tabId, tabSlot.tabUrl);
                if (engine) {
                    engine.setZoomFactor(tabSlot.tabZoom);
                    tabSlot.applySoundPolicy();
                }
                showEngine();
            }

            // A page may start playing on its own: a silent video interrupts
            // nobody, and refusing playback outright costs the reader pages
            // that work in every other browser. What waits is the sound — the
            // reader's own muting, and the origin they have not dealt with yet.
            function applySoundPolicy() {
                if (!tabSlot.engine)
                    return;
                tabSlot.engine.autoplayAllowed = true;
                tabSlot.engine.audioMuted = tabSlot.tabMuted || tabSlot.tabSoundSuppressed;
            }

            // What the desktop is told about this tab: whether it is making
            // sound, the title to fall back on when the page declares none,
            // and whatever the page does declare. A Private window sends the
            // sound and nothing that says what it is (ADR 0012).
            function reportSound() {
                if (!tabSlot.engine || tabSlot.engine.pageFrozen)
                    return;
                SoundingTabs.reportSound(tabSlot.tabId, tabSlot.engine.pageAudible,
                                         tabSlot.engine.pageTitle,
                                         root.browserController.privateBrowsing,
                                         tabSlot.engine.pageMediaSession);
            }

            onTabKeepActiveChanged: root.applyPageLifecycle(tabId)
            onTabMutedChanged: tabSlot.applySoundPolicy()
            onTabSoundSuppressedChanged: tabSlot.applySoundPolicy()
            onTabZoomChanged: if (engine)
                                  engine.setZoomFactor(tabSlot.tabZoom)

            onTabUrlChanged: {
                // A tab that has lost its address has lost its page, and the
                // renderer that drew it goes too rather than idling behind the
                // sheet that stands in for it.
                if (!needsEngine()) {
                    if (engine) {
                        root.discardEngine(tabId);
                        engine = null;
                    }
                    return;
                }
                if (!engine)
                    loadEngine();
                else if (engine.currentUrl !== tabUrl)
                    engine.currentUrl = tabUrl;
            }

            onWantsEngineChanged: if (wantsEngine)
                                      loadEngine()

            onShownChanged: {
                if (shown) {
                    everActive = true;
                    loadEngine();
                } else {
                    hideEngine();
                }
            }

            onActiveChanged: {
                if (active) {
                    everActive = true;
                    loadEngine();
                    // A tab with no engine is showing nothing, and the host has
                    // to say so. Leaving the last tab's engine as the active one
                    // would have the window believe a page is up: the sheet that
                    // stands in for a blank tab would stay away, and back and
                    // forward would answer for another tab's history.
                    if (!engine)
                        root.activeEngine = null;
                } else if (engine && shown) {
                    // Focus left for the other half: this page stays on show
                    // and stops answering.
                    if (root.activeEngine === engine)
                        root.activeEngine = null;
                    showEngine();
                }
            }

            // Becoming the tab beside, with an engine already: shown as such.
            // Ceasing to be it is heard through `shown` or `active`, and read
            // there rather than here, since `shown` may not have followed
            // this change yet when this handler runs.
            onTabBesideChanged: if (engine && tabBeside)
                                    showEngine()

            Component.onCompleted: {
                lastPartnerId = splitPartnerId;
                loadEngine();
            }

            // A retained tab's engine survives the Space switch that takes its
            // delegate; every other engine of that Space has already been
            // discarded by the suspension, so there is nothing here to hide.
            // Outside a suspension a disappearing delegate means the reader
            // closed the tab, and the page goes with it.
            Component.onDestruction: {
                if (root.preservingEngines) {
                    root.setEngineVisible(tabId, false);
                    if (root.activeEngine === engine)
                        root.activeEngine = null;
                    if (root.besideEngine === engine)
                        root.besideEngine = null;
                } else {
                    root.discardEngine(tabId);
                }
            }

            Connections {
                target: root

                function onSuspendedChanged() {
                    if (root.suspended) {
                        root.setEngineVisible(tabSlot.tabId, false);
                    } else {
                        tabSlot.loadEngine();
                    }
                }
            }

            Connections {
                target: tabSlot.engine
                ignoreUnknownSignals: true

                function onCurrentUrlChanged() {
                    root.browserController.reportTabPageState(tabSlot.tabId,
                                                              tabSlot.engine.currentUrl,
                                                              tabSlot.engine.pageTitle,
                                                              tabSlot.engine.pageIconUrl,
                                                              tabSlot.engine.loading,
                                                              tabSlot.engine.pageAudible);
                    tabSlot.applySoundPolicy();
                    tabSlot.engine.configureKeyboardNavigation(root.keyboardConfiguration(
                                                                   tabSlot.engine.currentUrl));
                }

                // The reader dealt with the page themselves, which is what the
                // sound was waiting for. The core answers for every tab on that
                // origin, so the next tab showing the same site hears it too
                // without being touched.
                function onUserActivated() {
                    root.browserController.recordOriginInteraction(tabSlot.engine.currentUrl);
                }

                function onPageIconUrlChanged() {
                    root.browserController.reportTabPageState(tabSlot.tabId,
                                                              tabSlot.engine.currentUrl,
                                                              tabSlot.engine.pageTitle,
                                                              tabSlot.engine.pageIconUrl,
                                                              tabSlot.engine.loading,
                                                              tabSlot.engine.pageAudible);
                }

                function onPageTitleChanged() {
                    root.browserController.reportTabPageState(tabSlot.tabId,
                                                              tabSlot.engine.currentUrl,
                                                              tabSlot.engine.pageTitle,
                                                              tabSlot.engine.pageIconUrl,
                                                              tabSlot.engine.loading,
                                                              tabSlot.engine.pageAudible);
                    tabSlot.reportSound();
                }

                function onPageAudibleChanged() {
                    root.browserController.reportTabPageState(tabSlot.tabId,
                                                              tabSlot.engine.currentUrl,
                                                              tabSlot.engine.pageTitle,
                                                              tabSlot.engine.pageIconUrl,
                                                              tabSlot.engine.loading,
                                                              tabSlot.engine.pageAudible);
                    // Sound is the other reason a hidden page runs, and it
                    // starts and stops on the page's own account.
                    root.applyPageLifecycle(tabSlot.tabId);
                    tabSlot.reportSound();
                }

                function onPageMediaSessionChanged() {
                    tabSlot.reportSound();
                }

                // A page that stopped running was dropped from what the desktop
                // is told. Continuing it says so again: the page declares
                // nothing new on its own, because nothing about it changed
                // while it was stopped.
                function onPageFrozenChanged() {
                    tabSlot.reportSound();
                }

                function onLoadingChanged() {
                    root.browserController.reportTabPageState(tabSlot.tabId,
                                                              tabSlot.engine.currentUrl,
                                                              tabSlot.engine.pageTitle,
                                                              tabSlot.engine.pageIconUrl,
                                                              tabSlot.engine.loading,
                                                              tabSlot.engine.pageAudible);
                }

                function onRendererFailed(reason) {
                    root.browserController.reportTabRendererFailure(tabSlot.tabId, reason);
                }

                function onAuxiliaryWindowRequested(request, requestedUrl) {
                    root.auxiliaryWindowRequested(tabSlot.engine, request, requestedUrl);
                }

                function onNewTabRequested(request, requestedUrl) {
                    root.newTabRequested(tabSlot.engine, request, requestedUrl);
                }

                function onBackgroundTabRequested(requestedUrl) {
                    root.backgroundTabRequested(requestedUrl);
                }

                function onPageContextRequested(context) {
                    root.pageContextRequested(tabSlot.engine, context);
                }

                function onPageTooltipRequested(tooltip) {
                    root.pageTooltipRequested(tabSlot.engine, tooltip);
                }

                function onPrintFinished(destination, succeeded) {
                    root.printFinished(destination, succeeded);
                }

                function onPageCaptured(destination, succeeded, reason) {
                    root.pageCaptured(destination, succeeded, reason);
                }

                // The frontend's own close button, which is the reader saying
                // they are finished with it rather than the tab going away.
                function onDeveloperToolsClosed() {
                    root.browserController.closeDeveloperTools();
                }

                function onSitePermissionRequested(requestId, origin, permission) {
                    root.sitePermissionRequested(tabSlot.engine, requestId, origin, permission);
                }

                function onCertificateErrorRaised(requestId, failure) {
                    root.certificateErrorRaised(tabSlot.engine, requestId, failure);
                }

                function onPageSiteDataCleared(origin, cleared, error) {
                    // Only the page the reader asked about answers to them.
                    if (tabSlot.engine !== root.activeEngine)
                        return;
                    root.pageSiteDataCleared(origin, cleared, error);
                }

                function onBrowserPromptRequested(requestId, prompt) {
                    root.browserPromptRequested(tabSlot.engine, requestId, prompt);
                }

                function onFileSelectionRequested(requestId, selection) {
                    root.fileSelectionRequested(tabSlot.engine, requestId, selection);
                }
            }
        }
    }

    // A press in the tab beside makes it the active tab. The handler is a
    // passive one, so the press reaches the page as well: focusing a pane is
    // not a click the page loses.
    Item {
        objectName: "tabBesidePane"
        visible: root.splitOnShow && root.besideEngine !== null
        x: root.besideEngine ? root.besideEngine.x : 0
        width: root.besideEngine ? root.besideEngine.width : 0
        height: root.height
        z: 5

        PointHandler {
            acceptedButtons: Qt.AllButtons
            onActiveChanged: {
                if (active && root.browserController)
                    root.browserController.activateTab(root.tabBesideId);
            }
        }
    }

    // The seam between the panes, drawn as the sidebar draws its own, and the
    // handle over it that moves it.
    Rectangle {
        objectName: "splitDivider"
        visible: root.splitOnShow && !root.siteFullscreenActive
        x: root.leftPaneWidth
        width: root.dividerWidth
        height: root.height
        z: 5
        color: root.colors && root.colors.separator !== undefined ? root.colors.separator :
                                                                    "transparent"

    }

    PanelResizer {
        id: splitResizer
        objectName: "splitResizer"
        visible: root.splitOnShow && !root.siteFullscreenActive
        enabled: visible
        height: parent.height
        x: root.leftPaneWidth + root.dividerWidth / 2 - width / 2
        z: 6
        colors: root.colors
        panelName: "Left pane"
        currentWidth: root.leftPaneWidth
        minimumWidth: root.paneMinimumWidth
        maximumWidth: Math.max(root.paneMinimumWidth, root.width - root.paneMinimumWidth)
        defaultWidth: Math.round(root.width / 2)

        onWidthRequested: function (width) {
            root.setLeftPaneWidth(width);
        }
        onPageFocusRequested: root.focusPage()
    }

    Connections {
        target: root.browserController

        function onRendererRecoveryReloadRequested() {
            root.reloadPage();
        }
    }

    Connections {
        target: root.keyboardManager

        function onConfigurationChanged() {
            if (root.activeEngine) {
                root.activeEngine.configureKeyboardNavigation(root.keyboardConfiguration(
                                                                  root.activeEngine.currentUrl));
            }
        }
    }

    onPageControlAccentChanged: {
        for (const tabId in root.engines)
            root.engines[tabId].pageControlAccent = root.pageControlAccent;
    }

    onPagePaletteChanged: {
        for (const tabId in root.engines)
            root.engines[tabId].pagePalette = root.pagePalette;
    }

    // A theme the reader changes has to reach the pages already open, not only
    // the next one to load.
    onPageScrollbarThumbChanged: {
        for (const tabId in root.engines)
            root.engines[tabId].pageScrollbarThumb = root.pageScrollbarThumb;
    }

    onPageScrollbarTrackChanged: {
        for (const tabId in root.engines)
            root.engines[tabId].pageScrollbarTrack = root.pageScrollbarTrack;
    }

    onDeveloperToolsColorsChanged: {
        for (const tabId in root.engines)
            root.engines[tabId].developerToolsColors = root.developerToolsColors;
    }

    onHintThemeChanged: {
        for (const tabId in root.engines) {
            const engine = root.engines[tabId];
            engine.configureKeyboardNavigation(root.keyboardConfiguration(engine.currentUrl));
        }
    }
}
