import QtQuick
import QtWebEngine

Item {
    id: root

    property alias currentUrl: webView.url
    property alias pageTitle: webView.title
    property alias loading: webView.loading
    property alias canGoBack: webView.canGoBack
    property alias canGoForward: webView.canGoForward
    property string profilePath: ""
    property var sharedProfile: null
    property var permissionController: null
    property var contentBlocker: null
    property var engineContentBlocker: null
    readonly property var browserProfile: webView.profile
    readonly property bool pageHasFocus: webView.activeFocus
    readonly property int capabilities: 91
    property int blockedRequestCount: 0
    property var keyboardNavigationConfiguration: ({})
    property var editedStateScript: {
        const script = WebEngine.script()
        script.name = "Tanto edited form state"
        script.injectionPoint = WebEngineScript.DocumentReady
        script.worldId = WebEngineScript.MainWorld
        script.runsOnSubFrames = false
        script.sourceCode = "globalThis.__tantoContentEditableEdited = false;"
            + "document.addEventListener('input', event => {"
            + "if (event.target && event.target.isContentEditable) "
            + "globalThis.__tantoContentEditableEdited = true;"
            + "}, true);"
        return script
    }

    signal rendererFailed(string reason)
    signal newTabRequested(var request, url requestedUrl)
    signal auxiliaryWindowRequested(var request, url requestedUrl)
    signal windowCloseRequested()
    signal backgroundTabRequested(url requestedUrl)
    signal sitePermissionRequested(string requestId, string origin, string permission)
    property var pendingPermissions: ({})
    property int nextPermissionRequestId: 0

    function respondToPermission(requestId, decision) {
        const request = pendingPermissions[requestId]
        if (!request) return
        delete pendingPermissions[requestId]
        if (decision === 1 || decision === 2) request.grant()
        else request.deny()
    }

    function goBack() { webView.goBack() }
    function goForward() { webView.goForward() }
    function focusPage() { webView.forceActiveFocus() }
    function reloadPage() { webView.reload() }
    function configureKeyboardNavigation(configuration) {
        keyboardNavigationConfiguration = configuration
        applyKeyboardNavigationConfiguration()
    }
    function applyKeyboardNavigationConfiguration() {
        if (!webView || !keyboardNavigationConfiguration.version) return
        webView.runJavaScript("globalThis.__tantoKeyboardNavigation && "
            + "globalThis.__tantoKeyboardNavigation.configure("
            + JSON.stringify(keyboardNavigationConfiguration) + ");")
    }
    function applyCosmeticRules() {
        if (!contentBlocker || loading) return
        const css = contentBlocker.cosmeticStyleSheet(currentUrl)
        webView.runJavaScript(
            "(() => { let style = document.getElementById('__tanto_content_blocking');"
            + "if (!style) { style = document.createElement('style');"
            + "style.id = '__tanto_content_blocking'; document.documentElement.append(style); }"
            + "style.textContent = " + JSON.stringify(css) + "; })()")
    }
    function checkForEditedFormState(callback) {
        webView.runJavaScript(
            "(() => {"
            + "for (const field of document.querySelectorAll('input, textarea')) {"
            + "if (field.type === 'checkbox' || field.type === 'radio') {"
            + "if (field.checked !== field.defaultChecked) return true;"
            + "} else if (field.value !== field.defaultValue) return true;"
            + "}"
            + "for (const option of document.querySelectorAll('select option')) {"
            + "if (option.selected !== option.defaultSelected) return true;"
            + "}"
            + "return Boolean(globalThis.__tantoContentEditableEdited);"
            + "})()",
            callback)
    }
    function acceptNewWindowRequest(request) {
        if (request) request.openIn(webView)
    }
    function isAuxiliaryDestination(destination) {
        return destination === WebEngineNewWindowRequest.InNewDialog
    }

    WebEngineProfile {
        id: spaceProfile
        storageName: "tanto-space"
        persistentStoragePath: root.profilePath
        cachePath: root.profilePath + "/cache"
        persistentCookiesPolicy: WebEngineProfile.ForcePersistentCookies
        httpCacheType: WebEngineProfile.DiskHttpCache
    }

    Component.onCompleted: {
        if (!root.sharedProfile && root.engineContentBlocker)
            root.engineContentBlocker.attachToProfile(spaceProfile)
        if (root.contentBlocker)
            root.blockedRequestCount = root.contentBlocker.blockedRequestCount(root.currentUrl)
        Qt.callLater(root.applyKeyboardNavigationConfiguration)
    }

    Connections {
        target: root.contentBlocker
        ignoreUnknownSignals: true

        function refresh() {
            root.blockedRequestCount = root.contentBlocker
                ? root.contentBlocker.blockedRequestCount(root.currentUrl) : 0
            root.applyCosmeticRules()
        }

        function onBlockedRequestCountChanged(siteUrl) { refresh() }
        function onConfigurationChanged() { refresh() }
        function onRulesChanged() { refresh() }
    }

    property var keyboardNavigationScript: {
        const script = WebEngine.script()
        script.name = "Tanto Keyboard navigation"
        script.injectionPoint = WebEngineScript.DocumentReady
        script.worldId = WebEngineScript.MainWorld
        script.runsOnSubFrames = false
        script.sourceCode = `(() => {
            if (globalThis.__tantoKeyboardNavigation) return;
            const state = {
                config: { enabled: false, bindings: {}, passthroughAll: false, passthroughKeys: [] },
                prefix: '', prefixTimer: 0, hintInput: '', hints: [], hintMode: ''
            };
            const keyName = event => event.shiftKey && event.key.toLowerCase() === 'f'
                ? 'Shift+F' : event.key;
            const editable = target => target && (target.isContentEditable
                || (typeof target.closest === 'function'
                    && target.closest('input, textarea, select, [contenteditable="true"]')));
            const visible = element => {
                const style = getComputedStyle(element);
                const rect = element.getBoundingClientRect();
                return style.visibility !== 'hidden' && style.display !== 'none'
                    && rect.width > 0 && rect.height > 0
                    && rect.bottom >= 0 && rect.right >= 0
                    && rect.top <= innerHeight && rect.left <= innerWidth;
            };
            const labelFor = index => {
                const alphabet = 'asdfghjklqwertyuiopzxcvbnm';
                let label = '';
                do { label = alphabet[index % alphabet.length] + label;
                    index = Math.floor(index / alphabet.length) - 1; } while (index >= 0);
                return label;
            };
            const clearHints = () => {
                document.getElementById('__tanto_link_hints')?.remove();
                state.hints = []; state.hintInput = ''; state.hintMode = '';
            };
            const clearPrefix = () => {
                state.prefix = ''; clearTimeout(state.prefixTimer);
                document.getElementById('__tanto_key_sequence')?.remove();
            };
            const showPrefix = key => {
                document.getElementById('__tanto_key_sequence')?.remove();
                const indicator = document.createElement('span');
                indicator.id = '__tanto_key_sequence';
                indicator.textContent = key + '…';
                indicator.setAttribute('role', 'status');
                indicator.setAttribute('aria-label', 'Keyboard sequence ' + key);
                indicator.style.cssText = 'position:fixed;right:1rem;bottom:1rem;z-index:2147483647;'
                    + 'padding:.25em .5em;border:2px solid ButtonText;border-radius:.25em;'
                    + 'background:ButtonFace;color:ButtonText;font:700 max(12px,.8rem) system-ui,sans-serif;';
                document.documentElement.append(indicator);
            };
            const showHints = command => {
                clearHints();
                const candidates = [...document.querySelectorAll(
                    'a[href], button, input:not([type="hidden"]), select, textarea, [role="link"], [role="button"], [tabindex]')]
                    .filter(visible);
                const overlay = document.createElement('div');
                overlay.id = '__tanto_link_hints';
                overlay.setAttribute('role', 'status');
                overlay.setAttribute('aria-live', 'polite');
                overlay.setAttribute('aria-label', candidates.length + ' link hints available');
                const rootStyle = getComputedStyle(document.documentElement);
                overlay.style.cssText = 'position:fixed;inset:0;pointer-events:none;z-index:2147483647;'
                    + 'color:' + rootStyle.color + ';font:700 max(12px,.8rem) system-ui,sans-serif;';
                state.hints = candidates.map((target, index) => {
                    const label = labelFor(index);
                    const rect = target.getBoundingClientRect();
                    const hint = document.createElement('span');
                    hint.textContent = label;
                    hint.setAttribute('aria-label', 'Link hint ' + label + ' for '
                        + (target.getAttribute('aria-label') || target.textContent || target.href || 'target').trim());
                    hint.style.cssText = 'position:absolute;left:' + Math.max(0, rect.left) + 'px;top:'
                        + Math.max(0, rect.top) + 'px;padding:.15em .35em;border:2px solid ButtonText;'
                        + 'border-radius:.25em;background:ButtonFace;color:ButtonText;line-height:1.2;'
                        + 'forced-color-adjust:auto;box-shadow:0 1px 3px #0008;';
                    overlay.append(hint);
                    return { label, target, hint };
                });
                document.documentElement.append(overlay);
                state.hintMode = command;
            };
            const activateHint = entry => {
                const target = entry.target;
                const background = state.hintMode === 'open-link-background';
                clearHints();
                if (background && target.href) {
                    target.dispatchEvent(new MouseEvent('click', {
                        bubbles: true, cancelable: true, view: window,
                        button: 0, ctrlKey: true, metaKey: true
                    }));
                } else target.click();
            };
            const execute = command => {
                const reduced = matchMedia('(prefers-reduced-motion: reduce)').matches;
                const behavior = reduced ? 'instant' : 'smooth';
                if (command === 'scroll-down') scrollBy({ top: Math.max(48, innerHeight * .5), behavior });
                else if (command === 'scroll-up') scrollBy({ top: -Math.max(48, innerHeight * .5), behavior });
                else if (command === 'scroll-top') scrollTo({ top: 0, behavior });
                else if (command === 'scroll-bottom') scrollTo({ top: document.documentElement.scrollHeight, behavior });
                else if (command === 'open-link' || command === 'open-link-background') showHints(command);
            };
            addEventListener('keydown', event => {
                if (!state.config.enabled || event.defaultPrevented || event.isComposing
                        || event.ctrlKey || event.metaKey || event.altKey || editable(event.target)) return;
                const key = keyName(event);
                if (state.config.passthroughAll || state.config.passthroughKeys.includes(key)) return;
                if (state.hintMode) {
                    if (event.key === 'Escape') { event.preventDefault(); clearHints(); return; }
                    if (event.key.length !== 1) return;
                    event.preventDefault(); event.stopImmediatePropagation();
                    state.hintInput += event.key.toLowerCase();
                    const matches = state.hints.filter(item => item.label.startsWith(state.hintInput));
                    state.hints.forEach(item => item.hint.style.opacity = matches.includes(item) ? '1' : '.25');
                    if (matches.length === 1 && matches[0].label === state.hintInput) activateHint(matches[0]);
                    else if (!matches.length) clearHints();
                    return;
                }
                const bindings = state.config.bindings || {};
                if (state.prefix) {
                    const sequence = state.prefix + key;
                    clearPrefix();
                    if (bindings[sequence]) {
                        event.preventDefault(); event.stopImmediatePropagation(); execute(bindings[sequence]);
                        return;
                    }
                }
                const hasSequence = Object.keys(bindings).some(binding => binding.length > key.length
                    && binding.startsWith(key));
                if (hasSequence) {
                    event.preventDefault(); event.stopImmediatePropagation(); state.prefix = key;
                    showPrefix(key);
                    state.prefixTimer = setTimeout(clearPrefix, 700); return;
                }
                if (!bindings[key]) return;
                event.preventDefault(); event.stopImmediatePropagation(); execute(bindings[key]);
            }, true);
            globalThis.__tantoKeyboardNavigation = {
                configure(configuration) { clearHints(); clearPrefix(); state.config = configuration; }
            };
        })();`
        return script
    }

    WebEngineView {
        id: webView
        objectName: "qtWebView"
        anchors.fill: parent
        profile: root.sharedProfile ? root.sharedProfile : spaceProfile
        backgroundColor: "white"
        focus: true
        userScripts.collection: [root.editedStateScript, root.keyboardNavigationScript]

        onRenderProcessTerminated: function(terminationStatus, exitCode) {
            root.rendererFailed("Renderer stopped with exit code " + exitCode)
        }

        onLoadingChanged: {
            root.applyCosmeticRules()
            if (!loading) root.applyKeyboardNavigationConfiguration()
        }

        onNewWindowRequested: function(request) {
            if (request.destination === WebEngineNewWindowRequest.InNewBackgroundTab)
                root.backgroundTabRequested(request.requestedUrl)
            else if (root.isAuxiliaryDestination(request.destination))
                root.auxiliaryWindowRequested(request, request.requestedUrl)
            else
                root.newTabRequested(request, request.requestedUrl)
        }

        onWindowCloseRequested: root.windowCloseRequested()

        onPermissionRequested: function(request) {
            const permission = String(request.permissionType)
            const decision = root.permissionController
                ? root.permissionController.permissionDecision(request.origin, permission)
                : 0
            if (decision === 1 || decision === 2)
                request.grant()
            else if (decision === 3)
                request.deny()
            else {
                const requestId = String(++root.nextPermissionRequestId)
                root.pendingPermissions[requestId] = request
                root.sitePermissionRequested(requestId, request.origin.toString(), permission)
            }
        }
    }
}
