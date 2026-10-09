import QtQuick
import QtQuick.Dialogs as Dialogs
import QtQuick.Window
import Omaweb
import qs.Commons
import "DevicePixels.mjs" as DevicePixels
import "SpaceColour.mjs" as SpaceColour

// A window of its own showing one tab of a Space (ADR 0062), so a page can sit
// on another monitor. It has no sidebar: a slim strip names the page's address,
// what the connection is and the Space, and every command about another tab or
// Space goes to the main window. The page is the main window's engine for the
// tab, lent to this window and handed back when it closes, so nothing reloads
// either way.
Window {
    id: root
    objectName: "tabWindow"

    // The tab as the core lists it: `tabId`, `spaceId`, `spaceName`,
    // `spaceColor`, `url`, `title`, `pinned`, `stripHidden`, `zoom` and `muted`.
    property var entry: ({})
    required property var browser
    // The main window's page area, which owns the engine and lends it here.
    required property var engineHost
    // Where commands about other tabs and Spaces run.
    required property var mainWindow
    property var colors
    property var commands: null
    property var keymap: null
    property string iconFontFamily
    property bool spaceColours: true
    property bool useFavicons: true
    property bool tintFavicons: false
    // Whether a page's new-tab request opens a Glance, which is the reader's
    // setting and the main window's to read.
    property bool glanceEnabled: true

    readonly property string tabId: root.entry.tabId || ""
    // The page this window draws, once the main window has lent it.
    property var engine: null
    // The main window is closing every window with it, which keeps the tab in
    // its Tab window for the next start rather than putting it back.
    property bool quitting: false
    // The tab has left the core's list, put back, closed or gone with its
    // Space, and this window is on its way out.
    property bool released: false

    readonly property string pageTitle: root.engine && root.engine.pageTitle.length > 0
                                        ? root.engine.pageTitle : String(root.entry.title || "")
    readonly property url pageUrl: root.engine ? root.engine.currentUrl : (root.entry.url || "")
    readonly property string pageHost: root.hostOf(root.pageUrl)

    // The compositor reads a window's first title when it maps the window, so
    // a rule can match a Tab window by it where it cannot by class. It is
    // English on purpose, like a class name: a rule written once has to keep
    // matching whatever language the chrome is in. Once the window is on
    // screen the title names the page and its Space.
    property bool mapped: false
    title: root.mapped ? qsTr("%1 · %2").arg(root.pageTitle).arg(String(root.entry.spaceName || "")) :
                         "Omaweb tab window"

    width: 1024
    height: 768
    minimumWidth: 360
    minimumHeight: 240
    visible: true
    color: root.colors ? root.colors.windowOpaque : "#16151d"
    flags: Qt.platform.os === "osx" ? Qt.Window : Qt.Window | Qt.FramelessWindowHint

    // `omaweb-tab` to the compositor, so a Hyprland rule can send the window to
    // another monitor. Where Qt cannot be asked, the window keeps `omaweb`, and
    // the first title above is what a rule matches.
    WindowAppId {
        id: appId
        window: root
        appId: "omaweb-tab"
    }

    onFrameSwapped: if (!root.mapped)
                        root.mapped = true

    function hostOf(url) {
        const found = /^[a-z][a-z0-9+.-]*:\/\/([^/?#:]+)/i.exec(String(url));
        return found === null ? "" : found[1].toLowerCase();
    }

    // The strip is the reader's to hide, and comes back by itself while a
    // prompt needs it. A page moving to another site brings it back for good,
    // so a page never changes site out of sight.
    readonly property bool promptShown: root.permissionQuestion !== null
                                        || root.certificateQuestion !== null
                                        || root.securityKeyResponder !== null || root.browserPrompt
                                        !== null
    readonly property bool stripShown: !root.siteFullscreen && (root.entry.stripHidden !== true
                                                                || root.promptShown
                                                                || root.omnibarOpen)
    property string hiddenOnHost: ""
    onPageHostChanged: {
        if (root.entry.stripHidden === true && root.pageHost.length > 0 && root.hiddenOnHost.length
                > 0 && root.pageHost !== root.hiddenOnHost)
            root.browser.setTabStripHidden(root.tabId, false);
    }

    function toggleStrip() {
        const hide = root.entry.stripHidden !== true;
        if (hide)
            root.hiddenOnHost = root.pageHost;
        root.browser.setTabStripHidden(root.tabId, hide);
    }

    // A page that asks for the screen, as a video does, fills this window
    // and the window fills its screen. The main window is left as it is.
    // The window coming out of fullscreen by any other route tells the page,
    // so it does not go on drawing for a screen it no longer has.
    readonly property bool siteFullscreen: root.engine !== null && root.engine.siteFullscreenActive
    property int windowedVisibility: Window.Windowed
    onSiteFullscreenChanged: {
        if (root.siteFullscreen) {
            if (root.visibility !== Window.FullScreen)
                root.windowedVisibility = root.visibility;
            root.visibility = Window.FullScreen;
            return;
        }
        if (root.visibility === Window.FullScreen)
            root.visibility = root.windowedVisibility;
    }
    onVisibilityChanged: {
        if (root.siteFullscreen && root.visibility !== Window.FullScreen)
            root.engine.exitSiteFullscreen();
    }

    // Built once the engine is lent; a restart may reach this window before
    // the tab's Space has a profile to build its page on, so it asks again.
    // A page that still cannot be had after five seconds never will be, and
    // the tab goes back to the sidebar rather than the window asking for ever.
    property int adoptAttempts: 0
    function adoptPage() {
        if (root.engine || root.released || root.tabId.length === 0)
            return;
        root.engine = root.engineHost.lendEngine(root.entry, pageHost);
        if (!root.engine) {
            root.adoptAttempts += 1;
            if (root.adoptAttempts >= 50) {
                root.putBack(false);
                return;
            }
            adoptAgain.start();
            return;
        }
        root.adoptAttempts = 0;
        // A page built a moment ago may not have its address yet, and then the
        // address the tab was saved with is the site the strip was hidden on.
        const host = root.hostOf(root.engine.currentUrl);
        root.hiddenOnHost = host.length > 0 ? host : root.hostOf(root.entry.url);
    }

    // A page taken from under the window, as a Sync reload takes the pages of
    // the Space on show, is built again here, and what it asked goes with it
    // unanswered. A tab closed or put back takes the window with it, which is
    // released by then or on its way.
    Connections {
        target: root.engineHost

        function onEngineDiscarded(tabId) {
            if (tabId !== root.tabId || root.released)
                return;
            root.refuseRequestsFrom(root.engine);
            root.engine = null;
            adoptAgain.start();
        }
    }

    Timer {
        id: adoptAgain
        interval: 100
        onTriggered: root.adoptPage()
    }

    // A turn later, so a page the window was opened for is handed to its tab
    // before the window asks for one.
    Component.onCompleted: Qt.callLater(root.adoptPage)

    // Hands the page back to the main window and the tab back to its sidebar.
    // With `show`, the main window switches to its Space and shows it.
    function putBack(show) {
        if (root.released)
            return;
        root.release();
        root.browser.putBackTab(root.tabId, show);
        if (show) {
            root.mainWindow.raise();
            root.mainWindow.requestActivate();
        }
    }

    // The tab is no longer this window's: the page, if it is still there, goes
    // back to the main window before the window goes.
    function release() {
        if (root.released)
            return;
        root.closeGlance();
        root.refuseRequestsFrom(root.engine);
        if (root.siteFullscreen)
            root.engine.exitSiteFullscreen();
        root.released = true;
        root.engineHost.reclaimEngine(root.tabId);
        root.engine = null;
    }

    // Closing the window is not closing the tab: it goes back to the sidebar.
    onClosing: function (close) {
        if (root.quitting || root.released)
            return;
        root.putBack(false);
    }

    function bringForward() {
        root.raise();
        root.requestActivate();
    }

    // What this window answers for itself: the tab's own page, its strip, and
    // putting it back. Anything about another tab or a Space is the main
    // window's, which comes forward to run it.
    function run(command, argument) {
        const engine = root.engine;
        switch (command) {
        case "back":
            if (engine)
                engine.goBack();
            return true;
        case "forward":
            if (engine)
                engine.goForward();
            return true;
        case "reload":
            if (engine)
                engine.reloadPage();
            return true;
        case "reload-bypassing-cache":
            if (engine)
                engine.reloadPageBypassingCache();
            return true;
        case "stop-loading":
            if (engine)
                engine.stopLoading();
            return true;
        case "open-address":
            root.openOmnibar();
            return true;
        case "close-tab":
            return root.browser.closeTabInSpace(root.tabId, root.entry.spaceId);
        case "put-back-tab":
            root.putBack(true);
            return true;
        case "toggle-tab-window-strip":
            root.toggleStrip();
            return true;
        case "find":
            root.openFind();
            return true;
        case "find-next":
            root.stepFind(true);
            return true;
        case "find-previous":
            root.stepFind(false);
            return true;
        case "zoom-in":
            root.browser.stepTabZoom(root.tabId, 1);
            return true;
        case "zoom-out":
            root.browser.stepTabZoom(root.tabId, -1);
            return true;
        case "zoom-reset":
            root.browser.resetTabZoom(root.tabId);
            return true;
        case "developer-tools":
            root.browser.toggleTabDeveloperTools(root.tabId);
            return true;
        case "focus-page":
            root.focusPage();
            return true;
        case "glance-to-tab":
            root.keepGlance();
            return true;
        }
        if (!root.commands)
            return false;
        root.mainWindow.raise();
        root.mainWindow.requestActivate();
        return root.commands.run(command, argument);
    }

    // An address the tab is given from outside, as an Agent gives one, is
    // loaded in the page here. The page's own reports come back as the same
    // address and change nothing.
    property string listedUrl: String(root.entry.url || "")
    onEntryChanged: {
        const url = String(root.entry.url || "");
        if (url === root.listedUrl)
            return;
        root.listedUrl = url;
        if (root.engine && url.length > 0 && url !== "about:blank" && String(
                    root.engine.currentUrl) !== url)
            root.engine.currentUrl = url;
    }

    // The tab's zoom is kept by the core, and its page follows it.
    readonly property real tabZoom: root.entry.zoom !== undefined ? root.entry.zoom : 1.0
    onTabZoomChanged: if (root.engine)
                          root.engine.setZoomFactor(root.tabZoom)
    onEngineChanged: if (root.engine)
                         root.engine.setZoomFactor(root.tabZoom)

    function focusPage() {
        if (root.engine)
            root.engine.focusPage();
    }

    // The keymap's bindings, as the main window has them. A key bound to a
    // command this window answers runs here; any other goes to the main
    // window.
    Repeater {
        model: root.keymap ? Object.keys(root.keymap.browserBindings) : []

        Item {
            required property string modelData

            Shortcut {
                sequence: root.keymap.keySequence(modelData)
                enabled: !root.omnibarOpen && (root.keymap.isChord(modelData)
                                               || root.keymap.pageCommandsEnabled)
                context: Qt.WindowShortcut
                onActivated: root.run(root.keymap.commandFor(modelData), parseInt(modelData.slice(
                                                                                      -1), 10) - 1)
            }
        }
    }

    // Find, as the main window keeps it, for this window's one page.
    property bool findOpen: false

    function openFind() {
        root.findOpen = true;
        Qt.callLater(findBar.focusField);
    }

    function closeFind() {
        root.findOpen = false;
        root.focusPage();
    }

    function stepFind(forward) {
        if (!root.findOpen || findBar.text.length === 0) {
            root.openFind();
            return;
        }
        if (root.engine)
            root.engine.findText(findBar.text, forward);
    }

    // The Omnibar over this window navigates this window's page.
    property bool omnibarOpen: false

    function openOmnibar() {
        omnibar.beginAddress(String(root.pageUrl), false);
        root.omnibarOpen = true;
    }

    function closeOmnibar() {
        root.omnibarOpen = false;
        root.focusPage();
    }

    function navigate(text) {
        const address = root.browser.addressFor(text);
        if (root.engine && String(address).length > 0)
            root.engine.currentUrl = address;
        root.closeOmnibar();
    }

    // What the page asks of the browser, answered for this window. Its
    // address, title and sound are the tab's, whichever Space it is in.
    function reportPageState() {
        const engine = root.engine;
        if (!engine)
            return;
        root.browser.reportTabPageState(root.tabId, engine.currentUrl, engine.pageTitle,
                                        engine.pageIconUrl, engine.loading, engine.pageAudible);
    }

    Connections {
        target: root.engine
        ignoreUnknownSignals: true

        function onCurrentUrlChanged() {
            root.reportPageState();
        }
        function onPageTitleChanged() {
            root.reportPageState();
            root.reportSound();
        }
        function onPageIconUrlChanged() {
            root.reportPageState();
        }
        function onLoadingChanged() {
            root.reportPageState();
        }
        function onPageAudibleChanged() {
            root.reportPageState();
            root.reportSound();
        }
        function onPageMediaSessionChanged() {
            root.reportSound();
        }
        function onUserActivated() {
            root.browser.recordOriginInteraction(root.engine.currentUrl);
        }
        function onRendererFailed(reason) {
            root.browser.reportTabRendererFailure(root.tabId, reason);
        }
        function onFormSubmitted(fields) {
            root.browser.rememberFormFields(root.engine.spaceId, fields);
        }
        function onDeveloperToolsClosed() {
            root.browser.closeDeveloperTools();
        }
        function onWindowCloseRequested() {
            root.browser.closeTabInSpace(root.tabId, root.entry.spaceId);
        }
        function onNewTabRequested(request, requestedUrl) {
            root.openRequestedPage(root.engine, request, requestedUrl);
        }
        function onBackgroundTabRequested(requestedUrl) {
            root.openBackgroundTab(requestedUrl);
        }
        function onAuxiliaryWindowRequested(request, requestedUrl) {
            root.mainWindow.openAuxiliaryWindow(root.engine, request, requestedUrl, root);
        }
        function onSitePermissionRequested(requestId, origin, permission) {
            root.askPermission(root.engine, requestId, origin, permission);
        }
        function onCertificateErrorRaised(requestId, failure) {
            root.askAboutCertificate(root.engine, requestId, failure);
        }
        function onSecurityKeyRequested(requestId, step) {
            root.showSecurityKey(root.engine, requestId, step);
        }
        function onBrowserPromptRequested(requestId, prompt) {
            root.showBrowserPrompt(root.engine, requestId, prompt);
        }
        function onFileSelectionRequested(requestId, selection) {
            root.showFileSelection(root.engine, requestId, selection);
        }
    }

    // The desktop's media controls reach this page as any tab's.
    function reportSound() {
        const engine = root.engine;
        if (!engine || engine.pageFrozen)
            return;
        SoundingTabs.reportSound(root.tabId, engine.pageAudible, engine.pageTitle, false,
                                 engine.pageMediaSession);
        root.engineHost.applyPageLifecycle(root.tabId);
    }

    // A link that asks for a new tab follows the main window's rule here: a
    // Glance over the page, or with Glance off a Tab window of its own, in
    // this Space. Neither moves the main window.
    function openRequestedPage(opener, request, requestedUrl) {
        // An Agent tab's page opens a window the Agent drives, as it does in
        // the main window.
        if (root.engineHost.agentTabIdOf(opener).length > 0) {
            root.mainWindow.openAuxiliaryWindow(opener, request, requestedUrl, root);
            return;
        }
        const destination = String(requestedUrl).length > 0 ? String(requestedUrl) : "about:blank";
        if (root.glanceEnabled && opener === root.engine && root.openGlance(request, destination))
            return;
        root.openNewTabWindow(request, destination);
    }

    // A background request is a tab in the main window's sidebar, in this
    // window's Space, and nothing comes forward.
    function openBackgroundTab(requestedUrl) {
        root.browser.openTabInSpace(String(root.entry.spaceId || ""), requestedUrl);
    }

    function openNewTabWindow(request, destination) {
        const spaceId = String(root.entry.spaceId || "");
        const tabId = root.browser.openTabWindow(spaceId, destination);
        if (tabId.length === 0 || !request)
            return tabId;
        const engine = root.engineHost.createDetachedEngineIn(null, "about:blank", spaceId);
        if (!engine)
            return tabId;
        root.engineHost.adoptTabWindowEngine(tabId, engine, spaceId);
        engine.acceptNewWindowRequest(request);
        return tabId;
    }

    // The Glance, as the main window has one: one page at a time over this
    // window's page, on this window's Space.
    property var glanceEngine: null
    readonly property bool glanceOpen: root.glanceEngine !== null

    function openGlance(request, destination) {
        root.closeGlance();
        const engine = root.engineHost.createDetachedEngineIn(glance.pageHost, request
                                                              ? "about:blank" : destination, String(
                                                                  root.entry.spaceId || ""));
        if (!engine)
            return false;
        const opener = root.engine;
        glance.origin = opener && opener.pressOrigin.width > 0 ? opener.mapToItem(glance,
                                                                                  opener.pressOrigin) :
                                                                 Qt.rect(0, 0, 0, 0);
        engine.anchors.fill = glance.pageHost;
        engine.visible = true;
        root.glanceEngine = engine;
        if (request)
            engine.acceptNewWindowRequest(request);
        return true;
    }

    function closeGlance() {
        const engine = root.glanceEngine;
        if (!engine)
            return;
        root.refuseRequestsFrom(engine);
        root.glanceEngine = null;
        engine.destroy(120);
        root.focusPage();
    }

    // Keeping the Glance makes its page a new Tab window in this Space, engine
    // and all.
    function keepGlance() {
        const engine = root.glanceEngine;
        if (!engine)
            return;
        root.refuseRequestsFrom(engine);
        root.glanceEngine = null;
        const spaceId = String(root.entry.spaceId || "");
        const address = String(engine.currentUrl).length > 0 && String(engine.currentUrl) !== "about:blank"
              ? engine.currentUrl : glance.pageAddress;
        const tabId = root.browser.openTabWindow(spaceId, address);
        if (tabId.length === 0) {
            engine.destroy();
            return;
        }
        engine.anchors.fill = undefined;
        root.engineHost.adoptTabWindowEngine(tabId, engine, spaceId);
    }

    Connections {
        target: root.glanceEngine
        ignoreUnknownSignals: true

        function onWindowCloseRequested() {
            root.closeGlance();
        }
        function onNewTabRequested(request, requestedUrl) {
            root.closeGlance();
            root.openNewTabWindow(request, String(requestedUrl).length > 0 ? String(requestedUrl) :
                                                                             "about:blank");
        }
        function onBackgroundTabRequested(requestedUrl) {
            root.openBackgroundTab(requestedUrl);
        }
        function onAuxiliaryWindowRequested(request, requestedUrl) {
            root.mainWindow.openAuxiliaryWindow(root.glanceEngine, request, requestedUrl, root);
        }
        function onUserActivated() {
            root.browser.recordOriginInteraction(root.glanceEngine.currentUrl);
        }
        function onFormSubmitted(fields) {
            root.browser.rememberFormFields(root.glanceEngine.spaceId, fields);
        }
        function onSitePermissionRequested(requestId, origin, permission) {
            root.askPermission(root.glanceEngine, requestId, origin, permission);
        }
        function onCertificateErrorRaised(requestId, failure) {
            root.askAboutCertificate(root.glanceEngine, requestId, failure);
        }
        function onSecurityKeyRequested(requestId, step) {
            root.showSecurityKey(root.glanceEngine, requestId, step);
        }
        function onBrowserPromptRequested(requestId, prompt) {
            root.showBrowserPrompt(root.glanceEngine, requestId, prompt);
        }
        function onFileSelectionRequested(requestId, selection) {
            root.showFileSelection(root.glanceEngine, requestId, selection);
        }
        function onRendererFailed(reason) {
            root.closeGlance();
        }
    }

    // The questions a page in this window asks are asked here, where the
    // reader is looking, and remembered in the page's own Space. One stands
    // at a time and the rest wait behind it.
    property var permissionQuestion: null
    property var heldPermissionQuestions: []

    function askPermission(engine, requestId, origin, permission) {
        const question = {
            "engine": engine,
            "requestId": requestId,
            "origin": origin,
            "permission": permission
        };
        if (root.permissionQuestion !== null) {
            root.heldPermissionQuestions = root.heldPermissionQuestions.concat([question]);
            return;
        }
        root.permissionQuestion = question;
    }

    // `record` is false for a question dropped with its page, which the
    // reader never answered.
    function respondToPermission(decision, record) {
        const question = root.permissionQuestion;
        if (!question)
            return;
        if (record !== false)
            root.browser.setPermissionDecision(question.origin, question.permission, decision,
                                               String(question.engine.spaceId || ""));
        question.engine.respondToPermission(question.requestId, decision);
        const held = root.heldPermissionQuestions;
        root.permissionQuestion = held.length > 0 ? held[0] : null;
        root.heldPermissionQuestions = held.slice(1);
    }

    property var certificateQuestion: null

    function askAboutCertificate(engine, requestId, failure) {
        const offerable = root.certificateQuestion === null
              && root.browser.mayOfferCertificateException(failure.url, failure.overridable === true,
                                                           failure.mainFrame === true,
                                                           failure.fatal === true);
        if (!offerable) {
            engine.respondToCertificateError(requestId, false);
            return;
        }
        root.certificateQuestion = {
            "engine": engine,
            "requestId": requestId,
            "failure": failure
        };
    }

    function respondToCertificate(accepted) {
        const question = root.certificateQuestion;
        if (!question)
            return;
        question.engine.respondToCertificateError(question.requestId, accepted);
        if (accepted)
            root.browser.recordCertificateException(question.failure.url);
        root.certificateQuestion = null;
    }

    property var securityKeyResponder: null
    property string securityKeyRequestId: ""
    property var securityKeyStep: ({})

    function showSecurityKey(engine, requestId, step) {
        const current = root.securityKeyResponder === engine && root.securityKeyRequestId
              === requestId;
        if (step.state === "closed") {
            if (current)
                root.clearSecurityKey();
            return;
        }
        if (!current && root.securityKeyResponder !== null) {
            engine.respondToSecurityKey(requestId, {
                                            "action": "cancel"
                                        });
            return;
        }
        root.securityKeyResponder = engine;
        root.securityKeyRequestId = requestId;
        root.securityKeyStep = step;
    }

    function clearSecurityKey() {
        root.securityKeyResponder = null;
        root.securityKeyRequestId = "";
        root.securityKeyStep = ({});
    }

    function answerSecurityKey(answer) {
        const responder = root.securityKeyResponder;
        const requestId = root.securityKeyRequestId;
        if (!responder)
            return;
        if (answer.action === "cancel")
            root.clearSecurityKey();
        responder.respondToSecurityKey(requestId, answer);
    }

    property var browserPrompt: null

    function showBrowserPrompt(engine, requestId, prompt) {
        if (root.browserPrompt !== null) {
            engine.respondToBrowserPrompt(requestId, false, {});
            return;
        }
        root.browserPrompt = {
            "engine": engine,
            "requestId": requestId,
            "prompt": prompt
        };
    }

    function respondToBrowserPrompt(accepted, text, user, password, stopPrompts, remember) {
        const pending = root.browserPrompt;
        root.browserPrompt = null;
        if (pending) {
            pending.engine.respondToBrowserPrompt(pending.requestId, accepted, {
                                                      "text": text,
                                                      "user": user,
                                                      "password": password,
                                                      "stopPrompts": stopPrompts,
                                                      "remember": remember
                                                  });
        }
    }

    property var fileSelection: null

    function showFileSelection(engine, requestId, selection) {
        if (root.fileSelection !== null) {
            engine.respondToFileSelection(requestId, []);
            return;
        }
        root.fileSelection = {
            "engine": engine,
            "requestId": requestId
        };
        if (selection.mode === "folder") {
            folderDialog.open();
            return;
        }
        fileDialog.fileMode = selection.mode === "open-multiple" ? Dialogs.FileDialog.OpenFiles : (
                                                                       selection.mode === "save"
                                                                       ? Dialogs.FileDialog.SaveFile :
                                                                         Dialogs.FileDialog.OpenFile);
        fileDialog.open();
    }

    function respondToFileSelection(files) {
        const pending = root.fileSelection;
        root.fileSelection = null;
        if (pending)
            pending.engine.respondToFileSelection(pending.requestId, files);
    }

    Dialogs.FileDialog {
        id: fileDialog
        objectName: "tabWindowFileDialog"
        title: qsTr("Choose file")
        onAccepted: {
            const files = [];
            for (let index = 0; index < selectedFiles.length; ++index)
                files.push(String(selectedFiles[index]));
            root.respondToFileSelection(files);
        }
        onRejected: root.respondToFileSelection([])
    }

    Dialogs.FolderDialog {
        id: folderDialog
        objectName: "tabWindowFolderDialog"
        title: qsTr("Choose folder")
        onAccepted: root.respondToFileSelection([String(selectedFolder)])
        onRejected: root.respondToFileSelection([])
    }

    // A question is dropped with the page that asked it, unanswered.
    function refuseRequestsFrom(engine) {
        if (!engine)
            return;
        if (root.permissionQuestion && root.permissionQuestion.engine === engine)
            root.respondToPermission(BrowserController.Block, false);
        root.heldPermissionQuestions = root.heldPermissionQuestions.filter(function (question) {
            return question.engine !== engine;
        });
        if (root.certificateQuestion && root.certificateQuestion.engine === engine)
            root.respondToCertificate(false);
        if (root.securityKeyResponder === engine)
            root.answerSecurityKey({
                                       "action": "cancel"
                                   });
        if (root.browserPrompt && root.browserPrompt.engine === engine)
            root.respondToBrowserPrompt(false, "", "", "", false, false);
        if (root.fileSelection && root.fileSelection.engine === engine)
            root.respondToFileSelection([]);
    }

    Rectangle {
        id: shell
        objectName: "tabWindowShell"
        anchors.fill: parent
        color: root.colors ? root.colors.windowOpaque : "transparent"

        // The strip: the Space, what the connection is, and the address, on
        // the chrome's 8 px grid with 4 as the half step.
        Rectangle {
            id: strip
            objectName: "tabWindowStrip"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: root.stripShown ? 32 : 0
            visible: root.stripShown
            color: root.colors ? root.colors.sidebarOpaque : "transparent"

            Row {
                id: spaceName
                objectName: "tabWindowSpace"
                anchors.left: parent.left
                anchors.leftMargin: Style.space(8)
                anchors.verticalCenter: parent.verticalCenter
                spacing: Style.space(4)

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 8
                    height: 8
                    radius: 4
                    color: root.colors ? SpaceColour.drawn(root.colors, String(
                                                               root.entry.spaceColor || ""),
                                                           root.spaceColours) : "transparent"
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: String(root.entry.spaceName || "")
                    color: root.colors ? root.colors.mutedText : "white"
                    font.family: Style.font.family
                    font.pixelSize: Style.font.bodySmall
                }
            }

            Text {
                id: securityGlyph
                objectName: "tabWindowSiteInformation"
                readonly property string state: root.engine ? root.engine.connectionState :
                                                              "internal"
                readonly property bool secure: state === "secure"
                readonly property bool certificateError: state === "certificate-error"
                anchors.left: spaceName.right
                anchors.leftMargin: Style.space(16)
                anchors.verticalCenter: parent.verticalCenter
                text: certificateError ? "warning" : (secure ? "lock" : "lock_open")
                color: root.colors ? (certificateError ? root.colors.urgent :
                                                         root.colors.mutedText) : "white"
                font.family: root.iconFontFamily
                font.pixelSize: Style.font.iconLarge
                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Site information: %1").arg(siteInformation.verdict)
                Accessible.onPressAction: siteInformation.open = !siteInformation.open

                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -5
                    cursorShape: Qt.PointingHandCursor
                    onClicked: siteInformation.open = !siteInformation.open
                }
            }

            Text {
                id: address
                objectName: "tabWindowAddress"
                anchors.left: securityGlyph.right
                anchors.leftMargin: Style.space(8)
                anchors.right: parent.right
                anchors.rightMargin: Style.space(8)
                anchors.verticalCenter: parent.verticalCenter
                text: String(root.pageUrl).replace(/^[a-z]+:\/\//, "")
                color: root.colors ? root.colors.text : "white"
                elide: Text.ElideMiddle
                font.family: Style.font.family
                font.pixelSize: Style.font.body

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.openOmnibar()
                }
            }
        }

        // What the strip's lock says, for this window's page alone.
        Rectangle {
            id: siteInformation
            objectName: "tabWindowSiteCard"
            property bool open: false
            readonly property string verdict: securityGlyph.certificateError ? qsTr(
                                                                                   "Certificate could not be verified") :
                                                                               (securityGlyph.secure
                                                                                ? qsTr("Connection is secure") :
                                                                                  (securityGlyph.state
                                                                                   === "internal"
                                                                                   ? qsTr("Omaweb page") :
                                                                                     qsTr("Not secure")))
            visible: open && root.stripShown
            z: 30
            x: securityGlyph.x
            y: strip.height + 4
            width: Math.min(360, root.width - x - 12)
            height: siteColumn.implicitHeight + 24
            radius: Style.cornerRadius
            color: root.colors ? root.colors.surface : "black"
            border.width: 1
            border.color: root.colors ? root.colors.separator : "transparent"

            Column {
                id: siteColumn
                anchors.fill: parent
                anchors.margins: 12
                spacing: 4

                Text {
                    width: parent.width
                    text: siteInformation.verdict
                    color: root.colors ? root.colors.text : "white"
                    font.family: Style.font.family
                    font.pixelSize: Style.font.body
                    wrapMode: Text.Wrap
                }

                Text {
                    width: parent.width
                    text: root.pageHost
                    color: root.colors ? root.colors.mutedText : "white"
                    font.family: Style.font.family
                    font.pixelSize: Style.font.bodySmall
                    elide: Text.ElideMiddle
                }
            }
        }

        // Where the lent page is drawn.
        Item {
            id: pageHost
            objectName: "tabWindowPageHost"
            anchors.left: parent.left
            anchors.right: developerToolsDock.visible ? developerToolsDock.left : parent.right
            anchors.top: strip.bottom
            anchors.bottom: parent.bottom
        }

        // The inspector of this window's page, beside it here rather than in
        // the main window.
        readonly property bool inspected: root.engineHost.inspectedTabId === root.tabId
                                          && root.tabId.length > 0
        DeveloperToolsDock {
            id: developerToolsDock
            objectName: "tabWindowDeveloperToolsDock"
            anchors.top: strip.bottom
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            width: Math.round(parent.width * 0.4)
            visible: shell.inspected && root.engine !== null
            z: 4
            colors: root.colors
            developerToolsView: shell.inspected ? root.engineHost.developerToolsView : null
        }

        FindBar {
            id: findBar
            objectName: "tabWindowFindBar"
            x: DevicePixels.snap(parent.width - width - 20, root.devicePixelRatio)
            anchors.top: pageHost.top
            anchors.topMargin: 16
            z: 41
            colors: root.colors
            iconFontFamily: root.iconFontFamily
            open: root.findOpen
            query: root.engine ? root.engine.findQuery : ""
            matchCount: root.engine ? root.engine.findMatchCount : 0
            activeMatch: root.engine ? root.engine.findActiveMatch : 0

            onSearchRequested: function (text, forward) {
                if (root.engine)
                    root.engine.findText(text, forward);
            }
            onClosed: root.closeFind()
        }

        Glance {
            id: glance
            objectName: "tabWindowGlance"
            anchors.fill: pageHost
            z: 35
            colors: root.colors
            iconFontFamily: root.iconFontFamily
            open: root.glanceOpen
            pageSource: pageHost
            engine: root.glanceEngine

            onClosed: root.closeGlance()
            onOpenAsTabRequested: root.keepGlance()
        }

        PageQuestionBar {
            id: permissionBar
            objectName: "tabWindowPermissionBar"
            anchors.left: pageHost.left
            anchors.right: pageHost.right
            anchors.top: pageHost.top
            z: 40
            colors: root.colors
            iconFontFamily: root.iconFontFamily
            backdropSource: pageHost
            open: root.permissionQuestion !== null
            glyph: "shield_person"
            readonly property var question: root.permissionQuestion || ({})
            readonly property bool rememberable: question.permission !== undefined
                                                 && root.browser.permissionPolicy(
                                                     question.permission)
                                                 === BrowserController.Rememberable
            message: qsTr("%1 asked for a protected browser capability").arg(String(question.origin
                                                                                    || ""))
            detail: rememberable ? qsTr("%1 · remembered in %2").arg(String(question.permission
                                                                            || "")).arg(String(
                                                                                            root.entry.spaceName
                                                                                            || "")) : qsTr(
                                       "%1 · asked every time, never remembered").arg(String(
                                                                                          question.permission
                                                                                          || ""))
            actions: rememberable ? [
                                        {
                                            "label": qsTr("Allow once"),
                                            "decision": BrowserController.AllowOnce
                                        },
                                        {
                                            "label": qsTr("Always allow"),
                                            "decision": BrowserController.AllowPersistently
                                        },
                                        {
                                            "label": qsTr("Block"),
                                            "decision": BrowserController.Block
                                        }
                                    ] : [
                                        {
                                            "label": qsTr("Allow once"),
                                            "decision": BrowserController.AllowOnce
                                        },
                                        {
                                            "label": qsTr("Block"),
                                            "decision": BrowserController.Block
                                        }
                                    ]

            onActionTriggered: function (index) {
                root.respondToPermission(permissionBar.actions[index].decision);
            }
        }

        PageQuestionBar {
            id: certificateBar
            objectName: "tabWindowCertificateBar"
            anchors.left: pageHost.left
            anchors.right: pageHost.right
            anchors.top: pageHost.top
            z: 41
            colors: root.colors
            iconFontFamily: root.iconFontFamily
            backdropSource: pageHost
            open: root.certificateQuestion !== null
            glyph: "warning"
            readonly property var failure: root.certificateQuestion
                                           ? root.certificateQuestion.failure : ({})
            message: qsTr("%1 could not prove its certificate").arg(String(failure.origin || ""))
            detail: qsTr("%1 · local development site · this load only, never remembered").arg(
                        String(failure.description || ""))
            actions: [
                {
                    "label": qsTr("Continue once")
                },
                {
                    "label": qsTr("Block")
                }
            ]

            onActionTriggered: function (index) {
                root.respondToCertificate(index === 0);
            }
        }

        SecurityKeyBar {
            objectName: "tabWindowSecurityKeyBar"
            anchors.left: pageHost.left
            anchors.right: pageHost.right
            anchors.top: pageHost.top
            z: 43
            colors: root.colors
            iconFontFamily: root.iconFontFamily
            backdropSource: pageHost
            open: root.securityKeyResponder !== null
            step: root.securityKeyStep
            transports: root.securityKeyResponder ? root.securityKeyResponder.securityKeyTransports :
                                                    []
            place: String(root.entry.spaceName || "")

            onAnswered: function (answer) {
                root.answerSecurityKey(answer);
            }
        }

        PagePromptBar {
            objectName: "tabWindowPromptBar"
            anchors.fill: pageHost
            z: 43
            colors: root.colors
            iconFontFamily: root.iconFontFamily
            backdropSource: pageHost
            open: root.browserPrompt !== null
            prompt: root.browserPrompt ? root.browserPrompt.prompt : ({})

            onAnswered: function (accepted, text, user, password, stopPrompts, remember) {
                root.respondToBrowserPrompt(accepted, text, user, password, stopPrompts, remember);
            }
        }
    }

    // Site fullscreen ends with Escape whatever the page does with the key,
    // as it does in the main window.
    Shortcut {
        sequence: "Esc"
        enabled: root.siteFullscreen && !root.omnibarOpen && !root.promptShown
        context: Qt.WindowShortcut
        onActivated: root.engine.exitSiteFullscreen()
    }

    // A Glance closes with Escape whatever its page does with the key.
    Shortcut {
        sequence: "Esc"
        enabled: root.glanceOpen && !root.omnibarOpen && !root.promptShown && !root.siteFullscreen
        context: Qt.WindowShortcut
        onActivated: root.closeGlance()
    }

    Omnibar {
        id: omnibar
        objectName: "tabWindowOmnibar"
        anchors.fill: parent
        z: 50
        colors: root.colors
        commands: root.commands
        keymap: root.keymap
        browser: root.browser
        spaceColours: root.spaceColours
        iconFontFamily: root.iconFontFamily
        useFavicons: root.useFavicons
        tintFavicons: root.tintFavicons
        backdropSource: shell
        open: root.omnibarOpen
        enabled: root.omnibarOpen

        onDismissed: root.closeOmnibar()
        onCommitted: function (text) {
            root.navigate(text);
        }
    }
}
