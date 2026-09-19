import QtQuick
import QtWebEngine
import Omaweb
import Omaweb.Engine

Item {
    id: root

    property alias currentUrl: webView.url
    property alias pageTitle: webView.title
    property alias pageIconUrl: webView.icon
    property alias loading: webView.loading
    // Chromium keeps a page "recently audible" for a moment after the sound
    // stops, which is what keeps the tab's speaker from flickering through a
    // pause between clips. Muting is the embedder's to set and survives
    // navigation within the view, so the tab keeps its decision across pages.
    readonly property bool pageAudible: webView.recentlyAudible
    // What the page declares about what it is playing, in the shape the shell
    // hands the desktop: "state" as one of "none", "playing" or "paused", and
    // the "title", "artist", "album", "artwork", "canGoNext" and
    // "canGoPrevious" the page named. Empty for a page that has declared
    // nothing yet, which a new document is until its script has run.
    property var pageMediaSession: ({})
    property alias audioMuted: webView.audioMuted
    // Whether this page may start playing without a gesture of its own. It is
    // the shell's decision rather than the engine's default, and the shell
    // allows it: a silent video interrupts nobody. Whether it is heard is a
    // separate answer, given through the muting above, because the engine's own
    // gesture requirement cannot tell the two apart.
    property bool autoplayAllowed: false
    // The process drawing this page, so the shell can say what a retained tab
    // actually costs rather than only that it is running. Zero while the view
    // has no renderer, which is the honest answer for a page that is not up.
    //
    // Chromium announces the process when the page's frame host changes, which
    // for a page opening in a process of its own is before that process has
    // started, and it never announces the process again. So the announcement
    // alone reports zero for every page that is not sharing the view's first
    // process, and the pid is read back from the view again when a load ends,
    // when the process that drew it necessarily exists.
    readonly property int renderProcessPid: root.reportedRenderProcessPid
    property int reportedRenderProcessPid: 0
    function refreshRenderProcessPid() {
        root.reportedRenderProcessPid = webView.renderProcessPid;
    }
    // Whether this page may stop running while nobody is looking at it. The
    // shell decides that; Chromium decides whether it can be done. A frozen
    // page keeps its document, its process and everything it holds, and stops
    // running timers, animations and script, so clearing this continues the
    // page rather than loading it again. Discarding is never asked for: that
    // would take the page away, which is the shell's other policy and reaches
    // an engine as a destroyed adapter.
    property bool pageFrozen: false
    onPageFrozenChanged: root.applyPageLifecycle()

    // Chromium refuses to freeze a page it is still drawing, and refuses
    // anything below Active while the inspector is attached. It reports both as
    // a recommendation of its own, and reports it after the fact rather than
    // when the shell decides, so the decision is applied again every time that
    // recommendation moves.
    function applyPageLifecycle() {
        const frozen = WebEngineView.LifecycleState.Frozen;
        const active = WebEngineView.LifecycleState.Active;
        const wanted = root.pageFrozen && webView.recommendedState !== active ? frozen : active;
        if (webView.lifecycleState !== wanted)
            webView.lifecycleState = wanted;
    }
    property alias canGoBack: webView.canGoBack
    property alias canGoForward: webView.canGoForward
    property string profilePath: ""
    property var sharedProfile: null
    property var permissionController: null
    property var contentBlocker: null
    property var engineContentBlocker: null
    // The Qt cookie policy of the profile this view runs on, told which
    // document the view is showing. Null where no policy is attached, as in a
    // view on a profile of its own.
    property var engineCookiePolicy: null
    readonly property var browserProfile: webView.profile
    readonly property bool pageHasFocus: webView.activeFocus
    readonly property int capabilities: EngineCapabilities.Navigation
                                        | EngineCapabilities.PersistentProfiles
                                        | EngineCapabilities.PrivateProfiles
                                        | EngineCapabilities.ContentBlocking
                                        | EngineCapabilities.KeyboardPageCommands
                                        | EngineCapabilities.DeveloperTools
                                        | EngineCapabilities.RendererRecovery
                                        | EngineCapabilities.PageFind | EngineCapabilities.PageZoom
                                        | EngineCapabilities.Printing
                                        | EngineCapabilities.SiteFullscreen
                                        | EngineCapabilities.InlinePdfViewing
                                        | EngineCapabilities.CertificateDecisions
                                        | EngineCapabilities.ThirdPartyCookieControl | (
                                            EngineBuild.knownExtensions
                                            ? EngineCapabilities.KnownExtensions : 0)
    // Which Space's browsing identity these pages belong to. Handed down with
    // the profile, because it is the profile that decides it: Content blocking
    // keys the Refusal tally by it, and its interception is attached per
    // profile (ADR 0037). Empty in a Private window, whose shared session has
    // no Space of its own.
    property string spaceId: ""
    // What the view shows where no page has painted yet. A page itself sits on
    // the canvas every browser gives it, white or the dark one Chromium draws
    // under `color-scheme: dark`; the theme's colour is shown only from the
    // moment a document is created until it first paints, which is the window
    // in which white would flash through dark chrome.
    property color pageBackgroundColor: "#16151d"
    property bool documentPainted: false
    // The colour a page's own controls are drawn in: the checked box, the
    // selected option, the filled track. Chromium draws them itself and has no
    // idea what the window around them looks like, so the shell says.
    property color pageControlAccent: "transparent"
    property var keyboardNavigationConfiguration: ({})
    property string keyboardNavigationScriptSource: ""
    property bool keyboardNavigationHintModeActive: false
    // Reported by the page in CSS pixels of its main frame and kept here in
    // the view's own, so the shell need not know the zoom. A press in a
    // subframe is not reported: its coordinates are the frame's, and the
    // view cannot place them.
    property rect pressOrigin: Qt.rect(0, 0, 0, 0)
    function recordPressOrigin(report) {
        const scale = webView.zoomFactor;
        root.pressOrigin = Qt.rect(Number(report.x) * scale, Number(report.y) * scale, Math.max(1,
                                                                                                Number(report.width)
                                                                                                * scale), Math.max(
                                       1, Number(report.height) * scale));
    }
    property var editedStateScript: {
        const script = WebEngine.script();
        script.name = "Omaweb edited form state";
        script.injectionPoint = WebEngineScript.DocumentReady;
        script.worldId = WebEngineScript.MainWorld;
        script.runsOnSubFrames = false;
        script.sourceCode = "globalThis.__omawebContentEditableEdited = false;"
                + "document.addEventListener('input', event => {"
                + "if (event.target && event.target.isContentEditable) "
                + "globalThis.__omawebContentEditableEdited = true;" + "}, true);";
        return script;
    }

    // The inspector Chromium supplies, and the palette it is drawn in. The
    // view is the adapter's to build and destroy; where it sits is the shell's,
    // so the shell takes it as a child once it exists.
    property bool developerToolsAttached: false
    property var developerToolsView: null
    property var developerToolsColors: ({})
    // Whether Chromium is holding a node from a context menu. `InspectElement`
    // reads that node and dereferences it without checking first, so asking for
    // it when no menu has been opened takes the whole browser down. A keyboard
    // request never has one, which is exactly the case that has to be caught.
    property bool contextMenuTargetKnown: false
    // The reader asked to point at something before the inspector had loaded.
    property bool elementPickPending: false
    property int pageGeneration: 0
    property string lastContextMediaType: "none"

    // What the reader is looking for in this page, and where the search has
    // reached. One adapter draws one tab, so this is find belonging to a tab
    // without anything keeping a table of tabs: hiding the interface leaves
    // both standing, and only a navigation takes the matches away.
    property string findQuery: ""
    property int findMatchCount: 0
    property int findActiveMatch: 0
    // The page the outstanding search was issued against. Chromium answers a
    // search asynchronously, and the answer can arrive after a navigation has
    // already taken the matches away, so a result is only the current page's
    // if the page has not turned over since the search was asked.
    property int findGeneration: -1

    // How large this tab's page is drawn. The shell owns the value — it is the
    // tab's, and outlives the page in it — and the view is told what it is.
    property alias zoomFactor: webView.zoomFactor

    // Fullscreen the site asked for, which is not the reader asking for the
    // window. The origin is named so the shell can say who took the screen, and
    // the page is told when the reader takes it back.
    property bool siteFullscreenActive: false
    property string siteFullscreenOrigin: ""

    // What the reader pointed at, as plain values rather than as the engine's
    // own request object: a position, the addresses under the pointer, the
    // selection, and whether the target takes typing. Omaweb draws the menu, so
    // the engine's own never appears and nothing about Chromium's menu model
    // crosses this line.
    signal pageContextRequested(var context)
    // The text a page wants shown beside a point of its own, and whether it is
    // raising one or taking it back. The shell draws it, on the same terms as
    // the menu above: the engine reports the ask and keeps no tooltip of its
    // own. A withdrawal carries no text and no point, because the page is
    // saying only that what it raised is over.
    signal pageTooltipRequested(var tooltip)
    signal developerToolsClosed
    signal rendererFailed(string reason)
    signal newTabRequested(var request, url requestedUrl)
    signal auxiliaryWindowRequested(var request, url requestedUrl)
    signal windowCloseRequested
    signal backgroundTabRequested(url requestedUrl)
    signal sitePermissionRequested(string requestId, string origin, string permission)
    signal browserPromptRequested(string requestId, var prompt)
    signal fileSelectionRequested(string requestId, var selection)
    // The page has been rendered, or it has not. Either way the shell hears
    // about it: a print that produced nothing is not a print that quietly
    // didn't happen.
    signal printFinished(string destination, bool succeeded)
    // The reader dealt with this page themselves. What that earns the origin is
    // the shell's to decide and remember; the adapter only reports it.
    signal userActivated
    // The origin whose certificate failed, and whether the reader let the load
    // through anyway. Both are kept because the address trigger has to keep
    // saying so: an exception the reader granted is still a certificate error,
    // and there is nothing else on the page to tell them the check was waived.
    property string certificateErrorOrigin: ""
    // Whether the load now in flight has had a failure reported for it, which
    // is what tells a page reached over a waived certificate from one whose
    // certificate has since been fixed.
    property bool certificateErrorRaisedForLoad: false
    // Whether the last committed load ended in a failure. Chromium draws its
    // own error page in place of the document, and there is no connection to
    // the address to report anything about.
    property bool lastLoadFailed: false
    // What the connection to the page on show is, from what the engine
    // committed and what it reported about the certificate — never from an
    // address the shell parsed for itself.
    readonly property string connectionState: {
        const address = String(webView.url);
        const separator = address.indexOf("://");
        const scheme = separator === -1 ? "" : address.substring(0, separator).toLowerCase();
        if (root.certificateErrorOrigin.length > 0 && root.certificateErrorOrigin
                === root.originLabel(webView.url))
            return "certificate-error";
        // A page that never arrived is not a page reached over an unencrypted
        // connection: there is no connection to report either way.
        if (root.lastLoadFailed)
            return "internal";
        if (scheme !== "http" && scheme !== "https")
            return "internal";
        return scheme === "https" ? "secure" : "insecure";
    }
    // Chromium refuses active mixed content unless the embedder turns the
    // refusal off. Omaweb never does, and reports the setting rather than the
    // intention so the claim is checkable.
    readonly property bool insecureContentBlocked: !webView.settings.allowRunningInsecureContent
    signal certificateErrorRaised(string requestId, var failure)
    // The origin whose data was asked for, what the page managed to empty, and
    // why it could not — one of the three is always the useful one.
    signal pageSiteDataCleared(string origin, var cleared, string error)
    property var pendingCertificateErrors: ({})
    property int nextCertificateErrorId: 0
    // The failures Chromium never offers an override for, whatever else it
    // says about them: a pinned key, an interception it recognised, a
    // revocation, and the transparency and legacy-authority refusals. They are
    // named here because classifying an engine's own error codes is the
    // adapter's work, and the shell's refusal is written in its own terms.
    function fatalCertificateError(type) {
        switch (Number(type)) {
        case WebEngineCertificateError.SslPinnedKeyNotInCertificateChain:
        case WebEngineCertificateError.CertificateKnownInterceptionBlocked:
        case WebEngineCertificateError.CertificateRevoked:
        case WebEngineCertificateError.CertificateTransparencyRequired:
        case WebEngineCertificateError.CertificateSymantecLegacy:
            return true;
        default:
            return false;
        }
    }

    // The reader's answer to one failure. Accepting it lets this load through
    // and nothing else: Chromium is told about this certificate for this host,
    // and the adapter keeps saying the connection is in error.
    // A page emptying its own storage, which is the only per-origin removal
    // there is: engines expose none at the embedding boundary, and everything
    // here is reachable from inside the document. Cookies are not — the ones
    // that matter are unreadable from script — so they stay the Space-wide
    // action's to take.
    //
    // The work is asynchronous and `runJavaScript` does not wait for a promise,
    // so the page reports back through the console the way the rest of this
    // adapter's page reports do, from the world they report from.
    function clearPageSiteData() {
        const address = String(webView.url);
        if (address.length === 0 || address.startsWith("about:")) {
            root.pageSiteDataCleared("", [], "there is no page to clear");
            return;
        }
        webView.runJavaScript(root.reporting(`(async () => {
            const cleared = [];
            const refused = [];
            const attempt = async (name, work) => {
                try { if (await work()) cleared.push(name) }
                catch (error) { refused.push(name) }
            };
            await attempt("local storage", () => {
                const had = localStorage.length > 0 || sessionStorage.length > 0;
                localStorage.clear();
                sessionStorage.clear();
                return had;
            });
            await attempt("databases", async () => {
                if (!indexedDB.databases) throw new Error("no database listing");
                const named = await indexedDB.databases();
                await Promise.all(named.map(one => new Promise(done => {
                    const request = indexedDB.deleteDatabase(one.name);
                    request.onsuccess = request.onerror = request.onblocked = done;
                })));
                return named.length > 0;
            });
            await attempt("caches", async () => {
                if (!window.caches) return false;
                const keys = await caches.keys();
                await Promise.all(keys.map(key => caches.delete(key)));
                return keys.length > 0;
            });
            await attempt("service workers", async () => {
                if (!navigator.serviceWorker) return false;
                const registered = await navigator.serviceWorker.getRegistrations();
                await Promise.all(registered.map(one => one.unregister()));
                return registered.length > 0;
            });
            report('site_data_cleared', {
                origin: location.origin, cleared: cleared, refused: refused
            });
        })()`), WebEngineScript.ApplicationWorld,
                              // The overload with a world needs the callback to pick it.
                              function () {});
    }

    function respondToCertificateError(requestId, accepted) {
        const error = root.pendingCertificateErrors[requestId];
        if (!error)
            return;
        delete root.pendingCertificateErrors[requestId];
        if (accepted)
            error.acceptCertificate();
        else
            error.rejectCertificate();
    }
    property var pendingPermissions: ({})
    property int nextPermissionRequestId: 0
    property var pendingBrowserPrompts: ({})
    property int nextBrowserPromptId: 0
    property bool javaScriptDialogsBlocked: false
    property var pendingFileSelections: ({})
    property var externalProtocolOrigins: ({})

    // Chromium's own permission numbers, in the words the shell's policy is
    // written in. Translating an engine's events into the common contract is
    // the adapter's work: the shell decides what may be remembered about a
    // capability, and it cannot do that about an integer whose meaning is
    // Chromium's. A capability with no name here is one this build has no
    // policy for, and the shell refuses what it cannot name.
    function permissionName(permissionType) {
        switch (permissionType) {
        case WebEnginePermission.PermissionType.MediaAudioCapture:
            return "microphone";
        case WebEnginePermission.PermissionType.MediaVideoCapture:
            return "camera";
        case WebEnginePermission.PermissionType.MediaAudioVideoCapture:
            return "camera-and-microphone";
            // Chromium tells the two desktop captures apart by whether the sound
            // comes with the screen. Omaweb does not: either one hands over
            // whatever is on the screen at that instant, and is asked every time.
        case WebEnginePermission.PermissionType.DesktopVideoCapture:
        case WebEnginePermission.PermissionType.DesktopAudioVideoCapture:
            return "screen-sharing";
        case WebEnginePermission.PermissionType.MouseLock:
            return "pointer-lock";
        case WebEnginePermission.PermissionType.Notifications:
            return "notifications";
        case WebEnginePermission.PermissionType.Geolocation:
            return "geolocation";
        case WebEnginePermission.PermissionType.ClipboardReadWrite:
            return "clipboard-read";
        case WebEnginePermission.PermissionType.LocalFontsAccess:
            return "local-fonts";
        default:
            return "";
        }
    }

    function respondToPermission(requestId, decision) {
        const request = pendingPermissions[requestId];
        if (!request)
            return;
        delete pendingPermissions[requestId];
        if (decision === 1 || decision === 2)
            request.grant();
        else
            request.deny();
    }

    function respondToBrowserPrompt(requestId, accepted, response) {
        const pending = root.pendingBrowserPrompts[requestId];
        if (!pending)
            return;
        delete root.pendingBrowserPrompts[requestId];
        if (pending.kind.startsWith("javascript-") && response.stopPrompts === true)
            root.javaScriptDialogsBlocked = true;
        if (pending.kind === "external-protocol") {
            if (accepted) {
                if (response.remember === true && root.permissionController)
                    root.permissionController.rememberExternalProtocolDecision(pending.origin,
                                                                               pending.scheme);
                ExternalProtocolHandler.open(pending.destination);
            }
        } else if (!accepted) {
            pending.request.dialogReject();
        } else if (pending.kind === "http-authentication") {
            pending.request.dialogAccept(String(response.user || ""), String(response.password
                                                                             || ""));
        } else {
            pending.request.dialogAccept(String(response.text || ""));
        }
    }

    function originAddress(url) {
        const match = String(url).match(/^([a-z][a-z0-9+.-]*:\/\/[^/]+)/i);
        return match ? match[1] : String(url);
    }

    function requestExternalProtocol(destination, mainFrame) {
        const address = String(destination);
        const separator = address.indexOf(":");
        const scheme = separator > 0 ? address.substring(0, separator).toLowerCase() : "";
        const rememberedOrigin = root.externalProtocolOrigins[address];
        delete root.externalProtocolOrigins[address];
        const origin = mainFrame ? root.originAddress(webView.url) : String(rememberedOrigin || "");
        if (scheme.length === 0)
            return;
        if (root.permissionController && origin.length > 0
                && root.permissionController.externalProtocolAllowed(origin, scheme)) {
            ExternalProtocolHandler.open(address);
            return;
        }
        const requestId = String(++root.nextBrowserPromptId);
        const application = ExternalProtocolHandler.applicationName(address);
        const displayOrigin = origin.length > 0 ? origin : "Unknown embedded origin";
        root.pendingBrowserPrompts[requestId] = {
            "kind": "external-protocol",
            "origin": origin,
            "scheme": scheme,
            "destination": address
        };
        root.browserPromptRequested(requestId, {
                                        "kind": "external-protocol",
                                        "application": application,
                                        "scheme": scheme,
                                        "origin": displayOrigin,
                                        "destination": address,
                                        "rememberable": origin.length > 0,
                                        "message": "Open " + application + "?",
                                        "detail": scheme + " · " + displayOrigin + " · " + address
                                    });
    }

    function respondToFileSelection(requestId, files) {
        const request = root.pendingFileSelections[requestId];
        if (!request)
            return;
        delete root.pendingFileSelections[requestId];
        if (files.length === 0) {
            request.dialogReject();
            return;
        }
        const paths = [];
        for (let index = 0; index < files.length; ++index)
            paths.push(root.localPath(files[index]));
        request.dialogAccept(paths);
    }

    function localPath(fileUrl) {
        let path = decodeURIComponent(String(fileUrl).replace(/^file:\/\//, ""));
        if (Qt.platform.os === "windows" && path.startsWith("/"))
            path = path.substring(1);
        return path;
    }

    function performPageContextAction(action, destination) {
        if (action === "copy-image") {
            webView.triggerWebAction(WebEngineView.CopyImageToClipboard);
            return;
        }
        const path = root.localPath(destination);
        if (path.length === 0)
            return;
        const profile = root.resolvedProfile();
        if (!profile || profile.preparedDownloadPath === undefined)
            return;
        profile.preparedDownloadPath = path;
        if (action === "save-link")
            webView.triggerWebAction(WebEngineView.DownloadLinkToDisk);
        else if (action === "save-media") {
            const webAction = root.lastContextMediaType === "image"
                  ? WebEngineView.DownloadImageToDisk : WebEngineView.DownloadMediaToDisk;
            webView.triggerWebAction(webAction);
        }
    }

    // Chromium's inspector is a webpage of its own, and it runs in the profile
    // of the page it inspects: a Private tab's inspector keeps its state in the
    // temporary Private profile and loses it with the session, as the page
    // does. It is built when it is first asked for rather than with the tab —
    // one inspector costs a second renderer process.
    property Component developerToolsComponent: Component {
        WebEngineView {
            objectName: "qtDeveloperToolsView"
            profile: root.resolvedProfile()
            backgroundColor: root.developerToolsBackgroundColor()
            userScripts.collection: [root.developerToolsThemeScript]

            // The frontend's own close button asks its window to close, which
            // is the reader saying they are finished with it.
            onWindowCloseRequested: root.developerToolsClosed()

            onLoadingChanged: function (loadRequest) {
                if (loadRequest.status !== WebEngineView.LoadSucceededStatus)
                    return;
                root.applyDeveloperToolsTheme();
                root.pickElement();
            }
        }
    }

    function attachDeveloperTools() {
        if (root.developerToolsAttached)
            return;
        const view = root.developerToolsComponent.createObject(root);
        if (!view)
            return;
        root.developerToolsView = view;
        webView.devToolsView = view;
        root.developerToolsAttached = true;
    }

    function detachDeveloperTools() {
        if (!root.developerToolsAttached)
            return;
        root.developerToolsAttached = false;
        root.elementPickPending = false;
        webView.devToolsView = null;
        const view = root.developerToolsView;
        root.developerToolsView = null;
        // The shell has taken the view as a child by now, so nothing else
        // would ever take it away.
        if (view)
            view.destroy();
    }

    // Two ways to name what to inspect, and the reader has already chosen
    // between them. Opening a context menu over a node is Chromium naming it,
    // and `InspectElement` is the action that reads it back. Asking from the
    // keyboard names nothing, so the inspector's own element picker is what
    // answers: the reader points at what they meant, which is what the key does
    // in every other browser.
    function inspectElement() {
        const alreadyOpen = root.developerToolsAttached;
        root.attachDeveloperTools();
        if (!root.developerToolsAttached)
            return;
        if (!root.contextMenuTargetKnown) {
            root.elementPickPending = true;
            if (alreadyOpen)
                root.pickElement();
            return;
        }
        if (alreadyOpen) {
            webView.triggerWebAction(WebEngineView.InspectElement);
            return;
        }
        // The inspector has to exist before the page can be told to reveal a
        // node in it.
        Qt.callLater(function () {
            if (root.developerToolsAttached && root.contextMenuTargetKnown)
                webView.triggerWebAction(WebEngineView.InspectElement);
        });
    }

    // The frontend's own picker, asked for the way its host asks for it. It
    // waits for the frontend to finish loading, because a page that has not run
    // its scripts has no picker to enter.
    function pickElement() {
        if (!root.elementPickPending || !root.developerToolsView)
            return;
        if (root.developerToolsView.loading)
            return;
        root.elementPickPending = false;
        root.developerToolsView.runJavaScript(
                    "globalThis.DevToolsAPI && globalThis.DevToolsAPI.enterInspectElementMode();");
    }

    // The engine's own enumeration never leaves the adapter; the shell reads a
    // name, and an engine with a kind Omaweb has no name for reports none.
    function mediaTypeName(mediaType) {
        switch (mediaType) {
        case ContextMenuRequest.MediaTypeImage:
            return "image";
        case ContextMenuRequest.MediaTypeVideo:
            return "video";
        case ContextMenuRequest.MediaTypeAudio:
            return "audio";
        case ContextMenuRequest.MediaTypeCanvas:
            return "canvas";
        case ContextMenuRequest.MediaTypeFile:
            return "file";
        }
        return "none";
    }

    function requestContextFromFrame(frame, generation) {
        frame.runJavaScript(`(() => {
            if (!document.hasFocus()) return null;
            const target = document.activeElement || document.body;
            const link = target && target.closest ? target.closest('a[href]') : null;
            const media = target && target.closest
                ? target.closest('img[src],video[src],audio[src]') : null;
            const rect = target && target.getBoundingClientRect
                ? target.getBoundingClientRect() : {left: innerWidth / 2, top: innerHeight / 2,
                    width: 0, height: 0};
            return {
                x: Math.round(rect.left + rect.width / 2),
                y: Math.round(rect.top + rect.height / 2),
                selectedText: String(getSelection() || ''),
                linkText: link ? String(link.textContent || '') : '',
                linkUrl: link ? link.href : '',
                mediaUrl: media ? (media.currentSrc || media.src || '') : '',
                mediaType: media ? media.tagName.toLowerCase().replace('img', 'image') : 'none',
                editable: Boolean(target && (target.isContentEditable
                    || /^(INPUT|TEXTAREA|SELECT)$/.test(target.tagName)))
            };
        })()`, function (context) {
    if (!context || generation !== root.pageGeneration)
        return;
    context.pageGeneration = generation;
    root.pageContextRequested(context);
});
        for (let index = 0; index < frame.children.length; ++index)
            root.requestContextFromFrame(frame.children[index], generation);
    }

    // Qt runs the query inside each frame, so a focused element keeps its own
    // origin's DOM even when the embedding page cannot read it.
    function requestPageContextMenu() {
        root.contextMenuTargetKnown = false;
        root.requestContextFromFrame(webView.mainFrame, root.pageGeneration);
    }

    function goBack() {
        webView.goBack();
    }
    function goForward() {
        webView.goForward();
    }
    function focusPage() {
        webView.forceActiveFocus();
    }
    function reloadPage() {
        webView.reload();
    }
    // Reading the page again and reading it again from the network are two
    // different asks, and so is stopping: a stopped load leaves the page and
    // everything typed into it exactly where it was.
    function reloadPageBypassingCache() {
        webView.triggerWebAction(WebEngineView.ReloadAndBypassCache);
    }
    function stopLoading() {
        webView.stop();
    }

    // The matches, and not the query: a navigation invalidates where the search
    // had reached, but not what the reader was looking for.
    function forgetFindMatches() {
        root.findMatchCount = 0;
        root.findActiveMatch = 0;
    }

    function findText(query, forward) {
        root.findQuery = String(query);
        root.findGeneration = root.pageGeneration;
        if (root.findQuery.length === 0) {
            root.forgetFindMatches();
            webView.findText("");
            return;
        }
        webView.findText(root.findQuery, forward ? 0 : WebEngineView.FindBackward);
    }

    function clearFind() {
        root.findQuery = "";
        root.findGeneration = root.pageGeneration;
        root.forgetFindMatches();
        webView.findText("");
    }

    function setZoomFactor(factor) {
        const wanted = Number(factor);
        if (!(wanted > 0))
            return;
        webView.zoomFactor = wanted;
    }

    function printPage(destination) {
        const path = String(destination);
        if (path.length === 0) {
            root.printFinished("", false);
            return;
        }
        webView.printToPdf(path);
    }

    // Who a request came from, as the reader would recognise them. The engine
    // names the origin as a url, which JavaScript sees as a plain string with
    // no host to read off it, so the authority is taken from the string — the
    // host, and the port where there is one, because two development servers
    // on one host are two different sites.
    function originLabel(origin) {
        const address = String(origin);
        const scheme = address.indexOf("://");
        const authority = (scheme === -1 ? address : address.substring(scheme + 3)).split("/")[0];
        return authority.length > 0 ? authority : address;
    }

    function exitSiteFullscreen() {
        if (!root.siteFullscreenActive)
            return;
        root.siteFullscreenActive = false;
        root.siteFullscreenOrigin = "";
        webView.fullScreenCancelled();
    }
    // The desktop's media key, handed to the page. One of "play", "pause",
    // "playpause", "stop", "next" or "previous".
    function invokeMediaAction(command) {
        if (!webView)
            return;
        webView.runJavaScript("globalThis.__omawebMediaSession && "
                              + "globalThis.__omawebMediaSession.invoke(" + JSON.stringify(String(
                                                                                               command))
                              + ");");
    }

    function configureKeyboardNavigation(configuration) {
        keyboardNavigationHintModeActive = false;
        keyboardNavigationConfiguration = configuration;
        applyKeyboardNavigationConfiguration();
    }
    function applyKeyboardNavigationConfiguration() {
        if (!webView || !keyboardNavigationConfiguration.version)
            return;
        webView.runJavaScript("globalThis.__omawebKeyboardNavigation && "
                              + "globalThis.__omawebKeyboardNavigation.configure(" + JSON.stringify(
                                  keyboardNavigationConfiguration) + ");");
    }
    // Content blocking owns the Refusal tally for the document on show, so
    // this view says which document that is and keeps no count of its own.
    // Said again when the address changes without a load of its own: a load
    // that resolves to a redirect arrives somewhere else and the tally goes
    // with it, while a jump inside the document is the same document and
    // Content blocking carries its tally on.
    function announcePage(pageAddress) {
        if (root.contentBlocker)
            root.contentBlocker.showPage(root, root.spaceId, pageAddress, root.pageGeneration);
    }
    // The address the load on show set out from. The engine names a
    // document's first party by this address rather than by the one the load
    // arrived at, so the cookie policy is told both (ADR 0046). The address
    // the load asks for at its start, kept while it redirects and while the
    // document moves inside itself, and replaced by the next load's.
    property url loadSetOutFrom: root.currentUrl
    // Said when a load starts, and again for each redirect the load resolves
    // to, with the address it is arriving at.
    function announceDocument(arrivedAt) {
        if (root.engineCookiePolicy)
            root.engineCookiePolicy.showDocument(root, root.loadSetOutFrom, arrivedAt);
    }
    property bool cosmeticRulesInjected: false
    property int cosmeticRuleGeneration: 0
    property int cosmeticSurveyGeneration: 0
    // Whether the main frame has surveyed itself since the load started. The
    // engine hands a frame the view's scripts when the frame is created, and
    // the first main frame of a renderer process it has just started is
    // sometimes handed them only once its document has parsed, so the survey
    // script in it has run in nothing: seen in about one first navigation of
    // a view in three here, a cross-site redirect into a cold process, with
    // the subframe of the same document surveyed. An error page and the engine's
    // own viewers run none either. A document that has not reported by load
    // success is asked.
    property bool documentSurveyed: false
    // Whether any frame of this document has been given generic rules, so a
    // frame that is no longer to carry them can be cleared without asking
    // every frame every time. Set at the first injection and reset by the
    // next document.
    property bool genericCosmeticRulesInjected: false
    readonly property string controlAccentSheetId: "__omaweb_control_accent"

    // `accent-color` is inherited, so naming it once on the root element
    // reaches every control on the page.
    //
    // `:where()` is what keeps this a fallback rather than an override. An
    // adopted sheet is ordered after every sheet the document carries of its
    // own, so it would win every tie on order. `:where(html)` has no
    // specificity at all, so a page naming an accent — on `html`, on `:root`,
    // or on the control itself — outranks this whatever the order turns out to
    // be.
    //
    // The colour is written out as components rather than as Qt's hex, which
    // puts alpha at the front where CSS reads it last.
    readonly property string controlAccentStyleSheet: {
        const accent = root.pageControlAccent;
        if (!accent || accent.a === 0)
            return "";
        const channel = value => Math.round(value * 255);
        const rgb = channel(accent.r) + " " + channel(accent.g) + " " + channel(accent.b);
        return ":where(html) { accent-color: rgb(" + rgb + "); }";
    }

    property var controlAccentScript: {
        const script = WebEngine.script();
        script.name = "Omaweb control accent";
        // Before the page paints, so no control is ever seen in the engine's
        // blue first. Subframes draw controls of their own and are their own
        // documents, so they need it too.
        script.injectionPoint = WebEngineScript.DocumentCreation;
        script.worldId = WebEngineScript.MainWorld;
        script.runsOnSubFrames = true;
        script.sourceCode = root.styleSheetSnippet(root.controlAccentSheetId,
                                                   root.controlAccentStyleSheet);
        return script;
    }

    onControlAccentScriptChanged: root.installUserScript(root.controlAccentScript.name,
                                                         root.controlAccentScript)

    // A theme the reader changes has to reach what is already open. The script
    // above answers for the next load; this answers for the page on show, and
    // the snippet replaces the rules in the sheet the document already adopted
    // rather than adopting a second one.
    onControlAccentStyleSheetChanged: {
        if (!root.completed)
            return;
        webView.runJavaScript(root.styleSheetSnippet(root.controlAccentSheetId,
                                                     root.controlAccentStyleSheet));
    }

    // The collection is written whole once, by its binding, while the view is
    // built, and edited a script at a time from then on. The engine takes a
    // whole assignment in as a clearing and one insertion per script, each a
    // message of its own to the renderer, and a document created between the
    // clearing and the last insertion runs none of the view's scripts. It
    // also keeps one observer of the view per assignment, each resending
    // every script to every new frame. Editing takes one script out and puts
    // its successor in, so the rest are never gone, and a script equal to the
    // one installed is left alone.
    //
    // The scripts' own change handlers are also fired while the component is
    // being built, for any sheet a creation property gives a value to, when
    // the collection's binding has not been evaluated yet. Until completion
    // the binding answers, and nothing here.
    property bool completed: false

    function installUserScript(name, script) {
        if (root.completed)
            root.replaceScript(webView.userScripts, name, script);
    }
    // One script by name, in the view's collection or the Engine profile's:
    // whatever carries the name goes, and `script`, if any, takes its place.
    function replaceScript(collection, name, script) {
        if (script && collection.contains(script))
            return;
        for (const installed of collection.find(name))
            collection.remove(installed);
        if (script)
            collection.insert(script);
    }

    // ---- the palette a page may follow ---------------------------------------
    //
    // A page that says so is given the shell's palette as custom properties on
    // its root element, so a site can be drawn in the reader's own theme rather
    // than in one it guessed at. Saying so is a `<meta name="omaweb-palette">`
    // in the document: the palette is the reader's to reveal, and which theme
    // they run is a fact about them a page has no other way to learn, so a
    // page learns nothing unless it asks in its markup where the reader can
    // see the asking.
    //
    // `:where(html)` as the accent above: no specificity, so the page's own
    // definition of any of these names outranks the shell's.
    readonly property string pagePaletteSheetId: "__omaweb_palette"
    property var pagePalette: null

    readonly property string pagePaletteStyleSheet: {
        const palette = root.pagePalette;
        if (!palette)
            return "";
        const rgb = colour => Math.round(colour.r * 255) + " " + Math.round(colour.g * 255) + " "
                              + Math.round(colour.b * 255);
        // The shell's palette names its colours as strings; a colour either way.
        let rules = "";
        for (const role of ["bg", "sidebar", "fg", "accent", "urgent", "muted"]) {
            if (!palette[role])
                continue;
            const colour = Qt.color(palette[role]);
            if (colour.valid && colour.a > 0)
                rules += "--omaweb-" + role + ": rgb(" + rgb(colour) + "); ";
        }
        return rules.length === 0 ? "" : ":where(html) { " + rules + "}";
    }

    // The sheet is adopted the moment the meta is parsed, which is before the
    // body exists, so the page never paints in a palette it did not want. A
    // document keeps nothing of the palette while nobody has asked, and the
    // watch ends with the parse: the ask is markup, so a page that has not
    // made it by then is one that is not making it.
    function pagePaletteSnippet(css) {
        return "(() => {" + "const asked = () => document.head"
                + " && document.head.querySelector('meta[name=\"omaweb-palette\"]') !== null;"
                + "const apply = () => " + root.styleSheetSnippet(root.pagePaletteSheetId, css)
                + ";" + "if (asked()) { apply(); return; }"
                + "if (document.readyState !== 'loading') return;"
                + "const watch = new MutationObserver(() => {" + "if (!asked()) return;"
                + "watch.disconnect();" + "apply();" + "});"
                + "watch.observe(document, { childList: true, subtree: true });"
                + "document.addEventListener('DOMContentLoaded', () => watch.disconnect(),"
                + " { once: true });" + "})()";
    }

    property var pagePaletteScript: {
        const script = WebEngine.script();
        script.name = "Omaweb page palette";
        script.injectionPoint = WebEngineScript.DocumentCreation;
        script.worldId = WebEngineScript.MainWorld;
        script.runsOnSubFrames = true;
        script.sourceCode = root.pagePaletteSnippet(root.pagePaletteStyleSheet);
        return script;
    }

    onPagePaletteScriptChanged: root.installUserScript(root.pagePaletteScript.name,
                                                       root.pagePaletteScript)

    onPagePaletteStyleSheetChanged: {
        if (!root.completed)
            return;
        webView.runJavaScript(root.pagePaletteSnippet(root.pagePaletteStyleSheet));
    }

    // ---- the page's own scrollbar ------------------------------------------
    //
    // The engine's scrollbar answers to nothing the theme says, so Omaweb hides
    // the one the document itself scrolls in and draws that bar for the shell
    // to place. What is hidden is only the viewport's: `scrollbar-width` on the
    // root element reaches the document's own scroller and leaves every box
    // inside the page scrolling in a bar of its own, which stays the engine's
    // to draw and is only coloured. Hiding all of them would put every
    // `overflow: auto` box on the page in Omaweb's hands.
    //
    // `:where(html)` for the same reason the accent uses it: no specificity, so
    // a page that names either property outranks Omaweb and keeps the bar it
    // drew for itself.
    readonly property string pageScrollbarSheetId: "__omaweb_scrollbars"
    property color pageScrollbarThumb: "transparent"
    property color pageScrollbarTrack: "transparent"

    readonly property string pageScrollbarStyleSheet: {
        const thumb = root.pageScrollbarThumb;
        const track = root.pageScrollbarTrack;
        if (!thumb || thumb.a === 0 || !track || track.a === 0)
            return "";
        const rgb = colour => Math.round(colour.r * 255) + " " + Math.round(colour.g * 255) + " "
                              + Math.round(colour.b * 255);
        return ":where(html) { scrollbar-width: none; scrollbar-color: rgb(" + rgb(thumb)
                + ") rgb(" + rgb(track) + "); }";
    }

    property var pageScrollbarScript: {
        const script = WebEngine.script();
        script.name = "Omaweb page scrollbar";
        script.injectionPoint = WebEngineScript.DocumentCreation;
        script.worldId = WebEngineScript.MainWorld;
        // The document's own scroller only. A subframe is its own document with
        // its own scroller, and the shell draws one bar for the page it is
        // showing rather than one per frame.
        script.runsOnSubFrames = false;
        script.sourceCode = root.styleSheetSnippet(root.pageScrollbarSheetId,
                                                   root.pageScrollbarStyleSheet);
        return script;
    }

    property var pageScrollReportScript: {
        const script = WebEngine.script();
        script.name = "Omaweb page scroll";
        script.injectionPoint = WebEngineScript.DocumentCreation;
        script.worldId = WebEngineScript.ApplicationWorld;
        script.runsOnSubFrames = false;
        script.sourceCode = root.pageScrollReportSource;
        return script;
    }

    onPageScrollbarScriptChanged: root.installUserScript(root.pageScrollbarScript.name,
                                                         root.pageScrollbarScript)

    onPageScrollbarStyleSheetChanged: {
        if (!root.completed)
            return;
        webView.runJavaScript(root.styleSheetSnippet(root.pageScrollbarSheetId,
                                                     root.pageScrollbarStyleSheet));
    }

    // How a page reports a fact to the shell. The engine hands out no channel
    // of its own, so the report goes over the console, and the console is the
    // page's: anything written there in the page's world the page can read,
    // patch, or write itself. So the scripts that report run in the
    // application world, where the page's `console` is not theirs, and the
    // one that has to run in the page's world binds the console before the
    // page runs. Each report is sealed with a name drawn once per view that
    // the page never sees, so a page shouting the shell's prefix into its own
    // console is not believed. The level is the one an inspector hides by
    // default, so a reader debugging their own page does not read the shell's
    // traffic between their own lines. The keyboard navigation script's hint
    // mode report is the one line still sent unsealed, from the shared script
    // in `src/engine/api`.
    readonly property string reportHead: "__omaweb_"
    readonly property string reportToken: {
        let drawn = "";
        for (let part = 0; part < 3; ++part)
            drawn += Math.random().toString(36).slice(2);
        return drawn;
    }
    readonly property string reportSeal: "__" + root.reportToken

    // `report` is declared for the source that follows, in a scope of its own:
    // scripts in one world share a global scope, and a second declaration
    // there would be a syntax error that took the script with it.
    function reportSnippet() {
        return "const report = (() => {" + "const say = console.debug.bind(console);"
                + "const head = " + JSON.stringify(root.reportHead) + ";" + "const seal = "
                + JSON.stringify(root.reportSeal) + ";"
                + "return (channel, payload) => say(head + channel + seal"
                + " + (payload === undefined ? '' : JSON.stringify(payload)));" + "})();";
    }

    function reporting(source) {
        return "(() => {" + root.reportSnippet() + source + "})();";
    }

    // The report a console message carries, or null for a line that is the
    // page's own.
    function pageReport(message) {
        if (!message.startsWith(root.reportHead))
            return null;
        const end = message.indexOf(root.reportSeal, root.reportHead.length);
        if (end < 0)
            return null;
        return {
            "channel": message.substring(root.reportHead.length, end),
            "body": message.substring(end + root.reportSeal.length)
        };
    }

    // Where the page stands in its own length, reported by the document rather
    // than measured from outside: the engine draws the page and only the page
    // knows how far it runs. Zero until a document says otherwise, which is the
    // honest answer for a view with nothing in it.
    property real pageScrollOffset: 0
    property real pageScrollLength: 0
    property real pageViewportLength: 0

    // Measured on the page's own scroll and resize, and once more on a timer
    // slow enough to cost nothing: a page that changes its own length without
    // scrolling or resizing — anything that loads in below the fold — would
    // otherwise leave the bar drawn for a length the document no longer has.
    // Reported only when the measurement moved, so the timer costs a page
    // that stands still nothing but the measuring.
    readonly property string pageScrollReportSource: "(() => {"
                                                     + "if (globalThis.__omawebScrollReport) return;"
                                                     + root.reportSnippet() + "let last = '';"
                                                     + "const send = () => {"
                                                     + "const root = document.documentElement;"
                                                     + "if (!root) return;" + "const measured = {"
                                                     + "offset: window.scrollY, length: root.scrollHeight, viewport: window.innerHeight};"
                                                     + "const encoded = JSON.stringify(measured);"
                                                     + "if (encoded === last) return;"
                                                     + "last = encoded;"
                                                     + "report('page_scroll', measured);" + "};"
                                                     + "globalThis.__omawebScrollReport = send;"
                                                     + "addEventListener('scroll', send, { passive: true });"
                                                     + "addEventListener('resize', send, { passive: true });"
                                                     + "addEventListener('load', send);"
                                                     + "setInterval(send, 1000);" + "send();"
                                                     + "})()"

    // The shell drives the page from the bar it drew. Asked for in the
    // document's own units, which are what it reported.
    function scrollPageTo(offset) {
        webView.runJavaScript("window.scrollTo(0," + Math.round(offset) + ")");
    }

    function readPageScroll(text) {
        try {
            const report = JSON.parse(text);
            root.pageScrollOffset = Number(report.offset) || 0;
            root.pageScrollLength = Number(report.length) || 0;
            root.pageViewportLength = Number(report.viewport) || 0;
        } catch (error) {
            root.pageScrollOffset = 0;
            root.pageScrollLength = 0;
            root.pageViewportLength = 0;
        }
    }

    // A page that goes away takes its measurements with it, so the bar is not
    // drawn for the length of the document before it while the next one loads.
    function forgetPageScroll() {
        root.pageScrollOffset = 0;
        root.pageScrollLength = 0;
        root.pageViewportLength = 0;
    }

    readonly property string cosmeticSheetId: "__omaweb_content_blocking"
    readonly property string genericCosmeticSheetId: "__omaweb_content_blocking_generic"

    // The browser's own styling is not the page's inline style, but a <style>
    // element carrying it is indistinguishable from one: a page that sends
    // `default-src 'self'` refuses the element's content, and the scrollbar
    // stays drawn twice, the controls stay Chromium blue and the hiding rules
    // hide nothing. So the sheet is constructed rather than written into the
    // document, which is the same route an extension takes and is outside what
    // a page's policy governs.
    //
    // A constructed sheet also needs no element to hang from, so there is
    // nothing to wait for: the document exists before the parser has produced
    // even an <html> element, which is where this runs. Re-application into a
    // document that is already open finds the sheet it adopted before and
    // replaces the rules in it, rather than adopting a second one.
    //
    // The sheet is found by a mark on the sheet itself because a document
    // keeps no names for the sheets it adopts. An empty sheet is not worth
    // adopting, so nothing is adopted until there are rules to carry, and a
    // page that drops the sheet from the list gets it back on the next
    // application. The rules are written every time rather than only when they
    // differ from what was sent last: what a sheet carries is the page's to
    // change, and the value last sent is no answer to what it carries now.
    function styleSheetSnippet(sheetId, css) {
        return root.sheetSnippet(sheetId, css, "if (css.length === 0) return;",
                                 "sheet.replaceSync(css);");
    }

    // A late survey adds to what the sheet carries rather than replacing it:
    // what the first survey hid stays hidden. The rules are parsed apart
    // first, because a sheet takes one rule per insertion.
    function appendStyleSheetSnippet(sheetId, css) {
        return root.sheetSnippet(sheetId, css, "", "const parsed = new CSSStyleSheet();"
                                 + "parsed.replaceSync(css);"
                                 + "for (const rule of parsed.cssRules) "
                                 + "sheet.insertRule(rule.cssText, sheet.cssRules.length);");
    }

    // The sheet found or adopted, `withoutSheet` run before one would be
    // adopted, and `withSheet` run on it.
    function sheetSnippet(sheetId, css, withoutSheet, withSheet) {
        return "(() => {" + "const id = " + JSON.stringify(sheetId) + ";" + "const css = "
                + JSON.stringify(css) + ";" + "const adopted = document.adoptedStyleSheets;"
                + "let sheet = adopted.find(candidate => candidate.omawebSheetId === id);"
                + "if (!sheet) {" + withoutSheet + "sheet = new CSSStyleSheet();"
                + "sheet.omawebSheetId = id;"
                + "document.adoptedStyleSheets = [...adopted, sheet];" + "}" + withSheet + "})()";
    }

    // A scriptlet is a function from the vendored uBlock Origin library that a
    // `##+js(...)` rule names and supplies arguments for; the engine returns
    // the library's source for it together with the call. It runs in the
    // page's own world because that is the point — the code it neutralises is
    // the page's — but in a scope of its own, so the names it declares are not
    // the page's to see.
    //
    // `scriptletGlobals` is the one name the library expects its host to
    // supply: uBlock Origin passes its own extension's origin, a logging
    // channel, and a cache of the sandbox it builds. Omaweb passes an empty
    // object, which is what says "no web-accessible resources, no logging
    // channel" — the library reads it defensively and builds the rest itself.
    function scriptletSnippet(source) {
        if (source.length === 0)
            return "";
        return "(() => {\nconst scriptletGlobals = {};\n" + source + "\n})();\n";
    }

    // Hiding rules have to be in the document before its own markup renders,
    // or the ads they cover appear and then vanish; a scriptlet has to be
    // there before the page's first script, or the anti-adblock check it
    // neutralises has already run. Document creation is the only injection
    // point early enough for either, and the script has to be rebuilt for each
    // navigation because both depend on the host being loaded.
    //
    // It is installed on the Engine profile, not on the view, and matched to
    // its origin by the metadata header the engine reads (ADR 0048): a
    // navigation to another site creates its frame before the view hears of
    // the navigation, and a frame is handed the view's scripts only as it is
    // created, whereas the Engine profile's reach every renderer process
    // ahead of any document. Installed at the navigation request, which is
    // raised for a redirect with the address the load is arriving at and for
    // a reload the load-started report is not raised for. The origins
    // installed longest ago go once there are more than the limit; one left
    // stale by a rule or site change is replaced by the next navigation to
    // it, before any document there is created.
    readonly property string blockingScriptNamePrefix: "Omaweb content blocking "
    readonly property int blockingScriptLimit: 16
    // The pattern the engine matches against the document's address, which
    // it spells without the scheme's default port.
    function blockingScriptPattern(url) {
        return root.originAddress(url).replace(/^(http:\/\/.*):80$/, "$1").replace(
                    /^(https:\/\/.*):443$/, "$1") + "/*";
    }
    function installBlockingScript(url) {
        if (!contentBlocker)
            return;
        const css = contentBlocker.cosmeticStyleSheet(url);
        const scriptlets = contentBlocker.scriptletSource(url);
        // The stylesheet goes first: hiding what the page is about to render
        // does not depend on a scriptlet, and a scriptlet that throws must not
        // take the hiding with it.
        const source = (css.length > 0 ? root.styleSheetSnippet(root.cosmeticSheetId, css) + ";\n" :
                                         "") + root.scriptletSnippet(scriptlets);
        const pattern = root.blockingScriptPattern(url);
        const name = root.blockingScriptNamePrefix + pattern;
        const scripts = webView.profile.userScripts;
        let script = null;
        if (source.length > 0) {
            script = WebEngine.script();
            script.name = name;
            script.injectionPoint = WebEngineScript.DocumentCreation;
            script.worldId = WebEngineScript.MainWorld;
            script.runsOnSubFrames = false;
            script.sourceCode = "// ==UserScript==\n// @include " + pattern
                    + "\n// ==/UserScript==\n" + source;
        }
        root.replaceScript(scripts, name, script);
        const blocking = scripts.collection.filter(installed => installed.name.startsWith(
                                                                    root.blockingScriptNamePrefix));
        while (blocking.length > root.blockingScriptLimit)
            scripts.remove(blocking.shift());
        // The document about to be created carries whatever this script adds
        // and nothing else, so what the last one had is no longer there.
        root.cosmeticRulesInjected = css.length > 0;
    }

    // Re-application into a document that is already open, for a rule set or a
    // per-site decision that changed under it. A fresh load takes the document
    // creation path above instead. Only the stylesheet is re-applied: a
    // scriptlet that missed the page's own scripts has nothing left to
    // intercept, and one already run cannot be taken back, so both directions
    // wait for the next navigation.
    function applyCosmeticRules() {
        if (!contentBlocker || loading)
            return;
        const css = contentBlocker.cosmeticStyleSheet(currentUrl);
        // With no cosmetic rules for this site there is nothing to add and, if
        // nothing was ever added, nothing to clear either — so skip the script.
        if (css.length === 0 && !cosmeticRulesInjected)
            return;
        cosmeticRulesInjected = css.length > 0;
        webView.runJavaScript(root.styleSheetSnippet(root.cosmeticSheetId, css));
    }

    // The generic rules are the ones written against no particular site, and
    // sending all of them cost a 617 KB stylesheet on every page. Each frame
    // reports the classes and ids it actually carries, and only the generic
    // rules those could trigger come back. A frame with a $generichide
    // exception is answered with nothing and told to stop reporting.
    //
    // A frame surveys itself as soon as its DOM is parsed, from a script of
    // its own, rather than being asked once its load is over: a detection
    // page inserts its bait during the load and measures it as soon as the
    // bait's own images have settled, which is before the load event, and a
    // survey that waits for the load loses to it. The survey goes on
    // watching the document afterwards, because an ad slot a script fills in
    // later carries a class no survey has seen. The watch reports only names
    // the frame has not been asked about, and stops after the limit below.
    // The first new names go out on the next task, for the same detection
    // page; whatever appears within the interval after a report is held for
    // one more, so a page that churns its DOM pays at most one round trip per
    // interval and a page that adds nothing new pays nothing. The reports
    // come back as page reports do, and each names the frame's address,
    // because a report says nothing about which frame spoke; two frames at
    // one address get the same answer, which is the answer either would have
    // got.
    //
    // The survey and its watch run in the application world, as the
    // reporting scripts do, so a page that replaces `MutationObserver` in its
    // own world, as a page hiding from a content blocker does, takes nothing
    // away from them. The stylesheets stay in the page's world, where the
    // page's own scripts and the tests can see them.
    property int lateCosmeticSurveyLimit: 32
    property int lateCosmeticSurveyInterval: 100

    function forEachFrame(frame, act) {
        act(frame);
        for (let index = 0; index < frame.children.length; ++index)
            root.forEachFrame(frame.children[index], act);
    }
    // A frame's runJavaScript is overloaded and needs the callback to pick one.
    function runInFrame(frame, script) {
        frame.runJavaScript(script, WebEngineScript.MainWorld, function () {});
    }
    function runInFrameApplicationWorld(frame, script) {
        frame.runJavaScript(script, WebEngineScript.ApplicationWorld, function () {});
    }
    // The survey of the whole document, either posted to the adapter or
    // returned to a caller, and the watch that follows it. A survey that runs
    // again, for rules that changed under the document, starts the watch over:
    // every name is fresh again to the new rules.
    function cosmeticSurveySnippet(post) {
        const deliver = post ? "report('cosmetic_survey', survey);" : "return survey;";
        const survey = "const classes = new Set(), ids = new Set();"
              + "for (const element of document.querySelectorAll('[class], [id]')) {"
              + "if (element.id) ids.add(element.id);"
              + "for (const name of element.classList) classes.add(name);" + "}"
              + "const watch = globalThis.__omawebCosmeticWatch ??= {};"
              + "watch.seen = new Set([...Array.from(classes, name => '.' + name),"
              + " ...Array.from(ids, name => '#' + name)]);"
              + "watch.fresh = { classes: [], ids: [] };" + "watch.surveys = 0;"
              + "watch.asked = -Infinity;" + "clearTimeout(watch.timer);" + "watch.timer = 0;"
              + "if (!watch.observer) {" + "const note = element => {"
              + "if (element.id && !watch.seen.has('#' + element.id)) {"
              + "watch.seen.add('#' + element.id);" + "watch.fresh.ids.push(element.id);" + "}"
              + "for (const name of element.classList) {"
              + "if (watch.seen.has('.' + name)) continue;" + "watch.seen.add('.' + name);"
              + "watch.fresh.classes.push(name);" + "}" + "};" + "const ask = () => {"
              + "watch.timer = 0;" + "watch.asked = performance.now();"
              + "const fresh = watch.fresh;" + "watch.fresh = { classes: [], ids: [] };"
              + "if (++watch.surveys >= " + root.lateCosmeticSurveyLimit
              + ") watch.observer.disconnect();" + "report('cosmetic_survey', {"
              + "url: location.href, classes: fresh.classes, ids: fresh.ids });" + "};"
              + "watch.observer = new MutationObserver(records => {"
              + "for (const record of records) {"
              + "if (record.type === 'attributes') { note(record.target); continue; }"
              + "for (const node of record.addedNodes) {"
              + "if (node.nodeType !== Node.ELEMENT_NODE) continue;" + "note(node);"
              + "for (const element of node.querySelectorAll('[class], [id]')) note(element);"
              + "}" + "}"
              + "if (watch.timer || !(watch.fresh.classes.length || watch.fresh.ids.length))"
              + " return;" + "watch.timer = setTimeout(ask, Math.max(0, watch.asked + "
              + root.lateCosmeticSurveyInterval + " - performance.now()));" + "});" + "}"
              + "if (document.documentElement) "
              + "watch.observer.observe(document.documentElement, { childList: true,"
              + " subtree: true, attributes: true, attributeFilter: ['class', 'id'] });"
              + "const survey = { url: location.href, whole: true,"
              + " classes: Array.from(classes), ids: Array.from(ids) };" + deliver;
        return root.reporting("return (() => {" + survey + "})();");
    }
    readonly property string stopCosmeticWatchSnippet:
        "globalThis.__omawebCosmeticWatch?.observer?.disconnect()"

    property var cosmeticSurveyScript: {
        const script = WebEngine.script();
        script.name = "Omaweb cosmetic survey";
        script.injectionPoint = WebEngineScript.DocumentReady;
        script.worldId = WebEngineScript.ApplicationWorld;
        script.runsOnSubFrames = true;
        script.sourceCode = root.cosmeticSurveySnippet(true);
        return script;
    }

    // The site stylesheet is verified in the survey's round trip. The page may
    // have dropped the sheet from `adoptedStyleSheets` since document creation.
    // Only the main frame has one: the site rules go in at document creation,
    // and that script runs in no subframe.
    function siteRepairSnippet(frame) {
        if (!frame.isMainFrame)
            return "";
        const css = root.contentBlocker.cosmeticStyleSheet(frame.url);
        const repair = css.length > 0 || cosmeticRulesInjected ? root.styleSheetSnippet(
                                                                     root.cosmeticSheetId, css)
                                                                 + ";" : "";
        root.cosmeticRulesInjected = css.length > 0;
        return repair;
    }
    // What takes a frame's generic rules back, and its watch with them.
    function clearGenericCosmeticRules(frame, siteRepair) {
        const clear = genericCosmeticRulesInjected ? root.styleSheetSnippet(
                                                         root.genericCosmeticSheetId, "") : "";
        if (siteRepair.length > 0 || clear.length > 0)
            root.runInFrame(frame, siteRepair + clear);
        root.runInFrameApplicationWorld(frame, root.stopCosmeticWatchSnippet);
    }
    // What a frame reported: the whole document, which replaces what the frame
    // was given before, or the names that appeared since, which add to it.
    function answerCosmeticSurvey(frame, survey) {
        const blocker = root.contentBlocker;
        if (survey.whole && frame.isMainFrame)
            root.documentSurveyed = true;
        if (!blocker.cosmeticSurveyWanted(frame.url)) {
            if (survey.whole)
                root.clearGenericCosmeticRules(frame, root.siteRepairSnippet(frame));
            return;
        }
        const css = blocker.genericCosmeticStyleSheet(frame.url, survey.classes, survey.ids);
        if (!survey.whole) {
            if (css.length > 0) {
                root.genericCosmeticRulesInjected = true;
                root.runInFrame(frame, root.appendStyleSheetSnippet(root.genericCosmeticSheetId,
                                                                    css));
            }
            return;
        }
        // An empty answer takes back what an earlier one gave, if any did.
        const repair = root.siteRepairSnippet(frame);
        const sheet = css.length > 0 || genericCosmeticRulesInjected ? root.styleSheetSnippet(
                                                                           root.genericCosmeticSheetId,
                                                                           css) : "";
        if (css.length > 0)
            root.genericCosmeticRulesInjected = true;
        if (repair.length > 0 || sheet.length > 0)
            root.runInFrame(frame, repair + sheet);
    }
    function readCosmeticSurvey(text) {
        let survey;
        try {
            survey = JSON.parse(text);
        } catch (error) {
            return;
        }
        if (!survey || !root.contentBlocker || !Array.isArray(survey.classes) || !Array.isArray(
                    survey.ids))
            return;
        survey.classes = survey.classes.map(String);
        survey.ids = survey.ids.map(String);
        // The page writes its address the way the platform does, escaped, and
        // the view writes a URL the way Qt does; resolving the page's through
        // Qt gives the two one spelling. The main frame's is also held to the
        // address being loaded, because a document that replaces itself
        // reports after the next load has started and before its frame has
        // let go of its address.
        const reported = String(Qt.resolvedUrl(String(survey.url)));
        root.forEachFrame(webView.mainFrame, function (frame) {
            if (String(frame.url) !== reported)
                return;
            if (frame.isMainFrame && String(root.currentUrl) !== reported)
                return;
            root.answerCosmeticSurvey(frame, survey);
        });
    }
    // Rules or a site's own decision that changed under an open document: every
    // frame is surveyed again, because what it was given answers to rules that
    // are gone. Turning blocking off for a site has to take back what the
    // survey hid.
    function surveyGenericCosmeticRules() {
        const surveyGeneration = ++root.cosmeticSurveyGeneration;
        if (loading)
            return;
        if (!contentBlocker) {
            root.forEachFrame(webView.mainFrame, function (frame) {
                root.clearGenericCosmeticRules(frame, "");
            });
            return;
        }
        root.forEachFrame(webView.mainFrame, function (frame) {
            root.surveyFrameForGenericCosmeticRules(frame, surveyGeneration);
        });
    }
    function surveyFrameForGenericCosmeticRules(frame, surveyGeneration) {
        const blocker = root.contentBlocker;
        const surveyed = String(frame.url);
        const documentGeneration = root.pageGeneration;
        const ruleGeneration = root.cosmeticRuleGeneration;
        frame.runJavaScript(root.cosmeticSurveySnippet(false), WebEngineScript.ApplicationWorld,
                            function (survey) {
                                // The page can navigate away while the survey is in flight, and
                                // its classes say nothing about where the frame landed. A survey
                                // asked before the next change answers to rules that are gone.
                                const obsolete = root.loading || blocker !== root.contentBlocker
                                      || documentGeneration !== root.pageGeneration
                                      || ruleGeneration !== root.cosmeticRuleGeneration
                                      || surveyGeneration !== root.cosmeticSurveyGeneration ||
                                      !frame.isValid || surveyed !== String(frame.url);
                                if (!survey || obsolete)
                                    return;
                                root.answerCosmeticSurvey(frame, survey);
                            });
    }
    function checkForEditedFormState(callback) {
        webView.runJavaScript("(() => {"
                              + "for (const field of document.querySelectorAll('input, textarea')) {"
                              + "if (field.type === 'checkbox' || field.type === 'radio') {"
                              + "if (field.checked !== field.defaultChecked) return true;"
                              + "} else if (field.value !== field.defaultValue) return true;" + "}"
                              + "for (const option of document.querySelectorAll('select option')) {"
                              + "if (option.selected !== option.defaultSelected) return true;"
                              + "}" + "return Boolean(globalThis.__omawebContentEditableEdited);"
                              + "})()", callback);
    }
    function acceptNewWindowRequest(request) {
        if (request)
            request.openIn(webView);
    }
    function isAuxiliaryDestination(destination) {
        return destination === WebEngineNewWindowRequest.InNewDialog;
    }

    // A $popup rule is written against the windows a page opens, which is most
    // of them: a link that asks for a new tab counts, because that is how an
    // ad link opens. A background tab does not, because it takes a middle- or
    // ctrl-click to produce one and that is the user asking, not the page.
    function popupRefused(destination, requestedUrl) {
        return destination !== WebEngineNewWindowRequest.InNewBackgroundTab && root.contentBlocker
                && root.contentBlocker.shouldBlockPopup(requestedUrl, root.currentUrl,
                                                        root.spaceId);
    }

    // A Chromium profile is expensive and owns the Space's cache and cookie
    // store on disk. One per Space is correct; one per tab would have every
    // view contending for the same files. The window hands its Space profile
    // down as sharedProfile, so this one is built only for a view opened
    // without one, and only at the moment the view asks for it.
    property Component ownProfileComponent: Component {
        WebEngineProfile {
            storageName: "omaweb-space"
            // See EngineProfile.qml: without this the profile is memory-only
            // and every cookie dies with the process.
            offTheRecord: false
            persistentStoragePath: root.profilePath
            cachePath: root.profilePath + "/cache"
            persistentCookiesPolicy: WebEngineProfile.ForcePersistentCookies
            httpCacheType: WebEngineProfile.DiskHttpCache
        }
    }
    // A plain object rather than a property: the view reads the profile from
    // here while creating it, and a QML property would make that read a
    // dependency of the write and report a binding loop.
    readonly property var ownProfileHolder: ({
                                                 instance: null
                                             })

    function resolvedProfile() {
        if (root.sharedProfile)
            return root.sharedProfile;
        const holder = root.ownProfileHolder;
        if (!holder.instance) {
            holder.instance = root.ownProfileComponent.createObject(root);
            if (root.engineContentBlocker)
                root.engineContentBlocker.attachToProfile(holder.instance, root.spaceId);
        }
        return holder.instance;
    }

    Component.onCompleted: {
        root.completed = true;
        // Written once more with what it already holds, which takes the
        // binding off it: a script that changes from here on is put in by
        // installUserScript, and the binding must not write the collection
        // whole again for it.
        webView.userScripts.collection = root.userScriptList();
        Qt.callLater(root.applyKeyboardNavigationConfiguration);
        root.announcePage(root.currentUrl);
    }

    onCurrentUrlChanged: root.announcePage(root.currentUrl)

    Connections {
        target: root.contentBlocker
        ignoreUnknownSignals: true

        // Cosmetic rules change when the compiled rule set or a site's own
        // decision changes, so those are the only two that re-inject a
        // stylesheet. A refused request changes neither: doing it per refusal
        // cost a rule lookup and a script round trip hundreds of times over a
        // single page load, in every open tab at once.
        function onConfigurationChanged() {
            root.cosmeticRuleGeneration += 1;
            root.surveyGenericCosmeticRules();
        }

        function onRulesChanged() {
            root.cosmeticRuleGeneration += 1;
            root.surveyGenericCosmeticRules();
        }

        // Delivered to the view showing the page, so a batch for another view
        // is another view's.
        function onElementsRefused(view, addresses) {
            if (view === root)
                root.collapseRefusedElements(webView.mainFrame, addresses);
        }
    }

    // ---- the element a refusal leaves behind ----------------------------------
    //
    // Chromium draws an image whose request failed as a broken-image icon, so
    // a refusal on its own leaves a hole where the ad was, and a page measuring
    // its own bait reads the bait as shown. Content blocking delivers the
    // addresses it refused for the page on show, and this takes the elements
    // that asked for them out of the layout. The script only hides: what was
    // refused was decided in the core, and a request that failed any other way
    // is left as the engine draws it.
    //
    // Every frame, because the address says nothing about which frame's
    // element asked for it, and the page cannot reach into a frame of another
    // origin. In the application world, so a page that redefines what the
    // script leans on in its own world takes nothing away from it.
    //
    // The refused addresses stay with the document, because a request can be
    // made before its element is in the document: the preload scanner asks
    // for an image the parser has not reached yet. An element that settles
    // after the batch is checked against what was refused before it, on the
    // error a refusal fires and the load a substitute does. An element's own
    // addresses are the one it settled on and the one it was given, and a
    // media element's tracks besides; a substitute answers under the address
    // that was asked for, so the element still names it.
    function collapseSnippet(addresses) {
        return "(() => {" + "const first = !globalThis.__omawebRefused;"
                + "const refused = globalThis.__omawebRefused ??= new Set();"
                + "const asked = element => {"
                + "const addresses = [element.currentSrc, element.src, element.data];"
                + "for (const source of element.querySelectorAll('source')) "
                + "addresses.push(source.src);" + "return addresses;" + "};"
                + "const collapse = element => {"
                + "if (asked(element).some(address => address && refused.has(address))) "
                + "element.style.setProperty('display', 'none', 'important');" + "};"
                + "if (first) {"
                + "const settled = event => { if (event.target instanceof Element "
                + "&& event.target.matches('img, iframe, object, embed, video, audio')) "
                + "collapse(event.target); };" + "addEventListener('error', settled, true);"
                + "addEventListener('load', settled, true);" + "}" + "for (const address of "
                + JSON.stringify(addresses) + ") {"
                + "try { refused.add(new URL(address).href); } catch (error) {}" + "}"
                + "for (const element of document.querySelectorAll("
                + "'img, iframe, object, embed, video, audio')) collapse(element);" + "})()";
    }
    // A frame's runJavaScript is overloaded and needs the callback to pick one.
    function collapseRefusedElements(frame, addresses) {
        frame.runJavaScript(root.collapseSnippet(addresses), WebEngineScript.ApplicationWorld,
                            function () {});
        for (let index = 0; index < frame.children.length; ++index)
            root.collapseRefusedElements(frame.children[index], addresses);
    }

    readonly property string developerToolsSheetId: "__omaweb_developer_tools"

    // The palette arrives from the shell and may be empty until the theme has
    // loaded, so every colour read here names what to draw with instead.
    function paletteColor(source, name, fallback) {
        const value = source ? source[name] : undefined;
        return (value === undefined || String(value).length === 0) ? fallback : String(value);
    }

    function developerToolsColor(name, fallback) {
        return root.paletteColor(root.developerToolsColors, name, fallback);
    }

    function developerToolsSyntaxColor(name, fallback) {
        return root.paletteColor(root.developerToolsColors ? root.developerToolsColors.syntax : null,
                                 name, fallback);
    }

    function developerToolsBackgroundColor() {
        return root.developerToolsColor("windowOpaque", String(root.pageBackgroundColor));
    }

    // The frontend ships a light face and a dark one and picks between them
    // from a setting Omaweb does not own. Which one is right here is a question
    // about Omaweb's own window, so it is answered from the window's colour and
    // the answer is written onto the frontend's root element.
    function developerToolsDark() {
        const surface = Qt.color(root.developerToolsBackgroundColor());
        return (0.2126 * surface.r + 0.7152 * surface.g + 0.0722 * surface.b) < 0.5;
    }

    // Chromium's inspector is a webpage, and its whole design system is custom
    // properties declared on that page's `:root`. Naming them again, later and
    // marked important, is the whole of the theming: no patched frontend, no
    // debugging protocol, and nothing that breaks when the frontend adds a
    // token Omaweb has never heard of.
    //
    // The theme names nine syntax colours and the frontend has twenty-odd
    // tokens, so several tokens share one: they are the constructs that read
    // alike, and a terminal palette has no more hues to tell them apart with.
    // An attribute's value is drawn as the string it is, which is what every
    // editor does and what keeps the inspector reading like the one beside it.
    function developerToolsTokens() {
        const text = root.developerToolsColor("text", "#f3f1fa");
        const muted = root.developerToolsColor("mutedText", "#aaa5b7");
        const body = root.developerToolsBackgroundColor();
        const panel = root.developerToolsColor("sidebarOpaque", body);
        const surface = root.developerToolsColor("surface", "#302e3d");
        const hover = root.developerToolsColor("surfaceHover", "#3d394e");
        const border = root.developerToolsColor("border", "#4a4658");
        const accent = root.developerToolsColor("accent", "#9b87ff");
        const urgent = root.developerToolsColor("urgent", "#e06c75");
        const keyword = root.developerToolsSyntaxColor("keyword", accent);
        const string = root.developerToolsSyntaxColor("string", text);
        const number = root.developerToolsSyntaxColor("number", text);
        const comment = root.developerToolsSyntaxColor("comment", muted);
        const tag = root.developerToolsSyntaxColor("tag", text);
        const attribute = root.developerToolsSyntaxColor("attribute", text);
        const variable = root.developerToolsSyntaxColor("variable", text);
        const method = root.developerToolsSyntaxColor("function", accent);
        const type = root.developerToolsSyntaxColor("type", text);
        const punctuation = root.developerToolsSyntaxColor("punctuation", muted);
        return {
            // The panel bodies, the toolbars above them, and the raised
            // surfaces the frontend stacks on top.
            "--sys-color-cdt-base-container": body,
            "--sys-color-cdt-base": panel,
            "--sys-color-base": panel,
            "--sys-color-base-container": panel,
            "--sys-color-header-container": panel,
            "--sys-color-base-container-elevated": surface,
            "--sys-color-surface": surface,
            "--sys-color-surface1": surface,
            "--sys-color-surface2": surface,
            "--sys-color-surface3": hover,
            "--sys-color-surface4": hover,
            "--sys-color-surface5": hover,
            "--sys-color-surface-variant": hover,
            "--sys-color-neutral-container": surface,
            "--sys-color-omnibox-container": surface,
            "--sys-color-tonal-container": hover,
            // Type on all of them.
            "--sys-color-on-surface": text,
            "--sys-color-on-base": text,
            "--sys-color-on-tonal-container": text,
            "--sys-color-on-surface-subtle": muted,
            "--sys-color-on-surface-secondary": muted,
            "--sys-color-on-surface-primary": accent,
            // Every hairline the frontend draws.
            "--sys-color-divider": border,
            "--sys-color-divider-prominent": border,
            "--sys-color-divider-on-tonal-container": border,
            "--sys-color-on-base-divider": border,
            "--sys-color-outline": border,
            "--sys-color-neutral-outline": border,
            "--sys-color-tonal-outline": border,
            // What the frontend picks out, and what it warns with.
            "--sys-color-primary": accent,
            "--sys-color-primary-bright": accent,
            "--sys-color-state-focus-ring": accent,
            "--sys-color-state-text-highlight": accent,
            "--sys-color-state-on-text-highlight": body,
            "--sys-color-state-hover-on-subtle": hover,
            "--sys-color-state-header-hover": hover,
            "--sys-color-inverse-surface": text,
            "--sys-color-inverse-on-surface": body,
            "--sys-color-error": urgent,
            "--sys-color-error-bright": urgent,
            "--sys-color-error-outline": urgent,
            "--sys-color-on-surface-error": urgent,
            "--sys-color-token-deleted": urgent,
            // Source, markup and stylesheets.
            "--sys-color-token-keyword": keyword,
            "--sys-color-token-atom": number,
            "--sys-color-token-pseudo-element": keyword,
            "--sys-color-token-string": string,
            "--sys-color-token-inserted": string,
            "--sys-color-token-number": number,
            "--sys-color-token-comment": comment,
            "--sys-color-token-meta": comment,
            "--sys-color-token-subtle": punctuation,
            "--sys-color-token-tag": tag,
            "--sys-color-token-attribute": attribute,
            "--sys-color-token-property": method,
            "--sys-color-token-attribute-value": string,
            "--sys-color-token-string-special": string,
            "--sys-color-token-variable": variable,
            "--sys-color-token-property-special": method,
            "--sys-color-token-definition": method,
            // A builtin is the language's own name rather than one the
            // document invented, so it reads with the keywords and not with
            // the identifiers around it.
            "--sys-color-token-builtin": keyword,
            "--sys-color-token-variable-special": method,
            "--sys-color-token-type": type
        };
    }

    // Chromium themes its own inspector through the palette underneath the
    // tokens rather than through the tokens themselves: the frontend links
    // `devtools://theme/colors.css`, which is the browser's own UI theme
    // rendered as a Material tonal ramp, and every one of its two hundred-odd
    // design tokens is a tone of that ramp. Naming only the tokens Omaweb knows
    // about leaves everything else — buttons, badges, selections, the panels
    // Omaweb has never heard of — in Chrome's blues and greys. So the ramp is
    // named too, and the tokens on top of it are the exceptions Omaweb is sure
    // about rather than the whole of the theming.
    //
    // A tone is a lightness: tone 0 is black and tone 100 is white, whatever
    // hue the family carries. So each family keeps its colour's hue and
    // saturation and takes its lightness from the tone.
    readonly property var developerToolsToneLadder: [0, 10, 15, 20, 25, 30, 35, 40, 50, 60, 70, 80,
        90, 94, 95, 98, 99, 100]

    function developerToolsTone(base, tone, saturationCeiling) {
        const colour = Qt.color(base);
        // A grey has no hue to keep, and Qt reports it as none rather than as
        // zero. Asking for a tone of it can only mean a grey of that lightness.
        const achromatic = colour.hslHue < 0;
        const hue = achromatic ? 0 : colour.hslHue;
        const saturation = achromatic ? 0 : Math.min(colour.hslSaturation, saturationCeiling === undefined
                                                     ? 1 : saturationCeiling);
        return String(Qt.hsla(hue, saturation, tone / 100, 1));
    }

    // Which of Omaweb's colours each of the frontend's palette families is a
    // ramp of. The neutrals carry the window's own tint and little more of it
    // than that, or every surface in the inspector would be tinted twice.
    function developerToolsRamps() {
        const accent = root.developerToolsColor("accent", "#9b87ff");
        const keyword = root.developerToolsSyntaxColor("keyword", accent);
        const window = root.developerToolsBackgroundColor();
        return {
            "neutral": [window, 0.08],
            "neutral-variant": [window, 0.12],
            "primary": [accent, undefined],
            "secondary": [accent, undefined],
            "tertiary": [keyword, undefined],
            "error": [root.developerToolsColor("urgent", "#e06c75"), undefined],
            "blue": [root.developerToolsSyntaxColor("function", accent), undefined],
            "green": [root.developerToolsSyntaxColor("string", accent), undefined],
            "yellow": [root.developerToolsSyntaxColor("number", accent), undefined],
            "orange": [root.developerToolsSyntaxColor("number", accent), undefined],
            "pink": [root.developerToolsSyntaxColor("tag", accent), undefined],
            "purple": [keyword, undefined],
            "indigo": [keyword, undefined],
            "cyan": [root.developerToolsSyntaxColor("type", accent), undefined]
        };
    }

    function developerToolsPaletteDeclarations() {
        const ramps = root.developerToolsRamps();
        let declarations = "";
        for (const family in ramps) {
            const base = ramps[family][0];
            const ceiling = ramps[family][1];
            for (const tone of root.developerToolsToneLadder) {
                declarations += "--ref-palette-" + family + tone + ":" + root.developerToolsTone(
                            base, tone, ceiling) + " !important;";
            }
        }
        return declarations;
    }

    function developerToolsStyleSheet() {
        const tokens = root.developerToolsTokens();
        let declarations = root.developerToolsPaletteDeclarations();
        for (const name in tokens)
            declarations += name + ":" + tokens[name] + " !important;";
        // The frontend names its type per platform, at a selector of its own
        // that an ordinary `:root` rule would lose to, so these are marked as
        // the rest are. Omaweb's whole interface is drawn in one family, and the
        // inspector docked inside it is part of that window: its panel labels
        // take the family too, not only the source it lists. The sizes stay the
        // frontend's own above, because its layout is built around them.
        const font = (root.developerToolsColors && root.developerToolsColors.font) || ({});
        const family = String(font.family || "");
        if (family.length > 0) {
            const quoted = JSON.stringify(family) + ", monospace";
            for (const name of ["--default-font-family", "--monospace-font-family",
                                "--source-code-font-family", "--report-font-family",
                                "--report-font-family-monospace"]) {
                declarations += name + ":" + quoted + " !important;";
            }
        }
        const size = parseInt(font.size, 10);
        if (!isNaN(size) && size > 0) {
            declarations += "--monospace-font-size:" + size + "px !important;";
            declarations += "--source-code-font-size:" + size + "px !important;";
        }
        return ":root{" + declarations + "}";
    }

    // The frontend's DOM tree draws every bracket, equals sign and quote in the
    // same colour as a tag's name, because one class carries both: the name is
    // a span inside the `<...>` the class wraps. An editor separates them, and
    // no design token can — only a rule can.
    //
    // Markup's delimiters take the type colour, which is where an editor puts
    // them: they are structure rather than content, but structure the language
    // itself defines, and drawn in the interface's quiet grey instead they
    // leave a tree of grey punctuation with a few coloured names floating in
    // it. Elsewhere in the frontend — source, stylesheets, the console — the
    // quiet grey is right and `--sys-color-token-subtle` still carries it.
    //
    // Those spans live in shadow trees, where a rule in the document cannot
    // reach them. So the frontend's own `attachShadow` is wrapped before any of
    // its scripts run, and every tree it opens from then on adopts one more
    // stylesheet. Nothing is rewritten and nothing is read back: the frontend
    // builds exactly what it would have built, in Omaweb's colours.
    function developerToolsMarkupStyleSheet() {
        const delimiter = root.developerToolsSyntaxColor("type", root.developerToolsColor(
                                                             "mutedText", "#aaa5b7"));
        const tag = root.developerToolsSyntaxColor("tag", root.developerToolsColor("text",
                                                                                   "#f3f1fa"));
        return ".webkit-html-tag{color:" + delimiter + " !important}"
                + ".webkit-html-tag-name,.webkit-html-close-tag-name{color:" + tag + " !important}";
    }

    function developerToolsShadowSnippet() {
        return "(() => {" + "const css = " + JSON.stringify(root.developerToolsMarkupStyleSheet())
                + ";" + "const host = globalThis.__omawebDeveloperToolsShadow;"
                + "if (host) { host.sheet.replaceSync(css); return; }"
                + "if (typeof CSSStyleSheet !== 'function') return;" + "let sheet;"
                + "try { sheet = new CSSStyleSheet(); sheet.replaceSync(css); }"
                + "catch (error) { return; }"
                + "globalThis.__omawebDeveloperToolsShadow = { sheet: sheet };"
                + "const attachShadow = Element.prototype.attachShadow;"
                + "Element.prototype.attachShadow = function(options) {"
                + "const shadow = attachShadow.call(this, options);"
                + "try { shadow.adoptedStyleSheets = [...shadow.adoptedStyleSheets, sheet]; }"
                + "catch (error) {}" + "return shadow;" + "};" + "})();\n";
    }

    function developerToolsThemeSnippet() {
        return "(() => {" + "const element = document.documentElement;"
                + "if (element) element.classList.toggle('theme-with-dark-background', " + (
                    root.developerToolsDark() ? "true" : "false") + ");" + "})();\n"
                + root.developerToolsShadowSnippet() + root.styleSheetSnippet(
                    root.developerToolsSheetId, root.developerToolsStyleSheet());
    }

    // The frontend has to open in Omaweb's colours rather than arrive in
    // Chromium's and change, so the sheet is in the document before the
    // frontend's own scripts run. It is re-applied on a live theme change,
    // which is the only time the colours move under an open inspector.
    property var developerToolsThemeScript: {
        const script = WebEngine.script();
        script.name = "Omaweb developer tools theme";
        script.injectionPoint = WebEngineScript.DocumentCreation;
        script.worldId = WebEngineScript.MainWorld;
        script.runsOnSubFrames = true;
        script.sourceCode = root.developerToolsThemeSnippet();
        return script;
    }

    function applyDeveloperToolsTheme() {
        if (!root.developerToolsView)
            return;
        root.developerToolsView.runJavaScript(root.developerToolsThemeSnippet());
    }

    onDeveloperToolsColorsChanged: root.applyDeveloperToolsTheme()

    property var keyboardNavigationScript: {
        const script = WebEngine.script();
        script.name = "Omaweb Keyboard navigation";
        script.injectionPoint = WebEngineScript.DocumentReady;
        script.worldId = WebEngineScript.MainWorld;
        script.runsOnSubFrames = false;
        script.sourceCode = root.keyboardNavigationScriptSource
                + "\nglobalThis.__omawebKeyboardNavigation && "
                + "globalThis.__omawebKeyboardNavigation.configure(" + JSON.stringify(
                    root.keyboardNavigationConfiguration) + ");";
        return script;
    }
    onKeyboardNavigationScriptChanged: root.installUserScript(root.keyboardNavigationScript.name,
                                                              root.keyboardNavigationScript)

    // When the document exists and when it has first painted. Chromium keeps
    // the outgoing page on show until the next one's first paint, so the
    // canvas matters from the moment that page's frames are the ones shown:
    // theme until the page has drawn something of its own, white after. The
    // engine does not hand either moment out, so the page reports them, the
    // paint through the timing entry Chromium records for it.
    property var documentPaintedScript: {
        const script = WebEngine.script();
        script.name = "Omaweb document painted";
        script.injectionPoint = WebEngineScript.DocumentCreation;
        script.worldId = WebEngineScript.ApplicationWorld;
        script.runsOnSubFrames = false;
        script.sourceCode = root.reporting(`
            report('document_created');
            let painted = false;
            const paint = () => {
                if (painted) return;
                painted = true;
                report('document_painted');
            };
            try {
                new PerformanceObserver(paint).observe({type: 'paint', buffered: true});
            } catch (error) {
                paint();
            }`);

        return script;
    }

    // A first-hand gesture on the page, reported once per document. Chromium
    // has its own record of user activation but does not hand it out, and the
    // shell needs it per origin rather than per page, so the page says so.
    property var userActivationScript: {
        const script = WebEngine.script();
        script.name = "Omaweb user activation";
        script.injectionPoint = WebEngineScript.DocumentReady;
        script.worldId = WebEngineScript.ApplicationWorld;
        script.runsOnSubFrames = false;
        script.sourceCode = root.reporting(`
            let reported = false;
            const activated = () => {
                if (reported) return;
                reported = true;
                report('user_activation');
            };
            for (const name of ['pointerdown', 'keydown', 'touchstart'])
                document.addEventListener(name, activated, {capture: true, passive: true});`);

        return script;
    }

    // What the page says it is playing, and the page's own answer to a media
    // key. The Media Session API is the page's statement about itself, and the
    // engine does not hand it out, so first-party script reads it and reports
    // it the way the other page facts here are reported.
    //
    // A page that registered its own handler for an action gets that handler
    // called, because the page knows what its next track is and Omaweb does
    // not. A page that registered none is answered by the element that is
    // sounding, which is what a video in a tab with no media session is.
    //
    // This one runs in the page's world, because the setters it wraps are the
    // page's own and a wrapper in another world would see none of the page's
    // writes. It runs before the page does and keeps the console it found.
    property var mediaSessionScript: {
        const script = WebEngine.script();
        script.name = "Omaweb media session";
        script.injectionPoint = WebEngineScript.DocumentCreation;
        script.worldId = WebEngineScript.MainWorld;
        script.runsOnSubFrames = false;
        script.sourceCode = `(() => {
            if (globalThis.__omawebMediaSession) return;
            ` + root.reportSnippet() + `
            const session = navigator.mediaSession;
            const handlers = new Map();
            let reported = '';
            const elements = () => Array.from(document.querySelectorAll('video, audio'));
            // The element the keys are for: the one that is running, or failing
            // that the one that has been played and stopped part way.
            const sounding = () => elements().find(element => !element.paused && !element.ended)
                || elements().find(element => element.currentTime > 0 && !element.ended)
                || null;
            const state = () => {
                const declared = session && session.playbackState;
                if (declared === 'playing' || declared === 'paused') return declared;
                const element = sounding();
                if (!element) return 'none';
                return element.paused ? 'paused' : 'playing';
            };
            // The largest picture the page named, as an address. Omaweb does
            // not fetch it: whoever draws it asks the site for it themselves.
            const artwork = metadata => {
                const pictures = (metadata && metadata.artwork) || [];
                let chosen = '';
                let widest = -1;
                for (const picture of pictures) {
                    const size = String(picture.sizes || '').split('x')[0];
                    const width = Number.parseInt(size, 10) || 0;
                    if (width >= widest) {
                        widest = width;
                        chosen = String(picture.src || '');
                    }
                }
                return chosen;
            };
            const declare = () => {
                const metadata = session ? session.metadata : null;
                const declaration = {
                    state: state(),
                    title: metadata ? String(metadata.title || '') : '',
                    artist: metadata ? String(metadata.artist || '') : '',
                    album: metadata ? String(metadata.album || '') : '',
                    artwork: artwork(metadata),
                    canGoNext: handlers.has('nexttrack'),
                    canGoPrevious: handlers.has('previoustrack'),
                };
                const encoded = JSON.stringify(declaration);
                if (encoded === reported) return;
                reported = encoded;
                report('media_session', declaration);
            };
            // The API reports no change of its own, so the two properties a
            // page writes are wrapped where they are defined. The page's own
            // write happens first and this reports what it left behind.
            if (session) {
                for (const name of ['metadata', 'playbackState']) {
                    const property = Object.getOwnPropertyDescriptor(
                        Object.getPrototypeOf(session), name);
                    if (!property || !property.set) continue;
                    Object.defineProperty(session, name, {
                        configurable: true,
                        enumerable: true,
                        get() { return property.get.call(this); },
                        set(value) { property.set.call(this, value); declare(); },
                    });
                }
                const setActionHandler = session.setActionHandler.bind(session);
                session.setActionHandler = (action, handler) => {
                    setActionHandler(action, handler);
                    if (handler) handlers.set(action, handler);
                    else handlers.delete(action);
                    declare();
                };
            }
            // Media events do not bubble, so the document listens in the
            // capture phase: a page that plays without a media session is still
            // a page the desktop hears.
            for (const name of ['play', 'pause', 'ended', 'emptied', 'loadedmetadata'])
                document.addEventListener(name, declare, {capture: true, passive: true});
            globalThis.__omawebMediaSession = {
                invoke(command) {
                    const wanted = command === 'playpause'
                        ? (state() === 'playing' ? 'pause' : 'play') : command;
                    const action = {play: 'play', pause: 'pause', stop: 'stop',
                                    next: 'nexttrack', previous: 'previoustrack'}[wanted];
                    const handler = handlers.get(action);
                    if (handler) {
                        try {
                            handler({action: action});
                            declare();
                            return;
                        } catch (error) {
                            // The page's own handler threw. The element below it
                            // is still there, so the key does something.
                        }
                    }
                    const element = sounding() || elements()[0];
                    if (!element) return;
                    if (wanted === 'play') element.play();
                    else if (wanted === 'pause') element.pause();
                    else if (wanted === 'stop') {
                        element.pause();
                        element.currentTime = 0;
                    }
                    declare();
                },
            };
            declare();
        })();`;

        return script;
    }

    // What the reader pressed, reported as the click is dispatched and before
    // anything the press does reaches the browser, so a window the click asks
    // for can be placed by it. The link or control pressed is the element a
    // window would come from; a press on nothing in particular is the point.
    // A click the page synthesised, a link hint following a link say, has no
    // point, and names the element instead.
    property var pressOriginScript: {
        const script = WebEngine.script();
        script.name = "Omaweb press origin";
        script.injectionPoint = WebEngineScript.DocumentReady;
        script.worldId = WebEngineScript.ApplicationWorld;
        script.runsOnSubFrames = false;
        script.sourceCode = root.reporting(`document.addEventListener('click', event => {
            const target = event.target && event.target.closest
                ? event.target.closest('a[href], button, [role="button"], [role="link"]')
                : null;
            let origin;
            if (target) {
                const rect = target.getBoundingClientRect();
                origin = {x: rect.left, y: rect.top, width: rect.width, height: rect.height};
            } else if (event.isTrusted) {
                origin = {x: event.clientX, y: event.clientY, width: 1, height: 1};
            } else {
                return;
            }
            report('press_origin', origin);
        }, {capture: true, passive: true});`);
        return script;
    }

    // Every script the view runs in a page, in one place: the collection is
    // written whole from this list while the view is built, and a list kept in
    // two places lost the scripts added to only one of them on the first page
    // load. A script that changes afterwards, a sheet with the theme or the
    // keyboard navigation with its configuration, goes in by installUserScript.
    // The Content blocking script is the Engine profile's, not the view's.
    function userScriptList() {
        return [root.editedStateScript, root.keyboardNavigationScript,
                root.externalProtocolOriginScript, root.documentPaintedScript,
                root.userActivationScript, root.pressOriginScript, root.controlAccentScript,
                root.pagePaletteScript, root.pageScrollbarScript, root.pageScrollReportScript,
                root.mediaSessionScript, root.cosmeticSurveyScript];
    }

    property var externalProtocolOriginScript: {
        const script = WebEngine.script();
        script.name = "Omaweb external protocol origin";
        script.injectionPoint = WebEngineScript.DocumentReady;
        script.worldId = WebEngineScript.ApplicationWorld;
        script.runsOnSubFrames = true;
        script.sourceCode = root.reporting(`document.addEventListener('click', event => {
            const link = event.target && event.target.closest
                ? event.target.closest('a[href]') : null;
            if (!link) return;
            const scheme = String(link.protocol || '').replace(':', '').toLowerCase();
            if (['http', 'https', 'file', 'about', 'data', 'omaweb'].includes(scheme)) return;
            report('external_protocol', {destination: link.href, origin: location.origin});
        }, true);`);

        return script;
    }

    WebEngineView {
        id: webView
        objectName: "qtWebView"
        anchors.fill: parent
        profile: root.resolvedProfile()
        // Chromium draws a PDF in a sandboxed viewer of its own, with find,
        // zoom, print and download inside it. Without this the profile
        // downloads the document instead, which is what an engine with no such
        // viewer does.
        settings.pdfViewerEnabled: true
        // A page may ask for the screen. Omaweb answers the request rather than
        // the engine, so the shell can say whose page took it and hand it back.
        settings.fullScreenSupportEnabled: true
        // Opening one selected local document must not give that document a
        // directory-wide read capability. Related resources need a server or
        // an explicit file-selection grant of their own. Chromium withholds the
        // same capability by default, and grants the remote one below, so a
        // page written against a browser renders here as its author saw it.
        settings.localContentCanAccessFileUrls: false
        settings.localContentCanAccessRemoteUrls: true
        // Chromium paints this under the page for the page's whole life, not
        // only before the page supplies a background, so a page that draws
        // none gets the white it was written against. The theme stands in
        // only between a document's creation and its first paint, where white
        // would flash a bright rectangle through dark chrome.
        backgroundColor: root.documentPainted ? "white" : root.pageBackgroundColor
        focus: true
        userScripts.collection: root.userScriptList()
        // Chromium's autoplay policy is per view. Requiring a gesture blocks
        // muted autoplay along with audible autoplay, so the shell decides
        // instead: it turns the requirement off once the page has nothing left
        // to gain by starting — either the tab is muted or the reader has
        // already dealt with the origin.
        settings.playbackRequiresUserGesture: !root.autoplayAllowed

        onRecommendedStateChanged: root.applyPageLifecycle()

        onRenderProcessTerminated: function (terminationStatus, exitCode) {
            root.rendererFailed("Renderer stopped with exit code " + exitCode);
        }

        // Deferred rather than answered: Chromium blocks the load while the
        // shell decides, which is what makes blocking the default. A failure
        // in a subresource or in a frame is reported with that fact attached
        // rather than filtered out here — refusing it is the shell's rule to
        // state, and the shell has to be able to say it refused one.
        onCertificateError: function (error) {
            error.defer();
            if (error.isMainFrame) {
                root.certificateErrorRaisedForLoad = true;
                root.certificateErrorOrigin = root.originLabel(error.url);
            }
            const requestId = String(++root.nextCertificateErrorId);
            root.pendingCertificateErrors[requestId] = error;
            root.certificateErrorRaised(requestId, {
                                            "url": String(error.url),
                                            "origin": root.originLabel(error.url),
                                            "description": error.description,
                                            "overridable": error.overridable,
                                            "mainFrame": error.isMainFrame,
                                            "fatal": root.fatalCertificateError(error.type)
                                        });
        }

        // Chromium keeps the node the menu was opened over, and Omaweb has to
        // know that it has one: nothing on the view reports it, and the action
        // that reads it crashes when there is none. Accepting the request is
        // what stops the engine drawing a menu of its own over Omaweb's.
        onContextMenuRequested: function (request) {
            root.contextMenuTargetKnown = true;
            root.lastContextMediaType = root.mediaTypeName(request.mediaType);
            request.accepted = true;
            root.pageContextRequested({
                                          "x": request.position.x,
                                          "y": request.position.y,
                                          "selectedText": request.selectedText,
                                          "linkText": request.linkText,
                                          "linkUrl": request.linkUrl,
                                          "mediaUrl": request.mediaUrl,
                                          "mediaType": root.mediaTypeName(request.mediaType),
                                          "editable": request.isContentEditable,
                                          "pageGeneration": root.pageGeneration
                                      });
        }

        // Accepting the request is what stops the engine's own tooltip, in the
        // way the menu above is taken. Chromium reports the point in the view's
        // coordinates and leaves the text on a withdrawal, so the type decides
        // what is passed on rather than the text being tested for emptiness: a
        // page may raise a tooltip whose title really is empty.
        onTooltipRequested: function (request) {
            request.accepted = true;
            const showing = request.type === TooltipRequest.Show;
            root.pageTooltipRequested({
                                          "visible": showing,
                                          "text": showing ? request.text : "",
                                          "x": request.x,
                                          "y": request.y,
                                          "pageGeneration": root.pageGeneration
                                      });
        }

        onRenderProcessPidChanged: root.refreshRenderProcessPid()
        onLoadingChanged: function (loadRequest) {
            root.refreshRenderProcessPid();
            if (loadRequest.status === WebEngineView.LoadStartedStatus) {
                root.pageGeneration += 1;
                root.loadSetOutFrom = loadRequest.url;
                root.announceDocument(loadRequest.url);
                // The page being left takes its length with it, so the bar is
                // not drawn for the last document while the next one arrives.
                root.forgetPageScroll();
                // A press on the page being left says nothing about where a
                // window the next page asks for should come from.
                root.pressOrigin = Qt.rect(0, 0, 0, 0);
                // The address being loaded, not the one still on show: a
                // refusal the outgoing document earned belongs to it.
                root.announcePage(loadRequest.url);
                root.javaScriptDialogsBlocked = false;
                root.lastLoadFailed = false;
                root.certificateErrorRaisedForLoad = false;
                // The node Chromium is holding belonged to the page being
                // replaced. What is at those coordinates now is not what the
                // reader pointed at, so the next keyboard request picks again.
                root.contextMenuTargetKnown = false;
                // The matches were in the page being replaced. The query is the
                // reader's and stays, ready to run against what arrives.
                root.forgetFindMatches();
                // The document about to be created has been given nothing
                // yet, whatever the last one had.
                root.genericCosmeticRulesInjected = false;
                root.documentSurveyed = false;
                return;
            }
            // A document that loaded surveyed itself when its DOM was parsed
            // and verified the site stylesheet with the answer, unless the
            // survey script never reached it. One that did not load gets the
            // site stylesheet verified here.
            if (loadRequest.status !== WebEngineView.LoadSucceededStatus)
                root.applyCosmeticRules();
            else if (!root.documentSurveyed)
                root.surveyGenericCosmeticRules();
            if (loadRequest.status === WebEngineView.LoadFailedStatus) {
                root.lastLoadFailed = true;
            }
            // A document the script does not reach, a `data:` page, an error
            // page or a viewer of the engine's own, reports neither moment: it
            // keeps the canvas of the page before it, white after any page has
            // painted, and is on show once its load is over.
            if (!loading)
                root.documentPainted = true;
            // A load that arrived without a certificate failure clears the
            // report. The certificate that failed may since have been fixed,
            // and the adapter must not keep saying otherwise — the engine's
            // own memory of an accepted certificate is the shell's to carry,
            // because the adapter cannot tell a fixed certificate from a
            // waived one and must not guess.
            if (loadRequest.status === WebEngineView.LoadSucceededStatus &&
                    !root.certificateErrorRaisedForLoad) {
                root.certificateErrorOrigin = "";
            }
            if (!loading)
                root.applyKeyboardNavigationConfiguration();
        }

        onNewWindowRequested: function (request) {
            if (root.popupRefused(request.destination, request.requestedUrl))
                return;
            if (request.destination === WebEngineNewWindowRequest.InNewBackgroundTab)
                root.backgroundTabRequested(request.requestedUrl);
            else if (root.isAuxiliaryDestination(request.destination))
                root.auxiliaryWindowRequested(request, request.requestedUrl);
            else
                root.newTabRequested(request, request.requestedUrl);
        }

        onNavigationRequested: function (request) {
            // A redirect is asked about here, before its destination is even
            // fetched, which is the last moment before the arriving document
            // runs its own scripts; the address property moves only once the
            // document has committed, when those scripts have run already.
            if (request.isMainFrame && request.navigationType
                    === WebEngineNavigationRequest.RedirectNavigation)
                root.announceDocument(request.url);
            const address = String(request.url);
            const scheme = address.substring(0, address.indexOf(":")).toLowerCase();
            if (scheme === "http" || scheme === "https" || scheme === "file" || scheme === "about"
                    || scheme === "data" || scheme === "omaweb") {
                // Site rules and scriptlets are named by host, which only a
                // web address has.
                if (request.isMainFrame && (scheme === "http" || scheme === "https"))
                    root.installBlockingScript(request.url);
                return;
            }
            request.reject();
            root.requestExternalProtocol(request.url, request.isMainFrame);
        }

        onWindowCloseRequested: root.windowCloseRequested()

        onFindTextFinished: function (result) {
            // A result for the page being replaced would otherwise report
            // matches against the page that replaced it.
            if (root.findGeneration !== root.pageGeneration)
                return;
            root.findMatchCount = result.numberOfMatches;
            root.findActiveMatch = result.activeMatch;
        }

        onPdfPrintingFinished: function (filePath, success) {
            root.printFinished(filePath, success);
        }

        // Accepted here and reported to the shell, which is what makes site
        // fullscreen a state Omaweb is in rather than something the engine did
        // to the window behind its back.
        // Accepted last, once Omaweb is in the state it is accepting. Accepting
        // first would leave a page laid out for a screen it has been promised
        // and not given if anything here failed, and the reader looking at a
        // broken page in a window that never changed.
        onFullScreenRequested: function (request) {
            // The origin is named before the state changes: the shell reports
            // who took the screen the moment it hears that someone did.
            root.siteFullscreenOrigin = request.toggleOn ? root.originLabel(request.origin) : "";
            root.siteFullscreenActive = request.toggleOn;
            request.accept();
        }

        onJavaScriptConsoleMessage: function (level, message, lineNumber, sourceId) {
            const report = root.pageReport(message);
            if (!report) {
                if (message === "__omaweb_keyboard_hint_mode__:1")
                    root.keyboardNavigationHintModeActive = true;
                else if (message === "__omaweb_keyboard_hint_mode__:0")
                    root.keyboardNavigationHintModeActive = false;
                return;
            }
            if (report.channel === "site_data_cleared") {
                try {
                    const cleared = JSON.parse(report.body);
                    root.pageSiteDataCleared(String(cleared.origin), cleared.cleared,
                                             cleared.refused.length > 0 ? cleared.refused.join(
                                                                              " and ")
                                                                          + " could not be emptied" :
                                                                          "");
                } catch (error) {
                    root.pageSiteDataCleared("", [], "the page did not answer");
                }
            } else if (report.channel === "external_protocol") {
                try {
                    const named = JSON.parse(report.body);
                    root.externalProtocolOrigins[String(named.destination)] = String(named.origin);
                } catch (error) {
                    console.warn("Could not read external protocol origin: " + error);
                }
            } else if (report.channel === "press_origin") {
                try {
                    root.recordPressOrigin(JSON.parse(report.body));
                } catch (error) {
                    console.warn("Could not read the press origin: " + error);
                }
            } else if (report.channel === "page_scroll") {
                root.readPageScroll(report.body);
            } else if (report.channel === "cosmetic_survey") {
                root.readCosmeticSurvey(report.body);
            } else if (report.channel === "media_session") {
                try {
                    root.pageMediaSession = JSON.parse(report.body);
                } catch (error) {
                    // A page that cannot be read declares nothing, which is
                    // what a page with no media session declares too.
                    root.pageMediaSession = {};
                }
            } else if (report.channel === "document_created") {
                root.documentPainted = false;
            } else if (report.channel === "document_painted") {
                root.documentPainted = true;
            } else if (report.channel === "user_activation") {
                root.userActivated();
            }
        }

        onPermissionRequested: function (request) {
            const permission = root.permissionName(request.permissionType);
            const decision = root.permissionController
                  ? root.permissionController.permissionDecision(request.origin, permission) : 0;
            if (decision === 1 || decision === 2)
                request.grant();
            else if (decision === 3)
                request.deny();
            else {
                const requestId = String(++root.nextPermissionRequestId);
                root.pendingPermissions[requestId] = request;
                root.sitePermissionRequested(requestId, request.origin.toString(), permission);
            }
        }

        onJavaScriptDialogRequested: function (request) {
            request.accepted = true;
            if (root.javaScriptDialogsBlocked) {
                request.dialogReject();
                return;
            }
            let suffix = "alert";
            if (request.type === JavaScriptDialogRequest.DialogTypeConfirm)
                suffix = "confirm";
            else if (request.type === JavaScriptDialogRequest.DialogTypePrompt)
                suffix = "prompt";
            else if (request.type === JavaScriptDialogRequest.DialogTypeBeforeUnload)
                suffix = "before-unload";
            const requestId = String(++root.nextBrowserPromptId);
            const kind = "javascript-" + suffix;
            root.pendingBrowserPrompts[requestId] = {
                "request": request,
                "kind": kind
            };
            root.browserPromptRequested(requestId, {
                                            "kind": kind,
                                            "origin": request.securityOrigin.toString(),
                                            "message": request.message,
                                            "defaultText": request.defaultText
                                        });
        }

        onAuthenticationDialogRequested: function (request) {
            request.accepted = true;
            const requestId = String(++root.nextBrowserPromptId);
            root.pendingBrowserPrompts[requestId] = {
                "request": request,
                "kind": "http-authentication"
            };
            root.browserPromptRequested(requestId, {
                                            "kind": "http-authentication",
                                            "origin": request.url.toString(),
                                            "message": "Sign in to " + root.originLabel(request.url),
                                            "detail": request.realm
                                        });
        }

        onFileDialogRequested: function (request) {
            request.accepted = true;
            const requestId = String(++root.nextBrowserPromptId);
            let mode = "open";
            if (request.mode === FileDialogRequest.FileModeOpenMultiple)
                mode = "open-multiple";
            else if (request.mode === FileDialogRequest.FileModeUploadFolder)
                mode = "folder";
            else if (request.mode === FileDialogRequest.FileModeSave)
                mode = "save";
            root.pendingFileSelections[requestId] = request;
            root.fileSelectionRequested(requestId, {
                                            "mode": mode,
                                            "mimeTypes": request.acceptedMimeTypes,
                                            "suggestedName": request.defaultFileName
                                        });
        }
    }
}
