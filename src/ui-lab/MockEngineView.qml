import QtQuick
import Omaweb.Engine

Rectangle {
    id: root
    objectName: "mockEngineView"

    property url currentUrl: "about:blank"
    property bool blurReviewPattern: false
    // Draws the stand-in document below instead of the lab's own card.
    property bool documentReview: false
    // `--browse` asks for the browser in use: a web address draws a sample
    // page, in the page palette a themed site would take, instead of saying
    // no engine is running. The lab's own reviews keep the plain view.
    readonly property bool samplePage: typeof labSamplePages !== "undefined" && labSamplePages
                                       && String(root.currentUrl).startsWith("http")
    // A page that is never still, so the chrome can be timed over a page that
    // asks for a frame every frame, the way one with a video or a spinner
    // does. Off unless a probe turns it on: a page at rest costs no frames.
    property bool motionReview: false
    property string pageTitle: currentUrl.toString() === "about:blank" ? qsTr("New tab") :
                                                                         currentUrl.toString()
    // The lab runs no engine and so has no icon store. Handing every host one
    // of the drawn stand-ins is what makes the sidebar's chips reviewable
    // without the QtWebEngine build.
    property url pageIconUrl: {
        if (!mockFaviconUrls || mockFaviconUrls.length === 0)
            return "";
        const address = String(root.currentUrl);
        if (address.length === 0 || address.startsWith("about:"))
            return "";
        let hash = 0;
        for (let index = 0; index < address.length; ++index)
            hash = (hash * 31 + address.charCodeAt(index)) % 104729;
        return mockFaviconUrls[hash % mockFaviconUrls.length];
    }
    property bool loading: false
    // Whether the page on show has drawn anything of its own yet. The lab
    // paints as soon as an address arrives, except on `slow-paint.example`,
    // which stands in for a page slow to draw until `simulateFirstPaint`.
    property bool documentPainted: root.paintsAtOnce(root.currentUrl)
    function paintsAtOnce(address) {
        return String(address).indexOf("://slow-paint.example") < 0;
    }
    function simulateFirstPaint() {
        root.documentPainted = true;
    }
    // The lab plays nothing, so both sides of the tab's speaker are set by
    // hand: `simulateAudible` stands in for a page that started making sound.
    property bool pageAudible: false
    // The lab runs no page, so what a page would declare about what it is
    // playing is set by hand through `simulateMediaSession`.
    property var pageMediaSession: ({})
    // What the desktop last asked of this page, so a review can see that the
    // media key reached the tab it was meant for.
    property string lastMediaAction: ""
    property bool audioMuted: false
    // The lab starts no page and no process, so both sides of autoplay are set
    // by hand: `autoplayAllowed` is what the shell decides, and
    // `simulateAutoplay` stands in for a page that tried to start on its own —
    // which it may do silently, and is heard only where the shell has not
    // silenced the tab.
    property bool autoplayAllowed: false
    property int autoplayBlockedCount: 0
    // No renderer, so no process to account for. The lab reports a pid a test
    // can name rather than pretending to have one.
    property int renderProcessPid: 0
    // Whether the shell has stopped this page for want of a reader. The lab has
    // nothing to stop, so it records the decision and nothing else: which pages
    // are frozen is the shell's rule, and this is where a test reads it.
    property bool pageFrozen: false
    property bool canGoBack: false
    property bool canGoForward: false
    property string pageLocalState: ""
    property string profilePath: ""
    property var sharedProfile: null
    property var contentBlocker: null
    property var engineContentBlocker: null
    property var engineCookiePolicy: null
    property var permissionController: null
    readonly property var browserProfile: root.sharedProfile ? root.sharedProfile : root
    property string spaceId: ""
    property var keyboardNavigationConfiguration: ({})
    property string keyboardNavigationScriptSource: ""
    property bool keyboardNavigationHintModeActive: false
    property string keyboardInput: ""
    property color pageBackgroundColor: "#16151d"
    property color pageControlAccent: "transparent"
    property var pagePalette: null
    property real pageScrollOffset: 0
    property real pageScrollLength: 0
    property real pageViewportLength: 0
    property color pageScrollbarThumb: "transparent"
    property color pageScrollbarTrack: "transparent"
    function scrollPageTo(offset) {
        root.pageScrollOffset = offset;
    }
    readonly property bool pageHasFocus: root.activeFocus
    // The lab is also the engine that cannot do everything. Each everyday page
    // operation is switchable on its own, because the shell has to be
    // reviewable against an adapter that reports the gap rather than only
    // against one that answers for everything.
    property bool findAvailable: true
    property bool zoomAvailable: true
    property bool printingAvailable: true
    property bool siteFullscreenAvailable: true
    property bool inlinePdfViewingAvailable: true
    // The two parts of a site's security contract an engine can be missing.
    // Each is switchable on its own so the shell can be reviewed against an
    // adapter that says it cannot report a certificate failure, and against one
    // that cannot refuse a third-party cookie.
    property bool certificateDecisionsAvailable: true
    property bool thirdPartyCookieControlAvailable: true
    // Whether the lab reports the chain a page arrived over, as the patched Qt
    // engine does. Off stands for an engine that reports only the chain a
    // certificate failure was raised for.
    property bool pageCertificatesAvailable: true
    // Off by default and honest about it: the lab keeps nothing on disk, so
    // Site information has no size to show. A test that needs the shell stood
    // up against an engine that does keep a profile turns it on.
    property bool persistentProfilesAvailable: false
    readonly property int capabilities: EngineCapabilities.Navigation
                                        | EngineCapabilities.PrivateProfiles
                                        | EngineCapabilities.ContentBlocking
                                        | EngineCapabilities.KeyboardPageCommands | (
                                            root.inspectorAvailable
                                            ? EngineCapabilities.DeveloperTools : 0)
                                        | EngineCapabilities.RendererRecovery | (root.findAvailable
                                                                                 ? EngineCapabilities.PageFind :
                                                                                   0) | (root.zoomAvailable
                                                                                         ? EngineCapabilities.PageZoom :
                                                                                           0) | (root.printingAvailable
                                                                                                 ? EngineCapabilities.Printing :
                                                                                                   0) | (root.siteFullscreenAvailable
                                                                                                         ? EngineCapabilities.SiteFullscreen :
                                                                                                           0) | (root.inlinePdfViewingAvailable
                                                                                                                 ? EngineCapabilities.InlinePdfViewing :
                                                                                                                   0) | (root.persistentProfilesAvailable
                                                                                                                         ? EngineCapabilities.PersistentProfiles :
                                                                                                                           0) | (root.certificateDecisionsAvailable
                                                                                                                                 ? EngineCapabilities.CertificateDecisions :
                                                                                                                                   0) | (root.thirdPartyCookieControlAvailable
                                                                                                                                         ? EngineCapabilities.ThirdPartyCookieControl :
                                                                                                                                           0) | (root.pageCertificatesAvailable
                                                                                                                                                 ? EngineCapabilities.PageCertificates :
                                                                                                                                                   0)

    // The lab renders nothing, so find counts the plain occurrences of the
    // query in a body of text a test names. That is enough for the interface:
    // the bar reads a count and a position, and neither cares what drew them.
    property string pageText: ""
    property string findQuery: ""
    property int findMatchCount: 0
    property int findActiveMatch: 0
    property real zoomFactor: 1.0
    property bool siteFullscreenActive: false
    property string siteFullscreenOrigin: ""

    // The lab reaches no network, so what a connection is has to be named. The
    // address alone is what the adapter would know: an https page is reached
    // securely unless a certificate failure has been reported for its origin,
    // and anything that is not http or https is Omaweb's own furniture.
    property string certificateErrorOrigin: ""
    // The lab makes no connection, so the chain an https page arrives over is
    // named: the host's own certificate, an intermediate and a root, in the
    // shape `describeCertificateChain` gives every adapter.
    property var certificateChain: []
    function labCertificateChain(url) {
        const address = String(url);
        if (!address.startsWith("https://") || !root.pageCertificatesAvailable)
            return [];
        const host = root.originLabel(url).split(":")[0];
        return [
                    {
                        "name": host,
                        "subject": "CN=" + host,
                        "issuer": "O=Omaweb Lab, CN=Omaweb Lab Intermediate",
                        "notBefore": "2026-01-01 00:00:00 UTC",
                        "notAfter": "2026-12-31 23:59:59 UTC",
                        "sha256": "11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:00:"
                                  + "11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:00",
                        "subjectAlternativeNames": ["DNS:" + host, "DNS:www." + host],
                        "selfSigned": false
                    },
                    {
                        "name": "Omaweb Lab Intermediate",
                        "subject": "O=Omaweb Lab, CN=Omaweb Lab Intermediate",
                        "issuer": "O=Omaweb Lab, CN=Omaweb Lab Root",
                        "notBefore": "2025-01-01 00:00:00 UTC",
                        "notAfter": "2030-01-01 00:00:00 UTC",
                        "sha256": "22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:00:11:"
                                  + "22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:00:11",
                        "subjectAlternativeNames": [],
                        "selfSigned": false
                    },
                    {
                        "name": "Omaweb Lab Root",
                        "subject": "O=Omaweb Lab, CN=Omaweb Lab Root",
                        "issuer": "O=Omaweb Lab, CN=Omaweb Lab Root",
                        "notBefore": "2020-01-01 00:00:00 UTC",
                        "notAfter": "2040-01-01 00:00:00 UTC",
                        "sha256": "33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:00:11:22:"
                                  + "33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:00:11:22",
                        "subjectAlternativeNames": [],
                        "selfSigned": true
                    }
                ];
    }
    // What a Local-development server usually presents: one certificate that
    // vouches for itself.
    function selfSignedCertificateChain(url) {
        const host = root.originLabel(url).split(":")[0];
        return [
                    {
                        "name": host,
                        "subject": "CN=" + host + ", O=Omaweb Lab",
                        "issuer": "CN=" + host + ", O=Omaweb Lab",
                        "notBefore": "2026-09-03 09:30:47 UTC",
                        "notAfter": "2300-06-19 09:30:47 UTC",
                        "sha256": "44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:00:11:22:33:"
                                  + "44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:00:11:22:33",
                        "subjectAlternativeNames": ["DNS:" + host, "IP:127.0.0.1"],
                        "selfSigned": true
                    }
                ];
    }
    property bool lastLoadFailed: false
    property bool lastLoadNameUnresolved: false
    // The lab loads nothing, so an upgrade that failed is named by hand with
    // `simulateHttpsUpgradeFailure`, and a navigation clears it as a load
    // would.
    property var httpsUpgradeFailure: ({})
    property bool arrivedThroughHttpsUpgrade: false
    function simulateHttpsUpgradeFailure(plainUrl, reason, error) {
        root.httpsUpgradeFailure = {
            "plainUrl": plainUrl,
            "host": String(plainUrl).replace(/^[a-z]+:\/\//, "").split(/[/:?#]/)[0],
            "reason": reason,
            "error": error
        };
    }
    readonly property string connectionState: {
        const address = String(root.currentUrl);
        const separator = address.indexOf("://");
        const scheme = separator === -1 ? "" : address.substring(0, separator).toLowerCase();
        if (root.certificateErrorOrigin.length > 0 && root.certificateErrorOrigin
                === root.originLabel(root.currentUrl))
            return "certificate-error";
        // A page that never arrived is not a page reached over an unencrypted
        // connection: there is no connection to report either way.
        if (root.lastLoadFailed)
            return "internal";
        if (scheme !== "http" && scheme !== "https")
            return "internal";
        return scheme === "https" ? "secure" : "insecure";
    }
    // The lab enables no override, and says so the same way the engine adapter
    // does rather than by being trusted not to.
    readonly property bool insecureContentBlocked: true

    function originLabel(origin) {
        const address = String(origin);
        const scheme = address.indexOf("://");
        const authority = (scheme === -1 ? address : address.substring(scheme + 3)).split("/")[0];
        return authority.length > 0 ? authority : address;
    }

    signal certificateErrorRaised(string requestId, var failure)
    signal pageSiteDataCleared(string origin, var cleared, string error)
    // The lab has no storage to empty, so what a page would have reported is
    // named. An engine whose page cannot answer reports that instead.
    property var pageSiteData: ["local storage", "databases"]
    property string pageSiteDataRefusal: ""
    property int pageSiteDataClearCount: 0
    function clearPageSiteData() {
        const address = String(root.currentUrl);
        if (address.length === 0 || address.startsWith("about:")) {
            root.pageSiteDataCleared("", [], qsTr("there is no page to clear"));
            return;
        }
        root.pageSiteDataClearCount += 1;
        root.pageSiteDataCleared(root.originLabel(root.currentUrl), root.pageSiteDataRefusal.length
                                 > 0 ? [] : root.pageSiteData, root.pageSiteDataRefusal);
    }
    property var pendingCertificateErrors: ({})
    property int nextCertificateErrorId: 0
    property var certificateDecisions: ({})

    // The lab has no certificate to fail, so a failure is named. An engine
    // that cannot report one reports nothing at all, which is what the
    // capability being off has to mean.
    function simulateCertificateError(failure) {
        if (!root.certificateDecisionsAvailable)
            return "";
        const named = failure || ({});
        const url = String(named.url !== undefined ? named.url : root.currentUrl);
        const requestId = String(++root.nextCertificateErrorId);
        const mainFrame = named.mainFrame !== false;
        const chain = named.certificateChain !== undefined ? named.certificateChain :
                                                             root.selfSignedCertificateChain(url);
        if (mainFrame) {
            root.certificateErrorOrigin = root.originLabel(url);
            root.certificateChain = chain;
        }
        root.pendingCertificateErrors[requestId] = {
            "url": url,
            "mainFrame": mainFrame
        };
        root.certificateErrorRaised(requestId, {
                                        "url": url,
                                        "origin": root.originLabel(url),
                                        "description": String(named.description !== undefined
                                                              ? named.description : qsTr(
                                                                    "The certificate could not be verified")),
                                        "overridable": named.overridable !== false,
                                        "mainFrame": mainFrame,
                                        "fatal": named.fatal === true,
                                        "certificateChain": chain
                                    });
        return requestId;
    }

    function respondToCertificateError(requestId, accepted) {
        const pending = root.pendingCertificateErrors[requestId];
        if (!pending)
            return;
        delete root.pendingCertificateErrors[requestId];
        root.certificateDecisions[requestId] = accepted === true;
        // A refused failure is a load that never arrived; an accepted one is
        // this load let through, and the connection stays in error either way.
        if (!accepted && pending.mainFrame)
            root.lastLoadFailed = true;
    }

    // The lab runs no engine and so has no inspector. It reports the capability
    // and hands back a drawn stand-in, which is what makes the dock, its width
    // and the tab it follows reviewable without the QtWebEngine build.
    // `inspectorAvailable` is the other engine the contract has to answer for:
    // one that supplies no inspector at all, whose command must be unavailable
    // rather than offering a dock nothing can fill.
    property bool inspectorAvailable: true
    property bool developerToolsAttached: false
    property var developerToolsView: null
    property var developerToolsColors: ({})
    // How many times the page's own target was asked for, so a test can tell
    // opening the dock from inspecting through it.
    property int inspectedElementCount: 0
    property int contextMenuRequestCount: 0
    property int pageGeneration: 0

    signal pageContextRequested(var context)
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
    signal printFinished(string destination, bool succeeded)
    signal pageCaptured(string destination, bool succeeded, string reason)
    signal userActivated
    // The page verbs, answered as a page with nothing on it would answer
    // them, on the next turn as the engine's own answer comes. The requests
    // are kept so a test can say which reached this page.
    signal agentVerbAnswered(int requestId, var answer)
    signal pageConsoleMessage(int level, string message, int lineNumber, string sourceId,
                              int document)
    property bool pageTakesFocus: true
    property int agentNextLabel: 1
    property bool agentOwned: false
    property string agentDownloadDirectory: ""
    property int agentCancels: 0
    function cancelAgentVerbs() {
        root.agentCancels += 1;
    }
    function releasePageFocus() {
        root.focus = false;
    }
    property var agentRequests: []
    function answerAgentVerb(requestId, verb, args) {
        root.agentRequests = root.agentRequests.concat([
                                                           {
                                                               "requestId": requestId,
                                                               "verb": verb,
                                                               "arguments": args
                                                           }
                                                       ]);
        const look = {
            "title": String(root.pageTitle),
            "url": String(root.currentUrl),
            "outline": "",
            "targets": [],
            "above": 0,
            "below": 0
        };
        const answer = function () {
            // Each step goes through, and a step that carries a `name` is
            // reported as reaching an element the page names so.
            const steps = verb === "do" ? ((args || {}).steps || []).map(function (step) {
                return step.name !== undefined ? {
                                                     "ok": true,
                                                     "name": String(step.name)
                                                 } : {
                    "ok": true
                };
            }) : [];
            root.agentVerbAnswered(requestId, verb === "do" ? {
                                                                  "ok": true,
                                                                  "steps": steps,
                                                                  "look": look
                                                              } : {
                                       "ok": true,
                                       "look": look
                                   });
        };
        if (root.holdAgentAnswers)
            root.heldAgentAnswers.push(answer);
        else
            Qt.callLater(answer);
    }
    // A test holds the page's answers to have an Agent's steps still running
    // while the page reports what they did.
    property bool holdAgentAnswers: false
    property var heldAgentAnswers: []
    function releaseAgentAnswers() {
        const held = root.heldAgentAnswers;
        root.heldAgentAnswers = [];
        root.holdAgentAnswers = false;
        for (const answer of held)
            answer();
    }
    property rect pressOrigin: Qt.rect(0, 0, 0, 0)
    function simulatePress(x, y, width, height) {
        root.pressOrigin = Qt.rect(x, y, width, height);
    }
    // Form history. A test focuses a field and presses the list's keys as
    // the page would report them; the page's own rule for which keys it
    // gives up is mirrored here, so a key the page would keep never arrives.
    property var formField: null
    property int formFieldSerial: 0
    property bool formSuggestionsShown: false
    property bool formSuggestionHighlighted: false
    signal formSubmitted(var fields)
    signal formKeyPressed(string key)
    signal paymentCardSubmitted(var card)
    property var paymentCardFills: []
    // The card the shell last asked the page to fill its form from, and the
    // origin of the frame the focused card field is in, which is the page's
    // own unless a test says otherwise.
    property var filledCard: null
    property string cardFrameOrigin: ""
    function simulateFormFieldFocus(name, value, x, y, width, height, address) {
        root.formFieldSerial += 1;
        root.formSuggestionsShown = false;
        root.formSuggestionHighlighted = false;
        root.formField = {
            "serial": root.formFieldSerial,
            "name": name,
            "address": address || "",
            "value": value,
            "x": x,
            "y": y,
            "width": width,
            "height": height
        };
    }
    // A card field the reader pressed, reported as the page reports one:
    // with its token and whether it is empty, never with what it holds.
    function simulateCardFieldFocus(card, x, y, width, height, frame, serial) {
        root.formFieldSerial = serial !== undefined ? serial : root.formFieldSerial + 1;
        root.formSuggestionsShown = false;
        root.formSuggestionHighlighted = false;
        root.formField = {
            "serial": root.formFieldSerial,
            "frame": frame || "main",
            "origin": root.cardFrameOrigin || root.originOf(root.currentUrl),
            "name": "",
            "address": "",
            "card": card,
            "empty": true,
            "value": "",
            "x": x,
            "y": y,
            "width": width,
            "height": height
        };
    }
    function simulateCardFieldInput() {
        if (root.formField)
            root.formField = Object.assign({}, root.formField, {
                                               "empty": false
                                           });
    }
    function simulateCardFieldBlur() {
        root.simulateFormFieldBlur();
    }
    // How many times the shell asked the page to forget what was typed.
    property int typedInputForgotten: 0
    function forgetTypedInput() {
        root.typedInputForgotten += 1;
    }
    function simulatePaymentCardSubmit(card) {
        root.paymentCardSubmitted(card);
    }
    function fillPaymentCard(card, serial) {
        if (!root.formField || !root.formField.card || serial !== root.formField.serial)
            return;
        root.filledCard = card;
        root.paymentCardFills = root.paymentCardFills.concat([
                                                                 {
                                                                     "last4": card.last4,
                                                                     "brand": card.brand,
                                                                     "nickname": card.nickname,
                                                                     "origin": root.cardFrameOrigin
                                                                               || root.originOf(
                                                                                   root.currentUrl)
                                                                 }
                                                             ]);
    }
    function originOf(url) {
        const match = /^([a-z][a-z0-9+.-]*:\/\/[^\/?#]*)/i.exec(String(url));
        return match ? match[1] : "";
    }
    function simulateFormFieldInput(value) {
        if (!root.formField)
            return;
        root.formField = Object.assign({}, root.formField, {
                                           "value": value
                                       });
    }
    function simulateFormFieldBlur() {
        root.formField = null;
        root.formSuggestionsShown = false;
        root.formSuggestionHighlighted = false;
    }
    function simulateFormSubmit(fields) {
        root.formSubmitted(fields);
    }
    // A key reaches the shell only where the page would have given it up.
    function simulateFormKey(key) {
        const taken = root.formSuggestionsShown && (key === "down" || key === "up" || key
                                                    === "escape" || root.formSuggestionHighlighted);
        if (taken && (key === "escape" || key === "accept")) {
            root.formSuggestionsShown = false;
            root.formSuggestionHighlighted = false;
        }
        if (taken)
            root.formKeyPressed(key);
        return taken;
    }
    function showFormSuggestions(shown, highlighted) {
        root.formSuggestionsShown = shown && root.formField !== null;
        root.formSuggestionHighlighted = root.formSuggestionsShown && highlighted;
    }
    function fillFormField(value) {
        root.simulateFormFieldInput(value);
    }
    // The address the shell last asked the page to fill its form from.
    property var filledAddress: null
    function fillAddress(address, serial) {
        if (root.formField && serial === root.formField.serial)
            root.filledAddress = address;
    }
    function simulateUserActivation() {
        root.userActivated();
    }
    // A page trying to play of its own accord. It gets the sound only where the
    // shell has already said it may.
    function simulateAutoplay() {
        if (!root.autoplayAllowed) {
            root.autoplayBlockedCount += 1;
            return false;
        }
        root.pageAudible = !root.audioMuted;
        return true;
    }
    // The lab has no capability to ask for, so a request is named. The engine's
    // part is to ask and to hear the answer; which answers may be remembered is
    // the core's, and the request goes straight through where one already was.
    property int nextPermissionRequestId: 0
    property var permissionAnswers: ({})
    property int permissionsSettledWithoutAsking: 0
    function simulateSitePermission(origin, permission) {
        const decision = root.permissionController ? root.permissionController.permissionDecision(
                                                         origin, permission) : 0;
        if (decision !== 0) {
            root.permissionsSettledWithoutAsking += 1;
            return "";
        }
        const requestId = String(++root.nextPermissionRequestId);
        root.sitePermissionRequested(requestId, String(origin), String(permission));
        return requestId;
    }
    function respondToPermission(requestId, decision) {
        root.permissionAnswers[requestId] = decision;
    }
    property bool javaScriptDialogsBlocked: false
    property bool lastPromptAccepted: false
    property var lastPromptResponse: ({})
    property int nextBrowserPromptId: 0
    property var pendingExternalProtocols: ({})
    property int externalOpenCount: 0
    property var lastSelectedFiles: []
    property bool fileSelectionCancelled: false
    property string lastContextAction: ""
    property string lastContextDestination: ""
    function respondToBrowserPrompt(requestId, accepted, response) {
        root.lastPromptAccepted = accepted;
        root.lastPromptResponse = response;
        if (response.stopPrompts === true)
            root.javaScriptDialogsBlocked = true;
        const external = root.pendingExternalProtocols[requestId];
        if (external) {
            delete root.pendingExternalProtocols[requestId];
            if (accepted) {
                if (response.remember === true && root.permissionController)
                    root.permissionController.rememberExternalProtocolDecision(external.origin,
                                                                               external.scheme);
                root.externalOpenCount += 1;
            }
        }
    }
    function simulateJavaScriptPrompt(type, origin, message, defaultText) {
        if (root.javaScriptDialogsBlocked)
            return;
        root.browserPromptRequested(String(++root.nextBrowserPromptId), {
                                        "kind": "javascript-" + type,
                                        "origin": String(origin),
                                        "message": String(message),
                                        "defaultText": String(defaultText || "")
                                    });
    }
    function simulateHttpAuthentication(origin, realm) {
        root.browserPromptRequested(String(++root.nextBrowserPromptId), {
                                        "kind": "http-authentication",
                                        "origin": String(origin),
                                        "message": qsTr("Sign in to %1").arg(String(origin)),
                                        "detail": String(realm)
                                    });
    }
    // One request at a time, as an engine runs one for a page: each step is
    // sent under the same id until a `closed` step ends it. A decline ends
    // it on the next turn of the event loop, as the Qt engine does.
    property var securityKeyTransports: ["usb"]
    property string securityKeyRequestId: ""
    property int nextSecurityKeyId: 0
    property var lastSecurityKeyAnswer: ({})
    signal securityKeyRequested(string requestId, var step)
    function simulateSecurityKey(step) {
        if (root.securityKeyRequestId.length === 0)
            root.securityKeyRequestId = String(++root.nextSecurityKeyId);
        const requestId = root.securityKeyRequestId;
        if (step.state === "closed")
            root.securityKeyRequestId = "";
        const page = String(root.currentUrl);
        const match = page.match(/^([a-z][a-z0-9+.-]*:\/\/[^/]+)/i);
        root.securityKeyRequested(requestId, Object.assign({
                                                               "site": match ? match[1] : page
                                                           }, step));
        return requestId;
    }
    function respondToSecurityKey(requestId, answer) {
        if (requestId !== root.securityKeyRequestId)
            return;
        root.lastSecurityKeyAnswer = answer;
        if (answer.action === "cancel")
            Qt.callLater(root.simulateSecurityKey, {
                             "state": "closed"
                         });
    }
    function simulateExternalProtocol(application, destination) {
        const address = String(destination);
        const scheme = address.substring(0, address.indexOf(":"));
        if (root.permissionController && root.permissionController.externalProtocolAllowed(
                    root.currentUrl, scheme)) {
            root.externalOpenCount += 1;
            return;
        }
        const requestId = String(++root.nextBrowserPromptId);
        const page = String(root.currentUrl);
        const match = page.match(/^([a-z][a-z0-9+.-]*:\/\/[^/]+)/i);
        const origin = match ? match[1] : page;
        root.pendingExternalProtocols[requestId] = {
            "origin": origin,
            "scheme": scheme,
            "destination": address
        };
        root.browserPromptRequested(requestId, {
                                        "kind": "external-protocol",
                                        "application": String(application),
                                        "scheme": scheme,
                                        "origin": origin,
                                        "destination": address,
                                        "message": qsTr("Open %1?").arg(String(application)),
                                        "detail": scheme + " · " + origin + " · " + address
                                    });
    }
    function simulateFileSelection(mode, mimeTypes) {
        root.fileSelectionRequested(String(++root.nextBrowserPromptId), {
                                        "mode": String(mode),
                                        "mimeTypes": mimeTypes || [],
                                        "suggestedName": ""
                                    });
    }
    function respondToFileSelection(requestId, files) {
        root.lastSelectedFiles = files;
        root.fileSelectionCancelled = files.length === 0;
    }
    function performPageContextAction(action, destination) {
        root.lastContextAction = String(action);
        root.lastContextDestination = String(destination);
    }

    // The lab refuses nothing, so its tally is always zero — but it says which
    // document it is showing anyway, because an adapter that does not is one
    // the chrome reads no tally for at all.
    function announcePage(pageAddress) {
        if (root.contentBlocker)
            root.contentBlocker.showPage(root, root.spaceId, pageAddress, root.pageGeneration);
    }

    Component.onCompleted: {
        root.announcePage(root.currentUrl);
        root.certificateChain = root.labCertificateChain(root.currentUrl);
    }

    onCurrentUrlChanged: {
        root.pageGeneration += 1;
        root.paymentCardFills = [];
        root.documentPainted = root.paintsAtOnce(root.currentUrl);
        root.announcePage(root.currentUrl);
        root.javaScriptDialogsBlocked = false;
        root.lastLoadFailed = false;
        // The failure belonged to the origin being left, and nothing wrote the
        // exception down, so arriving somewhere else clears the report.
        if (root.certificateErrorOrigin.length > 0 && root.certificateErrorOrigin
                !== root.originLabel(root.currentUrl)) {
            root.certificateErrorOrigin = "";
        }
        // A page on the origin whose certificate failed arrives over the same
        // certificate.
        if (root.certificateErrorOrigin.length === 0)
            root.certificateChain = root.labCertificateChain(root.currentUrl);
        pageLocalState = "";
        root.httpsUpgradeFailure = ({});
        // The matches were in the page that has just been replaced. The query
        // is the reader's and stays.
        root.forgetFindMatches();
    }

    color: root.pageBackgroundColor

    Keys.onPressed: function (event) {
        if (event.text.length > 0)
            root.keyboardInput += event.text.toLowerCase();
    }

    function goBack() {
        root.backCount += 1;
    }
    function goForward() {
        root.forwardCount += 1;
    }
    function focusPage() {
        root.forceActiveFocus();
    }
    function reloadPage() {
        loading = true;
        root.pageGeneration += 1;
        root.paymentCardFills = [];
        root.announcePage(root.currentUrl);
        settle.restart();
    }
    function reloadPageBypassingCache() {
        root.bypassedCacheCount += 1;
        root.reloadPage();
    }
    function stopLoading() {
        root.stoppedLoadCount += 1;
        root.loading = false;
        settle.stop();
    }
    // How many times the page was asked to be read again from the network, and
    // to stop reading: three asks that look alike from outside have to be told
    // apart from inside.
    property int bypassedCacheCount: 0
    property int stoppedLoadCount: 0
    property int backCount: 0
    property int forwardCount: 0

    function forgetFindMatches() {
        root.findMatchCount = 0;
        root.findActiveMatch = 0;
    }

    function findText(query, forward) {
        root.findQuery = String(query);
        if (root.findQuery.length === 0) {
            root.forgetFindMatches();
            return;
        }
        const haystack = root.pageText.toLowerCase();
        const needle = root.findQuery.toLowerCase();
        let matches = 0;
        for (let at = haystack.indexOf(needle); at !== -1; at = haystack.indexOf(needle, at + 1)) {
            matches += 1;
        }
        root.findMatchCount = matches;
        if (matches === 0) {
            root.findActiveMatch = 0;
            return;
        }
        const step = forward ? 1 : -1;
        const next = root.findActiveMatch === 0 ? (forward ? 1 : matches) : ((root.findActiveMatch
                                                                              - 1 + step + matches)
                                                                             % matches) + 1;
        root.findActiveMatch = next;
    }

    function clearFind() {
        root.findQuery = "";
        root.forgetFindMatches();
    }

    function setZoomFactor(factor) {
        const wanted = Number(factor);
        if (!(wanted > 0))
            return;
        root.zoomFactor = wanted;
    }

    // The lab has no printing system, so rendering is reported rather than
    // done: what the shell has to get right is naming a destination and hearing
    // the answer.
    function printPage(destination) {
        const path = String(destination);
        root.printFinished(path, path.length > 0);
    }

    // The lab's stand-in page is what a capture takes, drawn the same way the
    // engine's page would be: the view alone, at the display's pixel density.
    function capturePage(destination) {
        const path = String(destination);
        const grabbing = path.length > 0 && root.width > 0 && root.height > 0 && root.grabToImage(
                  function (result) {
                      root.pageCaptured(path, result.saveToFile(path), "");
                  });
        if (!grabbing)
            root.pageCaptured(path, false, "");
    }

    // The whole stand-in document, taken the way the engine takes a page: a
    // screenful at a time, scrolled, grabbed and joined, and scrolled back.
    // With no document set the page is one screenful long.
    function capturePageFully(destination) {
        const path = String(destination);
        const viewport = root.height;
        const length = Math.max(root.pageScrollLength, viewport);
        if (path.length === 0 || !(viewport > 0)) {
            root.pageCaptured(path, false, "");
            return;
        }
        const window = root.Window.window;
        const refusal = PageImages.heightRefusal(length, window && window.devicePixelRatio > 0
                                                 ? window.devicePixelRatio : 1);
        if (refusal.length > 0) {
            root.pageCaptured(path, false, refusal);
            return;
        }
        const tops = PageImages.stripTops(length, viewport);
        const kept = root.pageScrollOffset;
        const strips = [];
        const step = function (index) {
            if (index >= tops.length) {
                root.pageScrollOffset = kept;
                root.pageCaptured(path, PageImages.join(strips, tops, length, viewport, path), "");
                return;
            }
            root.pageScrollOffset = tops[index];
            const strip = PageImages.reserveStrip();
            const grabbing = root.grabToImage(function (result) {
                strips.push(strip);
                if (result.saveToFile(strip))
                    step(index + 1);
                else
                    fail();
            });
            if (!grabbing)
                fail();
        };
        const fail = function () {
            PageImages.join(strips, [], 0, 0, "");
            root.pageScrollOffset = kept;
            root.pageCaptured(path, false, "");
        };
        step(0);
    }

    function exitSiteFullscreen() {
        if (!root.siteFullscreenActive)
            return;
        root.siteFullscreenActive = false;
        root.siteFullscreenOrigin = "";
    }

    // The lab has no page to ask for the screen, so a request is named.
    function simulateSiteFullscreen(origin) {
        // The origin is named before the state changes: the shell reports who
        // took the screen the moment it hears that someone did.
        const named = String(origin || "");
        root.siteFullscreenOrigin = named;
        root.siteFullscreenActive = named.length > 0;
    }
    function configureKeyboardNavigation(configuration) {
        keyboardNavigationConfiguration = configuration;
    }
    function invokeMediaAction(command) {
        root.lastMediaAction = String(command);
    }
    function checkForEditedFormState(callback) {
        callback(false);
    }
    function attachDeveloperTools() {
        if (!root.inspectorAvailable || root.developerToolsAttached)
            return;
        const view = root.mockDeveloperToolsComponent.createObject(root);
        if (!view)
            return;
        root.developerToolsView = view;
        root.developerToolsAttached = true;
    }
    function detachDeveloperTools() {
        if (!root.developerToolsAttached)
            return;
        root.developerToolsAttached = false;
        const view = root.developerToolsView;
        root.developerToolsView = null;
        if (view)
            view.destroy();
    }
    function inspectElement() {
        if (!root.inspectorAvailable)
            return;
        root.attachDeveloperTools();
        root.inspectedElementCount += 1;
    }
    function simulateDeveloperToolsClose() {
        root.developerToolsClosed();
    }
    // The lab has no engine to right-click, so a context is handed over by
    // name. Anything the caller leaves out is what a click on bare page
    // background reports.
    function simulateContextMenu(context) {
        const named = context || ({});
        root.pageContextRequested({
                                      "x": named.x !== undefined ? named.x : 40,
                                      "y": named.y !== undefined ? named.y : 30,
                                      "selectedText": named.selectedText !== undefined
                                                      ? named.selectedText : "",
                                      "linkText": named.linkText !== undefined ? named.linkText : "",
                                      "linkUrl": named.linkUrl !== undefined ? named.linkUrl : "",
                                      "mediaUrl": named.mediaUrl !== undefined ? named.mediaUrl : "",
                                      "mediaType": named.mediaType !== undefined ? named.mediaType :
                                                                                   "none",
                                      "editable": named.editable === true,
                                      "pageGeneration": root.pageGeneration
                                  });
    }
    function requestPageContextMenu() {
        root.contextMenuRequestCount += 1;
        root.simulateContextMenu({
                                     "x": root.width / 2,
                                     "y": root.height / 2
                                 });
    }

    property Component mockDeveloperToolsComponent: Component {
        Rectangle {
            objectName: "mockDeveloperToolsView"
            color: root.developerToolsColors.windowOpaque !== undefined
                   ? root.developerToolsColors.windowOpaque : "#16151d"

            Text {
                anchors.centerIn: parent
                width: parent.width - 24
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: qsTr("No inspector: this engine is the UI lab's stand-in")
                color: root.developerToolsColors.mutedText !== undefined
                       ? root.developerToolsColors.mutedText : "#aaa5b7"
                font.pixelSize: 12
            }
        }
    }
    function acceptNewWindowRequest(request) {
        if (request && request.requestedUrl)
            currentUrl = request.requestedUrl;
    }
    function simulateAudible(audible) {
        root.pageAudible = audible;
    }
    function simulateMediaSession(declaration) {
        root.pageMediaSession = declaration ? declaration : {};
    }
    function simulateRendererFailure() {
        rendererFailed(qsTr("Renderer exited unexpectedly"));
    }
    function simulateNewWindowRequest(requestedUrl, auxiliary) {
        const request = {
            "requestedUrl": requestedUrl
        };
        if (auxiliary)
            auxiliaryWindowRequested(request, requestedUrl);
        else
            newTabRequested(request, requestedUrl);
    }
    function simulateBackgroundTabRequest(requestedUrl) {
        backgroundTabRequested(requestedUrl);
    }
    function simulateWindowCloseRequest() {
        windowCloseRequested();
    }

    Timer {
        id: settle
        interval: 180
        onTriggered: root.loading = false
    }

    // The lab draws no document, so the wheel moves the offset the page would
    // have reported, by the wheel's own step and no further than the page
    // runs. That the offset moved is how a test knows the wheel reached the
    // page rather than something standing over it.
    WheelHandler {
        onWheel: function (event) {
            const travel = Math.max(0, root.pageScrollLength - root.pageViewportLength);
            root.pageScrollOffset = Math.min(travel, Math.max(0, root.pageScrollOffset
                                                              - event.angleDelta.y));
        }
    }

    Row {
        anchors.fill: parent
        visible: root.blurReviewPattern

        Repeater {
            model: Math.ceil(root.width / 96)

            Rectangle {
                required property int index
                width: 96
                height: root.height
                color: index % 2 === 0 ? "#f4f0ff" : "#241832"
            }
        }
    }

    // A stand-in document as long as `pageScrollLength`, in bands a hundred
    // points tall that alternate colour, scrolled by `pageScrollOffset`, so a
    // review can tell one part of a long page from another.
    Item {
        anchors.fill: parent
        clip: true
        visible: root.documentReview

        Column {
            y: -root.pageScrollOffset
            width: parent.width

            Repeater {
                model: Math.ceil(root.pageScrollLength / 100)

                Rectangle {
                    required property int index
                    width: parent.width
                    height: 100
                    color: index % 2 === 0 ? "#d9f2e6" : "#3a1d4f"
                }
            }
        }
    }

    Rectangle {
        anchors.centerIn: parent
        width: 160
        height: 160
        radius: 24
        color: "#7c6cff"
        visible: root.motionReview

        RotationAnimation on rotation {
            running: root.motionReview
            from: 0
            to: 360
            duration: 1200
            loops: Animation.Infinite
        }
    }

    Column {
        anchors.centerIn: parent
        spacing: 10
        visible: !root.samplePage && !root.documentReview

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("Omaweb UI lab")
            color: "#25232b"
            font.pixelSize: 28
            font.weight: Font.DemiBold
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.currentUrl.toString()
            color: "#696473"
            font.pixelSize: 14
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("No browser engine is running")
            color: "#918a9b"
            font.pixelSize: 12
        }
    }

    // A documentation page, drawn rather than loaded: a site bar, the article
    // the tab is named for, and a contents column where the window has room.
    // Its words are the lab's own. Colours come from the page palette, so the
    // page is the one a site that follows the reader's theme would draw.
    Rectangle {
        id: samplePage
        anchors.fill: parent
        visible: root.samplePage
        clip: true

        readonly property var colors: root.pagePalette
        readonly property color ground: colors ? colors.bg : root.pageBackgroundColor
        readonly property color raised: colors ? colors.sidebar : Qt.darker(ground, 1.2)
        readonly property color ink: colors ? colors.fg : "#d8dbe3"
        readonly property color accent: colors ? colors.accent : "#7c6cff"
        readonly property color hot: colors ? colors.urgent : "#f38ba8"
        readonly property color quiet: colors ? colors.muted : "#929ca6"
        readonly property color rule: Qt.rgba(ink.r, ink.g, ink.b, 0.12)
        // The theme's resolved family, read off the lab's theme rather than
        // the kit: the engine contract tests load this view without the kit
        // on the import path, and without a theme the page is never drawn.
        readonly property string family: typeof theme !== "undefined" && theme && theme.palette
                                         && theme.palette.font ? theme.palette.font.family : ""
        readonly property real measure: Math.min(720, width - 96)
        readonly property bool contents: width > 1100
        readonly property string host: {
            const match = String(root.currentUrl).match(/^https?:\/\/([^\/]+)/);
            return match ? match[1] : "";
        }
        // The photo essay `--browse` ends on, in place of the documentation
        // page every other address draws.
        readonly property bool pictures: host === "afterdark.example"

        color: ground

        Rectangle {
            id: siteBar
            visible: !samplePage.pictures
            width: parent.width
            height: 56
            color: samplePage.raised

            // In line with the article, and never under the window controls
            // that float over the page's top left while the sidebar is hidden.
            Rectangle {
                id: siteMark
                x: Math.max(article.x, 216)
                anchors.verticalCenter: parent.verticalCenter
                width: 22
                height: 22
                radius: 5
                color: samplePage.accent
            }
            Text {
                anchors.left: siteMark.right
                anchors.leftMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                text: "Qt Documentation"
                color: samplePage.ink
                font.family: samplePage.family
                font.pixelSize: 15
                font.weight: Font.DemiBold
            }
            Row {
                anchors.right: parent.right
                anchors.rightMargin: 32
                anchors.verticalCenter: parent.verticalCenter
                spacing: 28

                Repeater {
                    model: ["Guides", "Reference", "Examples", "Search"]

                    Text {
                        required property string modelData
                        text: modelData
                        color: samplePage.quiet
                        font.family: samplePage.family
                        font.pixelSize: 13
                    }
                }
            }
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: samplePage.rule
            }
        }

        Column {
            id: article
            visible: !samplePage.pictures
            x: samplePage.contents ? Math.max(48, (samplePage.width - samplePage.measure - 260) / 2) :
                                     (samplePage.width - samplePage.measure) / 2
            y: siteBar.height + 44
            width: samplePage.measure
            spacing: 18

            Text {
                text: "Qt 6  ›  Qt Quick  ›  Rendering"
                color: samplePage.quiet
                font.family: samplePage.family
                font.pixelSize: 12
            }
            Text {
                width: parent.width
                text: "Qt Quick Scene Graph"
                wrapMode: Text.WordWrap
                color: samplePage.ink
                font.family: samplePage.family
                font.pixelSize: 34
                font.weight: Font.DemiBold
            }
            Text {
                width: parent.width
                text: "Every Qt Quick window draws through a scene graph: a tree of nodes that "
                      + "says what each item looks like. Once a frame the renderer walks the tree "
                      + "and groups what shares a material, so the GPU is asked for as few draw "
                      + "calls as the scene allows."
                wrapMode: Text.WordWrap
                lineHeight: 1.35
                color: samplePage.ink
                opacity: 0.85
                font.family: samplePage.family
                font.pixelSize: 15
            }
            Text {
                topPadding: 8
                text: "A thread of its own"
                color: samplePage.ink
                font.family: samplePage.family
                font.pixelSize: 21
                font.weight: Font.DemiBold
            }
            Text {
                width: parent.width
                text: "Where the platform allows it, rendering runs on a separate thread. The "
                      + "application thread changes items and hands over what changed, and "
                      + "animations keep their pace while the application is busy."
                wrapMode: Text.WordWrap
                lineHeight: 1.35
                color: samplePage.ink
                opacity: 0.85
                font.family: samplePage.family
                font.pixelSize: 15
            }
            Rectangle {
                width: parent.width
                height: code.implicitHeight + 36
                radius: 6
                color: samplePage.raised
                border.width: 1
                border.color: samplePage.rule

                Text {
                    id: code
                    x: 20
                    y: 18
                    textFormat: Text.StyledText
                    lineHeight: 1.4
                    color: samplePage.ink
                    font.family: samplePage.family
                    font.pixelSize: 14
                    text: "<font color='" + samplePage.accent + "'>Rectangle</font> {<br>"
                          + "&nbsp;&nbsp;&nbsp;&nbsp;width: <font color='" + samplePage.hot
                          + "'>320</font>; height: <font color='" + samplePage.hot
                          + "'>200</font><br>" + "&nbsp;&nbsp;&nbsp;&nbsp;color: <font color='"
                          + samplePage.hot + "'>\"steelblue\"</font><br>"
                          + "&nbsp;&nbsp;&nbsp;&nbsp;layer.enabled: <font color='"
                          + samplePage.accent + "'>true</font><br>}"
                }
            }
            Rectangle {
                width: parent.width
                height: 170
                radius: 6
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop {
                        position: 0
                        color: Qt.rgba(samplePage.accent.r, samplePage.accent.g, samplePage.accent.b,
                                       0.35)
                    }
                    GradientStop {
                        position: 1
                        color: Qt.rgba(samplePage.hot.r, samplePage.hot.g, samplePage.hot.b, 0.25)
                    }
                }

                Text {
                    anchors.centerIn: parent
                    text: "item  →  node  →  batch  →  frame"
                    color: samplePage.ink
                    font.family: samplePage.family
                    font.pixelSize: 16
                }
            }
            Text {
                width: parent.width
                text: "An item that needs more than rectangles and text builds its own nodes, "
                      + "handing the renderer geometry and a material directly."
                wrapMode: Text.WordWrap
                lineHeight: 1.35
                color: samplePage.ink
                opacity: 0.85
                font.family: samplePage.family
                font.pixelSize: 15
            }
        }

        Column {
            visible: samplePage.contents && !samplePage.pictures
            x: article.x + article.width + 64
            y: article.y + 4
            width: 196
            spacing: 12

            Text {
                text: "ON THIS PAGE"
                color: samplePage.quiet
                font.family: samplePage.family
                font.pixelSize: 11
                font.letterSpacing: 1.2
            }
            Repeater {
                model: ["Overview", "A thread of its own", "Batching", "Custom geometry",
                    "Profiling"]

                Text {
                    required property string modelData
                    required property int index
                    text: modelData
                    color: index === 1 ? samplePage.accent : samplePage.quiet
                    font.family: samplePage.family
                    font.pixelSize: 13
                }
            }
        }

        Item {
            visible: samplePage.pictures
            anchors.fill: parent

            Rectangle {
                id: essayBar
                width: parent.width
                height: 56
                color: samplePage.ground

                Text {
                    x: Math.max(essay.x, 216)
                    anchors.verticalCenter: parent.verticalCenter
                    text: "AFTER DARK"
                    color: samplePage.ink
                    font.family: samplePage.family
                    font.pixelSize: 15
                    font.weight: Font.Bold
                    font.letterSpacing: 3
                }
                Row {
                    anchors.right: parent.right
                    anchors.rightMargin: 32
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 28

                    Repeater {
                        model: ["Photo", "Essays", "Gear", "About"]

                        Text {
                            required property string modelData
                            text: modelData
                            color: samplePage.quiet
                            font.family: samplePage.family
                            font.pixelSize: 13
                        }
                    }
                }
            }

            Scene {
                id: hero
                y: essayBar.height
                width: parent.width
                height: Math.round(parent.height * 0.46)
                ground: samplePage.ground
                accent: samplePage.accent
                hot: samplePage.hot
                ink: samplePage.ink
            }

            Column {
                id: essay
                x: (parent.width - width) / 2
                y: hero.y + hero.height + 36
                width: Math.min(820, parent.width - 96)
                spacing: 14

                Text {
                    text: "PHOTO ESSAY  ·  6 MIN READ"
                    color: samplePage.accent
                    font.family: samplePage.family
                    font.pixelSize: 12
                    font.letterSpacing: 1.4
                }
                Text {
                    width: parent.width
                    text: "Night drive"
                    color: samplePage.ink
                    font.family: samplePage.family
                    font.pixelSize: 40
                    font.weight: Font.Bold
                }
                Text {
                    width: parent.width
                    text: "Four hundred kilometres of empty road between two cities, driven "
                          + "after midnight with the radio off and the sky doing all the talking."
                    wrapMode: Text.WordWrap
                    lineHeight: 1.35
                    color: samplePage.ink
                    opacity: 0.85
                    font.family: samplePage.family
                    font.pixelSize: 16
                }
                Row {
                    topPadding: 10
                    spacing: 16

                    Repeater {
                        model: [["Last light", 0.28, 0.22, 3], ["Moonrise", 0.72, 0.18, 5], ["The long straight",
                                                                                             0.5, 0.34,
                                                                                             8]]

                        Column {
                            required property var modelData
                            spacing: 8

                            Rectangle {
                                width: (essay.width - 32) / 3
                                height: width * 0.62
                                radius: 6
                                color: samplePage.raised
                                clip: true

                                Scene {
                                    anchors.fill: parent
                                    sunX: modelData[1]
                                    sunSize: modelData[2]
                                    seed: modelData[3]
                                    ground: samplePage.ground
                                    accent: samplePage.accent
                                    hot: samplePage.hot
                                    ink: samplePage.ink
                                }
                            }
                            Text {
                                text: modelData[0]
                                color: samplePage.quiet
                                font.family: samplePage.family
                                font.pixelSize: 13
                            }
                        }
                    }
                }
            }
        }
    }

    // A photo essay, drawn rather than loaded: a hero picture across the
    // page and a strip of smaller ones under the article. The pictures are
    // one night-road scene painted from the page palette, so every theme
    // gets its own version of it rather than a photo in someone else's
    // colours.
    component Scene: Canvas {
        id: scene

        property real sunX: 0.5
        property real sunSize: 0.3
        property real horizon: 0.64
        property int seed: 1
        // Handed in by the page: an inline component has its own scope, so it
        // cannot reach the page's palette by id.
        property color ground
        property color accent
        property color hot
        property color ink

        function mix(a, b, amount) {
            return Qt.rgba(a.r + (b.r - a.r) * amount, a.g + (b.g - a.g) * amount, a.b + (b.b
                                                                                          - a.b) * amount,
                           1);
        }
        function fade(color, alpha) {
            return Qt.rgba(color.r, color.g, color.b, alpha);
        }
        function noise(index) {
            const value = Math.sin(index * 12.9898 + scene.seed * 78.233) * 43758.5453;
            return value - Math.floor(value);
        }

        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onGroundChanged: requestPaint()
        onAccentChanged: requestPaint()

        onPaint: {
            const context = getContext("2d");
            const w = width;
            const h = height;
            const line = h * horizon;
            const night = Qt.darker(ground, 1.6);
            context.reset();

            const sky = context.createLinearGradient(0, 0, 0, line);
            sky.addColorStop(0, night);
            sky.addColorStop(0.55, mix(night, accent, 0.28));
            sky.addColorStop(1, mix(accent, hot, 0.55));
            context.fillStyle = sky;
            context.fillRect(0, 0, w, line);

            context.fillStyle = fade(ink, 0.7);
            for (let star = 0; star < 90; ++star) {
                const size = noise(star * 3) > 0.9 ? 2 : 1;
                context.fillRect(noise(star) * w, noise(star + 500) * line * 0.7, size, size);
            }

            // The sun, and the bands cut out of its lower half.
            const radius = h * sunSize;
            const centre = sunX * w;
            const sunTop = line - radius * 1.25;
            const sun = context.createLinearGradient(0, sunTop, 0, line);
            sun.addColorStop(0, mix(hot, ink, 0.25));
            sun.addColorStop(1, hot);
            context.fillStyle = sun;
            context.beginPath();
            context.arc(centre, line - radius * 0.25, radius, 0, Math.PI * 2);
            context.fill();
            context.save();
            context.clip();
            context.fillStyle = mix(night, accent, 0.4);
            for (let band = 0; band < 6; ++band) {
                const top = line - radius * 0.2 - band * radius * 0.16;
                context.fillRect(centre - radius, top, radius * 2, 2 + (5 - band) * 1.4);
            }
            context.restore();

            // Two ranges of mountains, the far one lit by the sky.
            const ranges = [[0.14, mix(night, accent, 0.5), 11], [0.08, night, 7]];
            for (let range = 0; range < ranges.length; ++range) {
                const height = h * ranges[range][0];
                context.fillStyle = ranges[range][1];
                context.beginPath();
                context.moveTo(0, line);
                const steps = ranges[range][2];
                for (let step = 0; step <= steps; ++step) {
                    const x = step / steps * w;
                    const peak = noise(step * 7 + range * 31);
                    const away = Math.abs(x - centre) / w;
                    context.lineTo(x, line - height * (0.35 + peak) * Math.min(1, away * 3));
                }
                context.lineTo(w, line);
                context.closePath();
                context.fill();
            }

            // The road plane and its grid, running to the sun.
            context.fillStyle = night;
            context.fillRect(0, line, w, h - line);
            context.strokeStyle = fade(accent, 0.65);
            context.lineWidth = 1.5;
            for (let row = 1; row < 12; ++row) {
                const y = line + Math.pow(row / 11, 2.2) * (h - line);
                context.beginPath();
                context.moveTo(0, y);
                context.lineTo(w, y);
                context.stroke();
            }
            for (let column = -12; column <= 12; ++column) {
                context.beginPath();
                context.moveTo(centre + column * w * 0.006, line);
                context.lineTo(centre + column * w * 0.16, h);
                context.stroke();
            }
            context.fillStyle = fade(hot, 0.9);
            context.fillRect(centre - 2, line + (h - line) * 0.2, 4, (h - line) * 0.8);
        }
    }
}
