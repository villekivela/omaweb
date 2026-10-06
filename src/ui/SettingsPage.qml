import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs as Dialogs
import QtQuick.Effects
import Omaweb
import qs.Commons
import qs.Ui as Omarchy

// Settings is a place, not a dialog. It has outgrown a modal — filter lists, a
// rule editor and download history in one scroll — so it takes the page area
// and keeps the sidebar beside it, where the tabs and Spaces stay reachable.
Rectangle {
    id: root
    objectName: "settingsPage"

    property var colors
    property string iconFontFamily
    property var browser
    property var blocker
    property var keyboard
    property var syncLauncher: null
    // What an Agent may do through the socket (ADR 0051). The ordinary
    // window's only: a Private window has none to offer.
    property var agentControl: null
    readonly property var sync: syncLauncher ? syncLauncher.controller : null

    Connections {
        target: root.sync
        ignoreUnknownSignals: true

        function onConsentPageRequested(url) {
            root.openSyncConsent(url);
        }

        function onStateChanged() {
            if (!root.sync)
                return;
            const detail = root.sync.errorMessage;
            if (detail.length === 0) {
                root.lastReportedSyncError = "";
                return;
            }
            if (detail === root.lastReportedSyncError)
                return;
            root.lastReportedSyncError = detail;
            root.syncConnectionFailed(root.providerText.failureTitle, detail);
        }
    }
    // Every provider-worded string a person reads comes from the provider itself, with neutral
    // wording before a provider is loaded.
    readonly property var providerText: root.sync ? root.sync.providerText : ({
                                                                                  "name": qsTr(
                                                                                              "Sync"),
                                                                                  "connectAction":
                                                                                  qsTr("Connect Sync"),
                                                                                  "authorizationAction":
                                                                                  qsTr("Open the authorization page"),
                                                                                  "failureTitle":
                                                                                  qsTr("Sync failed"),
                                                                                  "codeCopiedNotice":
                                                                                  qsTr("Code copied"),
                                                                                  "codePrompt": "",
                                                                                  "authorizationNote":
                                                                                  "",
                                                                                  "repositoryTitle":
                                                                                  "",
                                                                                  "repositoryNote":
                                                                                  "",
                                                                                  "installationTitle":
                                                                                  "",
                                                                                  "installationNote":
                                                                                  "",
                                                                                  "observationNote":
                                                                                  ""
                                                                              })
    readonly property bool syncAvailable: root.browser ? !root.browser.privateBrowsing : false
    property bool open: false
    // How far below its place the sheet's content stands, in pixels, while
    // the sheet arrives or leaves. The backdrop stays where it is: a ground
    // that moved would show the page's edge above it for the length of the
    // lift.
    property real lift: 0
    property int section: 0
    // The window's download list. A model rather than an array: it says when
    // it changes, so this page never asks for it again.
    property var downloads: null
    // Every Known extension Omaweb names, as the controller reports them.
    property var knownExtensions: []
    // Whether this build's engine can host one at all. Without it the section
    // says so rather than offering switches that would load an extension into
    // a browser that hangs on its first message.
    property bool knownExtensionsAvailable: false
    // Whether this build's engine resolves a request's host for Content
    // blocking, so a tracker behind a CNAME can be refused (ADR 0050).
    property bool cnameUncloakingAvailable: false
    // Whether this build's engine applies procedural cosmetic rules (ADR 0052).
    property bool proceduralCosmeticFilteringAvailable: false
    // Whether this window is a Private one. The capability above is the
    // build's and is the same in every window, so without this the section
    // offers a reader a switch that cannot do anything here.
    property bool privateWindow: false
    // What this build's engine can answer for about a site. Site information
    // states facts about the site; where the engine falls short, the shortfall
    // is the build's and is said here.
    property bool certificateDecisionsAvailable: true
    property bool pageCertificatesAvailable: true
    property bool thirdPartyCookieControlAvailable: true
    property bool siteDataOnDisk: true
    property bool insecureContentBlocked: true
    // Why the last download ended with nothing written, when one did.
    property string extensionFailure: ""
    property var subscriptions: []
    // The lists Content blocking knows by name and has not subscribed, offered
    // beside the subscriptions so the reader never has to fetch an address.
    property var knownLists: []
    // What Content blocking refused for the page on show.
    readonly property int refusalTally: refusals.count

    property RefusalTally refusals: RefusalTally {
        blocker: root.blocker
        browser: root.browser
        pageAddress: root.browser ? root.browser.activeUrl : ""
    }
    // What Omaweb knows about its own age, so About can say whether this build
    // is current and offer the reader the switch that stops it asking.
    property var releaseWatch: null
    property var globalPrivacyControl: null
    // HTTPS-only mode's switch, browser-wide like the one above.
    property var httpsOnly: null
    // Whether the Omnibar asks a search engine for Engine suggestions,
    // browser-wide like the switches above.
    property var engineSuggestions: null
    // The engine Engine suggestions ask when no keyword chooses another.
    readonly property var defaultEngine: root.engines.find(function (engine) {
        return engine.default;
    }) || null
    // What a page's call may learn about the reader's network, and the engine
    // adapter that says whether this build can set it at all (ADR 0047).
    property var webRtcPolicy: null
    // Where names are looked up, and whether the engine took the resolver it
    // was given. Null where the build has no such setting to offer.
    property var secureDns: null
    property var engineSecureDns: null
    property var engineWebRtcPolicy: null
    readonly property bool webRtcPolicyUnreachable: !!engineWebRtcPolicy &&
                                                    !engineWebRtcPolicy.available
    // The reader's type: the interface size over the theme's and a page's
    // fonts over the engine's. The engine adapter beside it says whether this
    // build can reach the engine's fonts at all; without one, as in the lab,
    // the controls are shown and reach nothing.
    property var fontSettings: null
    property var pageFonts: null
    readonly property bool pageFontsUnreachable: !!pageFonts && !pageFonts.available
    readonly property var pageFontsMap: fontSettings ? fontSettings.pageFontsMap : ({})
    property bool useFavicons: true
    property bool tintFavicons: false
    property bool floatingControls: true
    property string sidebarSide: "left"
    property bool glanceEnabled: true
    // The Start page's Scene, by its id, or "none".
    property string startPageScene: "crt-road"
    property bool startPageGlass: true
    property string lastReportedSyncError: ""
    property var engines: []
    // Every tab still running for a Space that is not on show, and what each
    // costs. A retained tab is a renderer the reader cannot see, so the browser
    // has to be able to show them the whole list.
    property var retainedTabs: []
    property var enginePresets: []
    // A selection, not a setting: nothing is cleared until the dialog these
    // arguments belong to is confirmed (ADR 0031). It is remembered anyway,
    // because a reader who took the same categories last month means the same
    // ones now. Scope is not remembered, and the dialog owns it.
    property var clearCategories: ["cookies", "storage", "cache", "permissions", "history", "forms"]
    property string clearRange: "86400000"
    property bool clearDataOpen: false

    // The page to blur behind the settings, when there is one. Must not be an
    // ancestor of this item.
    property Item pageSource: null

    // What the keymap could not honour, if anything. Empty is the ordinary
    // case, and shows nothing at all.
    readonly property string keyboardReport: keyboard ? keyboard.errorMessage : ""

    // What the desktop's input method configuration amounts to, if it named one
    // at all. A desktop that names a plugin it did not install is the case
    // worth a notice: nothing composes and nothing says why.
    readonly property bool inputMethodMissing: !InputMethodReport.available

    // Whether anything in here is waiting on the reader. The sidebar's settings
    // button reads this, so a notice that lives on one section is still
    // findable from outside it.
    readonly property bool needsAttention: keyboardReport.length > 0 || inputMethodMissing

    // The keys the rest of the chrome finds a section by, and what the rail
    // calls each one, in the same order.
    readonly property var sections: ["tabs", "interface", "keyboard", "content blocking", "network",
        "downloads", "search", "privacy", "addresses", "payment cards", "spaces", "agents",
        "extensions", "sync", "about"]
    readonly property var sectionTitles: [qsTr("tabs"), qsTr("interface"), qsTr("keyboard"), qsTr("content blocking"),
        qsTr("network"), qsTr("downloads"), qsTr("search"), qsTr("privacy"), qsTr("addresses"), qsTr(
            "payment cards"), qsTr("spaces"), qsTr("agents"), qsTr("extensions"), qsTr("sync"), qsTr(
            "about")]

    // The rail is as wide as the longest section name it draws, measured in the
    // bold face the current section takes so the pane beside it does not shift
    // when the selection moves. A pixel count here would be right at one theme
    // font size and clip the longest name at the next.
    FontMetrics {
        id: railMetrics
        font.family: Style.font.family
        font.pixelSize: Style.font.subtitle
        font.bold: true
    }

    // What `Font.Capitalize` will actually draw. In a proportional theme family
    // a capital is wider than the letter it replaces, so the lowercase name the
    // model holds is not what the rail has to fit.
    function railLabel(name) {
        return name.replace(/(^|\s)\S/g, function (first) {
            return first.toUpperCase();
        });
    }

    function selectSectionByLetter(letter) {
        for (let offset = 1; offset <= root.sections.length; ++offset) {
            const index = (root.section + offset) % root.sections.length;
            if (root.sectionTitles[index].charAt(0).toLowerCase() !== letter)
                continue;
            root.section = index;
            sectionRepeater.itemAt(index).forceActiveFocus();
            return true;
        }
        return false;
    }

    readonly property int railWidth: {
        // Read for the dependency alone: advanceWidth() measures in C++ off a
        // font this binding never otherwise touches.
        void (railMetrics.font.pixelSize);
        void (railMetrics.font.family);
        let widest = 0;
        for (let index = 0; index < root.sections.length; ++index)
            widest = Math.max(widest, railMetrics.advanceWidth(root.railLabel(
                                                                   root.sectionTitles[index])));

        return Math.ceil(widest);
    }

    // -------------------------------------------------------------- geometry
    //
    // Nothing about this page's frame is written down. `Style.space()` keeps
    // the proportions the page already had while letting the theme make the
    // shell denser or roomier, and `Style.spacing.*` is the shared rhythm
    // everything else in the kit is set on. A pixel count here would be right
    // at one theme font size and crowd or clip at the next (#85).
    readonly property int sideMargin: Style.space(48)
    // Every full-window sheet leaves the same gap above its heading.
    readonly property int topInset: sheetInsets.top
    readonly property int headerGap: Style.space(28)
    readonly property int bottomInset: Style.space(24)
    readonly property int railGap: Style.space(40)
    readonly property int closeSize: Style.space(30)

    // A line of explanation is a measure rather than a pixel count: the
    // readable one is written in characters, and the box that holds it is that
    // many characters of whatever face the theme sets the caption in. The 260
    // pixels this replaces was the same measure at one font size only, and
    // wrapped the same words into twice the lines at the next.
    FontMetrics {
        id: noteMetrics
        font.family: Style.font.family
        font.pixelSize: Style.font.caption
    }

    // Forty-four, which is 264 pixels of the default theme's caption face —
    // the measure this replaces, to within a rounding.
    readonly property int noteCharacters: 44
    readonly property int noteMeasure: Math.ceil(noteMetrics.averageCharacterWidth
                                                 * root.noteCharacters)

    // What the four fields of the subscription form ask to be called, in one
    // place, because the room a pair of them needs is measured from the
    // longest of them.
    readonly property var subscriptionPlaceholders: ({
                                                         title: qsTr("list name"),
                                                         license: qsTr("license"),
                                                         source: qsTr("source page"),
                                                         update: qsTr("update address")
                                                     })

    FontMetrics {
        id: fieldMetrics
        font.family: Style.font.family
        font.pixelSize: Style.font.body
    }

    // The width below which a field can no longer hold the words it is asking
    // for: its longest placeholder in the face the kit's field draws in, plus
    // the padding that field keeps on either side.
    readonly property int fieldFloor: {
        // The font is read for the dependency alone: advanceWidth() measures in
        // C++, so a theme that changed the type would otherwise leave the floor
        // at the size it was last measured at.
        void (fieldMetrics.font.pixelSize);
        void (fieldMetrics.font.family);
        let widest = 0;
        for (const field in root.subscriptionPlaceholders)
            widest = Math.max(widest, fieldMetrics.advanceWidth(
                                  root.subscriptionPlaceholders[field]));
        return Math.ceil(widest) + Style.spacing.controlPaddingX * 2;
    }

    // about:blank and other opaque addresses have no host to name, and saying
    // "blocked on about" would be worse than saying nothing.
    readonly property string activeHost: {
        if (!browser)
            return "";
        const value = String(browser.activeUrl);
        if (!/^[a-z]+:\/\//.test(value))
            return "";
        return value.replace(/^[a-z]+:\/\//, "").split("/")[0].split(":")[0];
    }

    // A download's state as the reader reads it. A state this build does not name is shown as
    // the core wrote it.
    function downloadStateLabel(state) {
        switch (state) {
        case "requested":
            return qsTr("requested");
        case "in-progress":
            return qsTr("in progress");
        case "completed":
            return qsTr("completed");
        case "interrupted":
            return qsTr("interrupted");
        case "cancelled":
            return qsTr("cancelled");
        default:
            return state;
        }
    }

    function projectNote(project) {
        if (!project)
            return "";
        const lines = [qsTr("Project folder: %1").arg(project.directory)];
        if (!project.present)
            lines.push(qsTr("This folder is not on this computer."));
        lines.push(qsTr("Address: %1").arg(project.address));
        lines.push(project.agentCommand.length > 0 ? qsTr("Agent command: %1").arg(
                                                         project.agentCommand) : qsTr(
                                                         "Agent command from the agents section"));
        return lines.join("\n");
    }

    // What a Space's colour is called to a screen reader.
    function colourName(colour) {
        switch (colour) {
        case "green":
            return qsTr("Green");
        case "yellow":
            return qsTr("Yellow");
        case "blue":
            return qsTr("Blue");
        case "bright_green":
            return qsTr("Bright green");
        case "bright_yellow":
            return qsTr("Bright yellow");
        case "bright_blue":
            return qsTr("Bright blue");
        default:
            return colour;
        }
    }

    // Bytes as the reader reads them. A retained tab whose renderer the
    // platform cannot account for says so rather than claiming nothing.
    function resourceLabel(bytes) {
        if (!(bytes > 0))
            return qsTr("size unavailable");
        const megabytes = bytes / (1024 * 1024);
        return qsTr("%1 MB").arg(megabytes >= 100 ? Math.round(megabytes) : Math.round(megabytes
                                                                                       * 10) / 10);
    }

    signal closed
    signal newSpaceRequested
    signal spaceActionRequested(string action, string spaceId, string spaceName)
    signal downloadDirectoryRequested
    signal downloadCancelled(int row)
    signal downloadRetried(int row)
    signal downloadRevealed(string path)
    signal downloadForgotten(int row)
    signal retainedTabReleased(string tabId)
    signal syncCodeCopied(string notice)
    signal syncConsentRequested(url url)
    signal syncConnectionFailed(string title, string detail)

    function copySyncCode() {
        if (root.sync && root.sync.userCode.length > 0 && SystemClipboard.copyText(
                    root.sync.userCode))
            root.syncCodeCopied(root.providerText.codeCopiedNotice);
    }

    function openSyncConsent(url) {
        if (root.sync && !root.sync.awaitingRepositoryCreation && !root.sync.awaitingInstallation
                && root.sync.userCode.length > 0)
            root.copySyncCode();
        root.closed();
        root.syncConsentRequested(url);
    }
    // A reader turning a Known extension on or off. One answer, not one per
    // Space: every Space loads the same package, and what they keep apart is
    // the storage the extension writes.
    signal knownExtensionToggled(string key, bool enabled)
    signal useFaviconsToggled(bool enabled)
    signal tintFaviconsToggled(bool enabled)
    signal floatingControlsToggled(bool enabled)
    signal sidebarSideChosen(string side)
    signal glanceToggled(bool enabled)
    signal startPageSceneChosen(string scene)
    signal startPageGlassToggled(bool enabled)

    Dialogs.FileDialog {
        id: recoveryKeySaveDialog
        title: qsTr("Save Sync recovery key")
        fileMode: Dialogs.FileDialog.SaveFile
        nameFilters: [qsTr("Text files (*.txt)")]
        onAccepted: {
            if (root.sync)
                root.sync.saveRecoveryKey(selectedFile);
        }
    }

    visible: open
    // Settings is a place over the page rather than instead of it, so the page
    // stays visible through it: blurred, under the same translucency the
    // sidebar beside it has.
    color: "transparent"
    // The dialog over this page owns the keyboard while it is open, so Escape
    // there closes the dialog rather than the place behind it.
    focus: open && !clearDataOpen

    Keys.onPressed: function (event) {
        if (root.clearDataOpen)
            return;
        if (event.key === Qt.Key_Escape) {
            root.closed();
            event.accepted = true;
            return;
        }
        const letter = event.text.toLowerCase();
        if (/^[a-z]$/.test(letter) && root.selectSectionByLetter(letter))
            event.accepted = true;
    }

    // The reader's saved addresses, and the one open in the fields under the
    // list: an id while one is edited, empty while a new one is added.
    property var savedAddresses: []
    property bool addressEditing: false
    property string editingAddressId: ""

    // The reader's saved cards, by everything but their number, and the one
    // open in the fields under the list. They are read from the keyring only
    // when their section is on show, so opening Settings does not ask the
    // desktop to unlock it.
    readonly property int cardsSection: root.sections.indexOf("payment cards")
    property var savedCards: []
    property bool cardEditing: false
    property string editingCardId: ""
    property string editingCardLast4: ""
    property bool cardRefused: false
    // A card saved into a locked keyring, waiting in its fields until the
    // reader answers the desktop's prompt to unlock it, and whether the keyring
    // stayed locked and the card was not kept.
    property string unlockingCardId: ""
    property bool cardNotKept: false

    function refreshCards() {
        root.savedCards = root.browser && root.section === root.cardsSection
                ? root.browser.paymentCards() : [];
    }

    onSectionChanged: root.refreshCards()

    Connections {
        target: root.browser || null
        ignoreUnknownSignals: true
        function onPaymentCardsChanged() {
            root.refreshCards();
            root.settleUnlockingCard();
        }
    }

    // What the section says with no card to list: where the keyring stands.
    // Until the first read has finished there is no knowing whether it holds
    // any. A keyring the reader left locked still takes a card, and saving one
    // asks the desktop to unlock it again.
    function keyringSays(state) {
        switch (state) {
        case "unread":
        case "reading":
            return {
                "title": qsTr("Reading the keyring"),
                "note": qsTr(
                            "Omaweb is asking the desktop's keyring for the cards. The desktop may ask you to unlock it.")
            };
        case "ready":
            return {
                "title": qsTr("No saved cards"),
                "note": qsTr(
                            "Cards are kept in the desktop's keyring and offered in forms in every Space, never in a Private window. The security code is never kept.")
            };
        case "unreachable":
            return {
                "title": qsTr("Omaweb could not reach the keyring"),
                "note": qsTr(
                            "The session bus offers a secret store, and nothing answered for it. An Omaweb on a session bus of its own cannot reach the desktop's keyring.")
            };
        case "locked":
            return {
                "title": qsTr("The keyring is locked"),
                "note": qsTr("The desktop did not unlock its keyring. Adding a card asks it again.")
            };
        case "failed":
            return {
                "title": qsTr("The keyring answered with an error"),
                "note": qsTr(
                            "Omaweb asked the desktop's keyring for the cards, and it refused. The log has its message.")
            };
        }
        return {
            "title": qsTr("No secret store"),
            "note": qsTr("The desktop offers no secret store, so Omaweb keeps no payment cards.")
        };
    }

    function cardTitle(card) {
        return qsTr("%1 •••• %2", "a saved card: its nickname or brand, its last four digits").arg(
                    card.nickname || card.brand || qsTr("Card", "a payment card with no name")).arg(
                    card.last4);
    }

    function cardExpiry(card) {
        return card.expiryMonth > 0 ? String(card.expiryMonth).padStart(2, "0") + "/" + String(
                                          card.expiryYear % 100).padStart(2, "0") : "";
    }

    function cardDetail(card) {
        const expiry = root.cardExpiry(card);
        return card.name && expiry ? qsTr("%1 · %2",
                                          "a saved card: the name on it · its expiry").arg(
                                         card.name).arg(expiry) : card.name || expiry;
    }

    // Opens the fields under the list, empty for a new card or holding the one
    // being edited, all but its number: that is shown only as its last four
    // digits, and stays as it is unless another is typed.
    function editCard(card) {
        root.editingCardId = card ? card.id : "";
        root.editingCardLast4 = card ? card.last4 : "";
        cardNumber.text = "";
        cardHolder.text = card ? card.name : "";
        cardExpiry.text = card ? root.cardExpiry(card) : "";
        cardNickname.text = card ? card.nickname : "";
        root.cardRefused = false;
        root.cardNotKept = false;
        root.unlockingCardId = "";
        root.cardEditing = true;
    }

    function settleUnlockingCard() {
        const state = root.browser ? root.browser.paymentCardsState : "unavailable";
        if (root.unlockingCardId.length === 0 || state === "unread" || state === "reading")
            return;
        // Asked of the browser, because the list is emptied while another
        // section is on show.
        const kept = root.browser.paymentCards().some(card => card.id === root.unlockingCardId);
        root.unlockingCardId = "";
        if (!kept) {
            root.cardNotKept = true;
            return;
        }
        cardNumber.text = "";
        root.cardEditing = false;
    }

    function saveCard() {
        const unlocking = root.browser.paymentCardsState === "locked";
        root.cardNotKept = false;
        const saved = root.browser.savePaymentCard({
                                                       "id": root.editingCardId,
                                                       "number": cardNumber.text,
                                                       "name": cardHolder.text,
                                                       "expiry": cardExpiry.text,
                                                       "nickname": cardNickname.text
                                                   });
        if (saved.length === 0) {
            root.cardRefused = true;
            return;
        }
        root.cardRefused = false;
        if (unlocking) {
            root.unlockingCardId = saved;
            return;
        }
        cardNumber.text = "";
        root.cardEditing = false;
        root.refreshCards();
    }

    function removeCard(id) {
        if (!root.browser.removePaymentCard(id))
            return;
        if (root.editingCardId === id)
            root.cardEditing = false;
        root.refreshCards();
    }

    function refresh() {
        if (!root.browser)
            return;
        root.savedAddresses = root.browser.addresses();
        root.refreshCards();
        root.engines = root.browser.searchEngines();
        root.enginePresets = root.browser.searchEnginePresets();
        root.subscriptions = root.blocker ? root.blocker.subscriptions : [];
        root.knownLists = root.blocker ? root.blocker.knownLists : [];
        userRules.text = root.blocker ? root.blocker.userRules : "";
        root.loadBrowsingDataSelection();
    }

    // The categories and the range are remembered through the same preference
    // store `use-favicons` uses. Scope is not: it is the one argument with no
    // prior art to borrow, and a choice inherited from a config file written
    // weeks ago is a default rather than a choice.
    function loadBrowsingDataSelection() {
        if (!root.browser)
            return;
        const saved = root.browser.preference("clear-data-categories",
                                              "cookies,storage,cache,permissions,history,forms");
        root.clearCategories = saved.length > 0 ? saved.split(",").filter(value => value
                                                                                   !== "cards") :
                                                  [];
        root.clearRange = root.browser.preference("clear-data-range", "86400000");
    }

    // Payment cards are not browsing history, so ticking them is never
    // remembered: they are off each time the dialog opens, as the reach of
    // every Space is.
    onClearDataOpenChanged: {
        if (root.clearDataOpen)
            root.clearCategories = root.clearCategories.filter(value => value !== "cards");
    }

    function toggleClearCategory(value) {
        const next = root.clearCategories.slice();
        const at = next.indexOf(value);
        if (at >= 0)
            next.splice(at, 1);
        else
            next.push(value);
        root.clearCategories = next;
        if (root.browser)
            root.browser.setPreference("clear-data-categories", next.filter(kept => kept
                                                                                    !== "cards").join(
                                           ","));
    }

    function chooseClearRange(value) {
        root.clearRange = value;
        if (root.browser)
            root.browser.setPreference("clear-data-range", value);
    }

    // Where Engine suggestions send the typing: the default engine, or the
    // plain fact that it has nowhere to ask.
    function engineSuggestionsNote(engine) {
        if (engine === null)
            return "";
        if (!engine.suggestUrl)
            return qsTr("%1 doesn't offer suggestions.").arg(engine.name);
        return qsTr(
                    "Sends what you type in the Omnibar to %1 as you type, without your cookies. Never in a Private window.").arg(
                    engine.name);
    }

    function makeDefaultSearchEngine(id) {
        if (root.browser.setDefaultSearchEngine(id))
            root.refresh();
    }

    function deleteSearchEngine(id) {
        if (root.browser.deleteSearchEngine(id))
            root.refresh();
    }

    function addressDetail(address) {
        return [address.street, address.city].filter(part => part.length > 0).join(", ");
    }

    // Opens the fields under the list, empty for a new address or holding the
    // one being edited.
    function editAddress(address) {
        root.editingAddressId = address ? address.id : "";
        addressName.text = address ? address.name : "";
        addressStreet.text = address ? address.street : "";
        addressPostalCode.text = address ? address.postalCode : "";
        addressCity.text = address ? address.city : "";
        addressCountry.text = address ? address.country : "";
        addressPhone.text = address ? address.phone : "";
        addressEmail.text = address ? address.email : "";
        root.addressEditing = true;
    }

    function saveAddress() {
        const saved = root.browser.saveAddress({
                                                   "id": root.editingAddressId,
                                                   "name": addressName.text,
                                                   "street": addressStreet.text,
                                                   "postalCode": addressPostalCode.text,
                                                   "city": addressCity.text,
                                                   "country": addressCountry.text,
                                                   "phone": addressPhone.text,
                                                   "email": addressEmail.text
                                               });
        if (saved.length === 0)
            return;
        root.addressEditing = false;
        root.refresh();
    }

    function removeAddress(id) {
        if (!root.browser.removeAddress(id))
            return;
        if (root.editingAddressId === id)
            root.addressEditing = false;
        root.refresh();
    }

    function searchEngineInstalled(id) {
        return root.engines.some(function (engine) {
            return engine.id === id;
        });
    }

    onOpenChanged: if (open)
                       refresh()
    // The selection is state the page holds whether or not it has been opened,
    // so it is read once rather than on the first visit.
    Component.onCompleted: loadBrowsingDataSelection()

    SheetInsets {
        id: sheetInsets
    }

    SheetFloor {}

    PageBackdrop {
        objectName: "settingsBackdrop"
        anchors.fill: parent
        source: root.pageSource
        tint: root.colors.sheet
    }

    Item {
        id: header
        transform: Translate {
            y: root.lift
        }
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: root.sideMargin
        anchors.rightMargin: root.sideMargin
        anchors.topMargin: root.topInset
        enabled: !root.clearDataOpen
        height: settingsEyebrow.height + Style.spacing.md + settingsHeading.height

        // Settings is a place, so it is titled like one: what this is, and the
        // key that leaves it, above the name.
        Text {
            id: settingsEyebrow
            objectName: "settingsEyebrow"
            anchors.left: parent.left
            anchors.top: parent.top
            text: qsTr("browsing · esc closes")
            color: root.colors.mutedText
            font.family: Style.font.family
            font.pixelSize: Style.font.caption
            font.bold: true
            font.capitalization: Font.AllUppercase
            font.letterSpacing: 1.6
            Accessible.ignored: true
        }

        Text {
            id: settingsHeading
            objectName: "settingsHeading"
            anchors.left: parent.left
            anchors.top: settingsEyebrow.bottom
            anchors.topMargin: Style.spacing.md
            text: qsTr("Settings")
            color: root.colors.text
            font.family: Style.font.family
            font.pixelSize: Style.font.display
            Accessible.role: Accessible.Heading
            Accessible.name: qsTr("Settings")
        }

        ChromeButton {
            objectName: "closeSettingsButton"
            anchors.right: parent.right
            anchors.verticalCenter: settingsHeading.verticalCenter
            width: root.closeSize
            height: root.closeSize
            icon: "close"
            accessibleName: qsTr("Close settings")
            fontFamily: root.iconFontFamily
            foreground: root.colors.mutedText
            accent: root.colors.accent
            onClicked: root.closed()
        }
    }

    Row {
        id: body
        transform: Translate {
            y: root.lift
        }
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        anchors.leftMargin: root.sideMargin
        // The trailing side margin is the pane's padding instead, so the
        // scrollbar has the margin to run in rather than the settings' edge.
        anchors.topMargin: root.headerGap
        anchors.bottomMargin: root.bottomInset
        spacing: root.railGap
        // A dialog over this page is modal, so the page under it takes neither
        // a click nor a Tab while it stands. Qt has no tab fence a QML item can
        // raise, so the way to keep the ring inside the dialog is to take the
        // rest of the page out of it.
        enabled: !root.clearDataOpen

        Column {
            id: rail
            width: root.railWidth
            spacing: Style.spacing.xxs

            Repeater {
                id: sectionRepeater
                model: root.sections

                Text {
                    required property int index
                    required property string modelData

                    objectName: "settingsSection" + index
                    text: root.sectionTitles[index]
                    color: index === root.section ? root.colors.accent : root.colors.mutedText
                    // A name in the rail leans on nothing: it belongs to the
                    // rail rather than to what follows it, so it takes the same
                    // room above and below.
                    topPadding: Style.spacing.md
                    bottomPadding: Style.spacing.md
                    font.family: Style.font.family
                    // The rail names the sections, so it is set like the labels
                    // that name them in the pane rather than like a row.
                    font.pixelSize: Style.font.subtitle
                    font.bold: index === root.section
                    font.capitalization: Font.Capitalize
                    activeFocusOnTab: true
                    Accessible.role: Accessible.PageTab
                    Accessible.name: root.sectionTitles[index]
                    Accessible.onPressAction: root.section = index

                    Keys.onPressed: function (event) {
                        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key
                                === Qt.Key_Space) {
                            root.section = index;
                            event.accepted = true;
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            parent.forceActiveFocus();
                            root.section = index;
                        }
                    }
                }
            }
        }

        ScrollView {
            id: scroll
            width: body.width - rail.width - body.spacing
            height: parent.height
            rightPadding: root.sideMargin
            contentWidth: availableWidth
            clip: true

            ScrollBar.vertical: ChromeScrollBar {
                view: scroll
                colors: root.colors
            }

            Column {
                id: pane
                objectName: "settingsPane"
                width: scroll.availableWidth
                // The gap between the blocks a section is made of — its cards,
                // its labelled lists, its forms. It is written here rather than
                // left to whatever the children happen to carry, so adding a
                // row to one block cannot change the rhythm of the block under
                // it. Inside a ruled list the rows abut instead: each draws the
                // rule that divides it from the row above, and a gap there
                // would break one list into a stack of cards.
                spacing: Style.spacing.lg

                // ---- tabs ---------------------------------------------------

                Column {
                    width: pane.width
                    visible: root.section === 0
                    spacing: pane.spacing

                    SettingToggle {
                        objectName: "useFavicons"
                        width: pane.width
                        colors: root.colors
                        title: qsTr("Use site favicons")
                        note: qsTr("When off, tabs show a two-letter tile in the favicon's colour.")
                        accessibleName: qsTr("Use site favicons")
                        checked: root.useFavicons
                        onClicked: root.useFaviconsToggled(!checked)
                    }

                    SettingToggle {
                        objectName: "tintFavicons"
                        width: pane.width
                        colors: root.colors
                        title: qsTr("Tint favicons")
                        note: qsTr("Recolor site artwork to match the sidebar palette.")
                        accessibleName: qsTr("Tint favicons")
                        enabled: root.useFavicons
                        checked: root.tintFavicons
                        onClicked: root.tintFaviconsToggled(!checked)
                    }

                    // The label and the list it heads are one block: the label
                    // already leans toward what follows it, and the rows under
                    // it abut so that their own rules divide them.
                    Column {
                        width: pane.width
                        spacing: 0

                        SectionLabel {
                            colors: root.colors
                            text: qsTr("kept active")
                        }

                        Repeater {
                            model: root.section === 0 ? root.retainedTabs : []

                            SettingRow {
                                required property var modelData

                                objectName: "retainedTab-" + modelData.tabId
                                width: pane.width
                                colors: root.colors
                                title: modelData.title.length > 0 ? modelData.title : String(
                                                                        modelData.url)
                                note: modelData.spaceName + " · " + (modelData.inspected ? qsTr(
                                                                                               "Developer tools") :
                                                                                           qsTr("Keep active"))
                                      + " · " + (modelData.running ? root.resourceLabel(
                                                                         modelData.residentBytes) :
                                                                     qsTr("not running"))

                                ActionButton {
                                    visible: !modelData.inspected
                                    colors: root.colors
                                    label: qsTr("stop", "verb: stop keeping a tab active")
                                    accessibleName: qsTr("Stop keeping %1 active").arg(
                                                        modelData.title)
                                    onClicked: root.retainedTabReleased(modelData.tabId)
                                }
                            }
                        }

                        SettingRow {
                            width: pane.width
                            visible: root.retainedTabs.length === 0
                            colors: root.colors
                            title: qsTr("Nothing is kept active")
                            note: qsTr(
                                      "A Pinned tab set to Keep active, or a tab with Developer tools attached, keeps running while its Space is inactive and is listed here with what it costs.")
                        }
                    }
                }

                // ---- interface ---------------------------------------------

                Column {
                    width: pane.width
                    visible: root.section === 1
                    spacing: pane.spacing

                    SettingRow {
                        width: pane.width
                        colors: root.colors
                        title: qsTr("Sidebar side")
                        note: qsTr(
                                  "The edge of the window the sidebar stands against. While it is hidden, the floating controls stay at that edge.")

                        SettingChoice {
                            objectName: "sidebarSide"
                            colors: root.colors
                            options: [
                                {
                                    value: "left",
                                    label: qsTr("Left", "sidebar side: the window's left edge")
                                },
                                {
                                    value: "right",
                                    label: qsTr("Right", "sidebar side: the window's right edge")
                                }
                            ]
                            value: root.sidebarSide
                            accessibleName: qsTr("Sidebar side")
                            onChanged: function (side) {
                                root.sidebarSideChosen(side);
                            }
                        }
                    }

                    SettingToggle {
                        objectName: "floatingControls"
                        width: pane.width
                        colors: root.colors
                        title: qsTr("Floating controls")
                        note: qsTr(
                                  "With the sidebar hidden, keep the navigation controls over the page. Pause at the sidebar's edge of the window to peek at it; it hides when the pointer leaves.")
                        accessibleName: qsTr("Floating controls")
                        checked: root.floatingControls
                        onClicked: root.floatingControlsToggled(!checked)
                    }

                    SettingToggle {
                        objectName: "glanceEnabled"
                        width: pane.width
                        colors: root.colors
                        title: qsTr("Glance at a page's new tabs")
                        note: qsTr(
                                  "A link that asks for a new tab opens over the page instead, for a look. Escape closes it; one command keeps it as a tab. When off, the link opens a tab.")
                        accessibleName: qsTr("Glance at a page's new tabs")
                        checked: root.glanceEnabled
                        onClicked: root.glanceToggled(!checked)
                    }

                    // The thumbnails stand in a row of their own under the
                    // title and note, rather than in the control's place at
                    // the right.
                    Column {
                        objectName: "startPageSceneRow"
                        width: pane.width

                        SettingRow {
                            width: pane.width
                            colors: root.colors
                            title: qsTr("Scene on the Start page")
                            note: qsTr(
                                      "What the Start page's Omnibar rests on. A Scene moves while the window is in use and quickens until the page you asked for paints. With None, the Omnibar rests on the sidebar's colour.")
                        }

                        ScenePicker {
                            objectName: "startPageScenePicker"
                            availableWidth: pane.width
                            colors: root.colors
                            value: root.startPageScene
                            accessibleName: qsTr("Scene on the Start page")
                            onChanged: function (scene) {
                                root.startPageSceneChosen(scene);
                            }
                        }

                        Item {
                            width: 1
                            height: Style.spacing.huge
                        }
                    }

                    SettingToggle {
                        objectName: "startPageGlass"
                        width: pane.width
                        visible: root.startPageScene !== "none"
                        colors: root.colors
                        title: qsTr("CRT glass over the Scene")
                        note: qsTr(
                                  "Show the Start page's Scene through an old screen's glass: scanlines, a soft bloom and a faint flicker. When off, the Scene is its plain pixels.")
                        accessibleName: qsTr("CRT glass over the Scene")
                        checked: root.startPageGlass
                        onClicked: root.startPageGlassToggled(!checked)
                    }

                    // An ordinary tab not on show for this long goes to its
                    // Space's put-away list. A Private window puts nothing
                    // away, so it has nothing to choose.
                    SettingRow {
                        objectName: "putAwayAfterRow"
                        width: pane.width
                        visible: !root.privateWindow && !!root.browser
                        colors: root.colors
                        title: qsTr("Put away unused tabs")
                        note: qsTr(
                                  "An ordinary tab you have not shown for this long closes into its Space's put-away list, in History and the Omnibar, for 30 days. Pinned tabs, a split, a tab making sound and an Agent tab stay.")

                        SettingDropdown {
                            objectName: "putAwayAfter"
                            colors: root.colors
                            options: [
                                {
                                    value: "0",
                                    label: qsTr("Off", "put unused tabs away: never")
                                },
                                {
                                    value: "3600",
                                    label: qsTr("1 hour")
                                },
                                {
                                    value: "43200",
                                    label: qsTr("12 hours")
                                },
                                {
                                    value: "86400",
                                    label: qsTr("1 day")
                                },
                                {
                                    value: "604800",
                                    label: qsTr("1 week")
                                }
                            ]
                            value: root.browser ? String(root.browser.putAwayAfterSeconds) : ""
                            accessibleName: qsTr("Put away unused tabs")
                            onChanged: function (seconds) {
                                root.browser.setPutAwayAfterSeconds(parseInt(seconds));
                            }
                        }
                    }

                    SectionLabel {
                        visible: !!root.fontSettings
                        colors: root.colors
                        text: qsTr("type", "section label: typography settings")
                    }

                    // The theme sets the base size Omaweb's own type scale
                    // grows from, and this is the reader's say over it (#170).
                    // It is the interface's alone: a page and its zoom are
                    // untouched, which is what the page fonts below are for.
                    SettingRow {
                        objectName: "interfaceFontSizeRow"
                        visible: !!root.fontSettings
                        width: pane.width
                        colors: root.colors
                        title: qsTr("Interface font size")
                        note: qsTr(
                                  "The size Omaweb's own type is drawn at; the theme's is %1px. Pages and their zoom are not changed by it.").arg(
                                  root.fontSettings ? root.fontSettings.themeFontSize : 0)

                        SettingStepper {
                            objectName: "interfaceFontSize"
                            colors: root.colors
                            value: root.fontSettings ? root.fontSettings.interfaceFontSize : 0
                            minimum: root.fontSettings ? root.fontSettings.minimumInterfaceFontSize :
                                                         0
                            maximum: root.fontSettings ? root.fontSettings.maximumInterfaceFontSize :
                                                         0
                            overridden: !!root.fontSettings
                                        && root.fontSettings.interfaceFontSizeOverridden
                            accessibleName: qsTr("Interface font size")
                            defaultName: qsTr("the theme's")
                            onIncreased: root.fontSettings.increaseInterfaceFontSize()
                            onDecreased: root.fontSettings.decreaseInterfaceFontSize()
                            onReset: root.fontSettings.resetInterfaceFontSize()
                        }
                    }

                    SectionLabel {
                        visible: !!root.fontSettings
                        colors: root.colors
                        text: qsTr("page fonts")
                    }

                    // A build running on a Qt it was not compiled against
                    // does not reach into the engine's settings (ADR 0047),
                    // so the group says so rather than offering controls
                    // that would change nothing.
                    NoticeBox {
                        objectName: "pageFontsNotice"
                        width: pane.width
                        visible: root.pageFontsUnreachable
                        colors: root.colors
                        iconFontFamily: root.iconFontFamily
                        glyph: "text_fields"
                        title: qsTr("This build cannot reach the engine's fonts")
                        detail: qsTr(
                                    "Omaweb was built against another Qt than the one it is running on, so pages are drawn in the engine's own fonts. A rebuild against this Qt brings the controls back.")
                    }

                    // A page that names no family gets these, and a page that
                    // names a smaller size than the floor gets the floor. Each
                    // is the engine's own until the reader picks one, and
                    // the families offered are the ones the host has (#293).
                    Column {
                        id: pageFontsGroup
                        objectName: "pageFontsGroup"
                        width: pane.width
                        visible: !!root.fontSettings && !root.pageFontsUnreachable
                        spacing: pane.spacing

                        // The engine's own answer leads the list, and the
                        // family the interface is drawn in is offered next for
                        // the fixed-width slot, since the terminal and the
                        // browser are often wanted to agree on code.
                        function familyOptions(engineFamily, offerInterfaceFamily) {
                            const engineLabel = engineFamily.length > 0 ? qsTr(
                                                                              "Engine default (%1)").arg(
                                                                              engineFamily) : qsTr(
                                                                              "Engine default");
                            const options = [
                                      {
                                          value: "",
                                          label: engineLabel
                                      }
                                  ];
                            const installed = root.fontSettings
                                  ? root.fontSettings.installedFamilies() : [];
                            if (offerInterfaceFamily && installed.indexOf(Style.font.family) !== -1)
                                options.push({
                                                 value: Style.font.family,
                                                 label: qsTr("Interface's (%1)").arg(
                                                            Style.font.family)
                                             });
                            for (const family of installed) {
                                if (!(offerInterfaceFamily && family === Style.font.family))
                                    options.push({
                                                     value: family,
                                                     label: family
                                                 });
                            }
                            return options;
                        }

                        function chosenFamily(entry) {
                            return entry && entry.overridden ? entry.value : "";
                        }

                        // Wide enough for the longest name it can be asked to
                        // show, in the face the kit's dropdown draws it in,
                        // plus the padding and chevron that dropdown keeps
                        // beside it; a family name is not a thing to elide.
                        // Capped at half the pane, which is where the title
                        // beside it would otherwise be squeezed out.
                        function familyControlWidth(options) {
                            void (fieldMetrics.font.pixelSize);
                            void (fieldMetrics.font.family);
                            let widest = 0;
                            for (const option of options)
                                widest = Math.max(widest, fieldMetrics.advanceWidth(option.label));
                            return Math.min(Math.ceil(widest) + Style.spacing.controlPaddingX * 2 + Style.spacing.md
                                            + Style.spacing.controlGap + Style.font.body * 2,
                                            Math.floor(pane.width / 2));
                        }

                        SettingRow {
                            width: pane.width
                            colors: root.colors
                            title: qsTr("Standard font")
                            note: qsTr("A page that names no font is drawn in this one.")

                            SettingDropdown {
                                objectName: "pageStandardFamily"
                                width: pageFontsGroup.familyControlWidth(options)
                                colors: root.colors
                                options: pageFontsGroup.familyOptions(
                                             root.pageFontsMap.standardFamily
                                             ? root.pageFontsMap.standardFamily.engine : "", false)
                                value: pageFontsGroup.chosenFamily(root.pageFontsMap.standardFamily)
                                accessibleName: qsTr("Standard font for pages")
                                onChanged: function (family) {
                                    root.fontSettings.setPageFamily(FontSettings.Standard, family);
                                }
                            }
                        }

                        SettingRow {
                            width: pane.width
                            colors: root.colors
                            title: qsTr("Fixed-width font")
                            note: qsTr(
                                      "Code on a page, and any text a page asks to have drawn monospaced.")

                            SettingDropdown {
                                objectName: "pageFixedFamily"
                                width: pageFontsGroup.familyControlWidth(options)
                                colors: root.colors
                                options: pageFontsGroup.familyOptions(root.pageFontsMap.fixedFamily
                                                                      ? root.pageFontsMap.fixedFamily.engine :
                                                                        "", true)
                                value: pageFontsGroup.chosenFamily(root.pageFontsMap.fixedFamily)
                                accessibleName: qsTr("Fixed-width font for pages")
                                onChanged: function (family) {
                                    root.fontSettings.setPageFamily(FontSettings.Fixed, family);
                                }
                            }
                        }

                        SettingRow {
                            width: pane.width
                            colors: root.colors
                            title: qsTr("Font size")
                            note: qsTr(
                                      "The size a page that names none is read at. Code follows it a step smaller, as the engine keeps it.")

                            SettingStepper {
                                objectName: "pageFontSize"
                                colors: root.colors
                                value: root.pageFontsMap.fontSize
                                       ? root.pageFontsMap.fontSize.value : 0
                                minimum: root.fontSettings ? root.fontSettings.minimumPageFontSize :
                                                             0
                                maximum: root.fontSettings ? root.fontSettings.maximumPageFontSize :
                                                             0
                                overridden: !!root.pageFontsMap.fontSize
                                            && root.pageFontsMap.fontSize.overridden
                                accessibleName: qsTr("Page font size")
                                defaultName: qsTr("the engine's")
                                onIncreased: root.fontSettings.setPageSize(FontSettings.Default,
                                                                           value + 1)
                                onDecreased: root.fontSettings.setPageSize(FontSettings.Default,
                                                                           value - 1)
                                onReset: root.fontSettings.setPageSize(FontSettings.Default, 0)
                            }
                        }

                        SettingRow {
                            width: pane.width
                            colors: root.colors
                            title: qsTr("Minimum font size")
                            note: qsTr(
                                      "No text on a page is drawn smaller than this, whatever size the page asks for. A tab's zoom multiplies it. A page already open takes it when it next lays out; reload to see it now.")

                            SettingStepper {
                                objectName: "pageMinimumFontSize"
                                colors: root.colors
                                value: root.pageFontsMap.minimumFontSize
                                       ? root.pageFontsMap.minimumFontSize.value : 0
                                minimum: 0
                                maximum: root.fontSettings
                                         ? root.fontSettings.maximumPageMinimumFontSize : 0
                                zeroLabel: qsTr("none", "no minimum font size")
                                overridden: !!root.pageFontsMap.minimumFontSize
                                            && root.pageFontsMap.minimumFontSize.overridden
                                accessibleName: qsTr("Minimum page font size")
                                defaultName: qsTr("the engine's")
                                onIncreased: root.fontSettings.setPageSize(FontSettings.Minimum,
                                                                           value + 1)
                                onDecreased: root.fontSettings.setPageSize(FontSettings.Minimum,
                                                                           value - 1)
                                onReset: root.fontSettings.setPageSize(FontSettings.Minimum, 0)
                            }
                        }
                    }

                    SectionLabel {
                        colors: root.colors
                        text: qsTr("language")
                    }

                    // Read-only: the locale is the desktop's to set, so the row says which one
                    // Omaweb found and what chose it, which is why the chrome speaks as it does.
                    SettingRow {
                        objectName: "languageRow"
                        width: pane.width
                        colors: root.colors
                        title: LocaleReport.title
                        note: (LocaleReport.variable.length > 0 ? qsTr(
                                                                      "Follows the system locale (%1). Shipped: %2.").arg(
                                                                      LocaleReport.variable) : qsTr(
                                                                      "Follows the system locale. Shipped: %1.")).arg(
                                  LocaleReport.shipped.join(", "))
                    }
                }

                // ---- keyboard ----------------------------------------------

                Column {
                    width: pane.width
                    visible: root.section === 2
                    spacing: pane.spacing

                    SettingToggle {
                        objectName: "keyboardNavigationEnabled"
                        width: pane.width
                        colors: root.colors
                        title: qsTr("Keyboard navigation")
                        note: qsTr(
                                  "Omaweb's own command layer. It gives the same commands with every engine, and lets sites receive the keys they need.")
                        accessibleName: qsTr("Enable Keyboard navigation")
                        checked: root.keyboard ? root.keyboard.enabled === true : false
                        onClicked: if (root.keyboard)
                                       root.keyboard.setEnabled(!checked)
                    }

                    // A binding this build cannot honour is dropped rather than
                    // taking the whole keymap with it, so this notice is the
                    // only thing that says a configured key is missing. It
                    // waits beside the setting it is about: over the page it
                    // would be a banner the reader cannot dismiss and cannot
                    // act on while browsing.
                    NoticeBox {
                        objectName: "keyboardBindingNotice"
                        width: pane.width
                        visible: root.keyboardReport.length > 0
                        colors: root.colors
                        iconFontFamily: root.iconFontFamily
                        glyph: "keyboard_alt"
                        title: qsTr("Some bindings were ignored")
                        detail: root.keyboardReport
                    }

                    // The plugin is the desktop's to install, so this names
                    // what is missing rather than offering to fix it. It sits
                    // here because an input method is keyboard input, and
                    // because the alternative is a reader typing into a page
                    // and watching nothing appear (#104).
                    NoticeBox {
                        objectName: "inputMethodNotice"
                        width: pane.width
                        visible: root.inputMethodMissing
                        colors: root.colors
                        iconFontFamily: root.iconFontFamily
                        glyph: "keyboard_alt"
                        title: qsTr("This desktop's input method is not installed")
                        detail: InputMethodReport.diagnostic
                    }
                }

                // ---- content blocking --------------------------------------

                Column {
                    width: pane.width
                    visible: root.section === 3
                    spacing: pane.spacing

                    SettingToggle {
                        objectName: "siteBlockingEnabled"
                        width: pane.width
                        colors: root.colors
                        title: qsTr("Block requests on this site")
                        note: root.activeHost.length > 0 ? qsTr(
                                                               "%1, and the switch covers every page on %2.").arg(
                                                               root.refusals.sentence).arg(
                                                               root.activeHost) : qsTr("%1.").arg(
                                                               root.refusals.sentence)
                        accessibleName: qsTr("Enable content blocking for this site")
                        checked: root.blocker && root.browser ? root.blocker.siteEnabled(
                                                                    root.browser.activeUrl) : false
                        onClicked: if (root.blocker)
                                       root.blocker.setSiteEnabled(root.browser.activeUrl, !checked)
                    }

                    Repeater {
                        model: root.section === 3 ? root.subscriptions : []

                        SettingToggle {
                            required property var modelData

                            width: pane.width
                            colors: root.colors
                            title: modelData.title
                            note: qsTr("%1 · %2\nSource %3\nUpdates from %4").arg(
                                      modelData.updateStatus).arg(modelData.license).arg(
                                      modelData.source).arg(modelData.updateAddress)
                            accessibleName: qsTr("Enable %1").arg(modelData.title)
                            checked: modelData.enabled
                            onClicked: root.blocker.setSubscriptionEnabled(modelData.id, !checked)
                        }
                    }

                    // No subscriptions is a state, not the absence of one.
                    // Left to the Repeater it drew as blank page between two
                    // headings, which reads as a section that failed to load
                    // rather than as a browser blocking nothing (#43). The
                    // default lists are offered back here because seeding
                    // happens once: nothing else brings them in. It waits for
                    // the blocker, because a button that cannot act is worse
                    // than no offer at all.
                    Column {
                        objectName: "noSubscriptionsNotice"
                        width: pane.width
                        visible: root.blocker !== null && root.blocker !== undefined
                                 && root.subscriptions.length === 0
                        spacing: Style.spacing.lg

                        Text {
                            objectName: "noSubscriptionsText"
                            width: pane.width
                            text: qsTr(
                                      "No filter lists. Network rules only block what the user rules below say to block.")
                            color: root.colors.mutedText
                            wrapMode: Text.WordWrap
                            font.family: Style.font.family
                            font.pixelSize: Style.font.body
                        }

                        ActionButton {
                            objectName: "restoreDefaultListsButton"
                            colors: root.colors
                            label: qsTr("Add EasyList and EasyPrivacy")
                            accessibleName: qsTr("Add the default filter lists")
                            onClicked: {
                                root.blocker.restoreDefaultSubscriptions();
                                root.refresh();
                            }
                        }
                    }

                    // A list Omaweb can name is offered by name, with the same
                    // provenance a subscription shows, and one action takes
                    // it. It waits for the blocker for the reason the offer
                    // above does. The cookie list is here rather than seeded
                    // because it takes a site's consent choice, which is the
                    // reader's to give away (#291).
                    Column {
                        objectName: "knownListOffers"
                        width: pane.width
                        visible: root.blocker !== null && root.blocker !== undefined
                                 && root.knownLists.length > 0

                        Repeater {
                            model: root.section === 3 ? root.knownLists : []

                            SettingRow {
                                required property var modelData

                                objectName: "knownListOffer"
                                width: pane.width
                                colors: root.colors
                                title: modelData.title
                                note: qsTr("Not subscribed · %1\nSource %2\nUpdates from %3").arg(
                                          modelData.license).arg(modelData.source).arg(
                                          modelData.updateAddress)

                                ActionButton {
                                    objectName: "subscribeKnownListButton"
                                    colors: root.colors
                                    label: qsTr("Subscribe")
                                    accessibleName: qsTr("Subscribe to %1").arg(modelData.title)
                                    onClicked: {
                                        root.blocker.subscribeKnownList(modelData.id);
                                        root.refresh();
                                    }
                                }
                            }
                        }
                    }

                    SectionLabel {
                        colors: root.colors
                        text: qsTr("add a list")
                    }

                    // Two fields to a row while a pair still fits the words
                    // they ask for, one to a row when it does not: a pair the
                    // theme has made too narrow to read stops being a pairing
                    // and starts being a clip, so how many share a row is
                    // derived rather than fixed at two (#82).
                    Grid {
                        id: subscriptionFields
                        objectName: "subscriptionFields"
                        columns: pane.width >= root.fieldFloor * 2 + columnSpacing ? 2 : 1
                        columnSpacing: Style.spacing.xl
                        rowSpacing: Style.spacing.lg

                        readonly property int fieldWidth: subscriptionFields.columns === 2
                                                          ? Math.max(1, (pane.width
                                                                         - columnSpacing) / 2) :
                                                            pane.width

                        SettingField {
                            id: subscriptionTitle
                            width: subscriptionFields.fieldWidth
                            colors: root.colors
                            placeholder: root.subscriptionPlaceholders.title
                            accessibleName: qsTr("Subscription name")
                        }

                        SettingField {
                            id: subscriptionLicense
                            width: subscriptionFields.fieldWidth
                            colors: root.colors
                            placeholder: root.subscriptionPlaceholders.license
                            accessibleName: qsTr("Subscription license")
                        }

                        SettingField {
                            id: subscriptionSource
                            width: subscriptionFields.fieldWidth
                            colors: root.colors
                            placeholder: root.subscriptionPlaceholders.source
                            accessibleName: qsTr("Subscription source page")
                        }

                        SettingField {
                            id: subscriptionUpdate
                            width: subscriptionFields.fieldWidth
                            colors: root.colors
                            placeholder: root.subscriptionPlaceholders.update
                            accessibleName: qsTr("Subscription update address")
                        }
                    }

                    ActionButton {
                        objectName: "addSubscriptionButton"
                        colors: root.colors
                        label: qsTr("Add subscription")
                        onClicked: {
                            root.blocker.addSubscription(subscriptionTitle.text,
                                                         subscriptionSource.text,
                                                         subscriptionLicense.text,
                                                         subscriptionUpdate.text);
                            root.refresh();
                        }
                    }

                    SectionLabel {
                        colors: root.colors
                        text: qsTr("user rules")
                    }

                    MultilineField {
                        id: userRules
                        objectName: "userRulesInput"
                        width: pane.width
                        colors: root.colors
                        placeholder: qsTr("one rule per line")
                        accessibleName: qsTr("User rules")
                    }

                    Text {
                        objectName: "contentBlockingSupport"
                        width: pane.width
                        text: [qsTr(
                                "Network rules, plain CSS cosmetic rules, parameter stripping, and the scriptlets and substitute resources in the bundled uBlock Origin library are supported; the scriptlets uBlock Origin gates behind trust are refused."),
                            root.proceduralCosmeticFilteringAvailable ? qsTr(
                                                                            "Procedural cosmetic rules written for a site are applied, except on the Ladybird engine.") :
                                                                        qsTr("Procedural cosmetic rules are not applied: the Ladybird engine lacks them."),
                            root.cnameUncloakingAvailable ? qsTr(
                                                                "Trackers behind a CNAME are refused, but a $cname rule that turns that off is not honoured. Response rewriting, content security policies, HTML filtering and dynamic rules are not supported.") :
                                                            qsTr("Response rewriting, content security policies, HTML filtering, dynamic rules, $cname rules and CNAME uncloaking are not supported; CNAME uncloaking needs Omaweb's own build of the Qt engine.")].join(
                            " ")
                        color: root.colors.mutedText
                        wrapMode: Text.WordWrap
                        font.family: Style.font.family
                        font.pixelSize: Style.font.caption
                    }

                    Text {
                        objectName: "contentBlockingUnsupported"
                        width: pane.width
                        visible: root.blocker !== null && root.blocker !== undefined
                                 && root.blocker.compilationReport.unsupported !== undefined
                                 && Object.keys(root.blocker.compilationReport.unsupported).length
                                 > 0
                        text: qsTr("Unsupported rules in the active lists: %1").arg(JSON.stringify(
                                                                                        root.blocker
                                                                                        ? root.blocker.compilationReport.unsupported :
                                                                                          {}))
                        color: root.colors.mutedText
                        wrapMode: Text.WordWrap
                        font.family: Style.font.family
                        font.pixelSize: Style.font.caption
                    }

                    ActionButton {
                        objectName: "saveUserRulesButton"
                        colors: root.colors
                        label: root.blocker && root.blocker.compiling ? qsTr("Compiling…") : qsTr(
                                                                            "Save user rules")

                        accessibleName: qsTr("Save user rules")
                        primary: true
                        enabled: root.blocker ? !root.blocker.compiling : false
                        onClicked: root.blocker.userRules = userRules.text
                    }
                }

                // ---- network -----------------------------------------------

                Column {
                    width: pane.width
                    visible: root.section === 4
                    spacing: 0

                    // Browser-wide and off by default: on, what is typed in
                    // the Omnibar leaves the machine before any commit. The
                    // note names where it goes, which follows the default
                    // engine, and the switch keeps its value when that engine
                    // has nowhere to ask.
                    SettingToggle {
                        objectName: "engineSuggestions"
                        visible: !!root.engineSuggestions
                        width: pane.width
                        colors: root.colors
                        title: qsTr("Engine suggestions")
                        note: root.engineSuggestionsNote(root.defaultEngine)
                        accessibleName: qsTr("Engine suggestions")
                        checked: !!root.engineSuggestions && root.engineSuggestions.enabled
                        onClicked: {
                            if (root.engineSuggestions)
                                root.engineSuggestions.enabled = !checked;
                        }
                    }

                    SettingRow {
                        width: pane.width
                        colors: root.colors
                        title: qsTr("Filter-list updates")

                        Text {
                            objectName: "automaticRequestsStatus"
                            width: Math.min(root.noteMeasure, pane.width / 2)
                            text: qsTr(
                                      "Enabled filter-list subscriptions make automatic network requests to their displayed update address when Omaweb starts.")
                            color: root.colors.mutedText
                            wrapMode: Text.WordWrap
                            font.family: Style.font.family
                            font.pixelSize: Style.font.caption
                        }
                    }
                }

                // ---- downloads ---------------------------------------------

                Column {
                    width: pane.width
                    visible: root.section === 5
                    spacing: 0

                    SettingRow {
                        width: pane.width
                        colors: root.colors
                        separated: false
                        title: qsTr("Download directory")
                        note: root.browser ? root.browser.downloadDirectory : ""

                        ActionButton {
                            objectName: "chooseDownloadDirectory"
                            colors: root.colors
                            label: qsTr("Change")
                            accessibleName: qsTr("Change the download directory")
                            enabled: root.browser ? !root.browser.privateBrowsing : false
                            onClicked: root.downloadDirectoryRequested()
                        }
                    }

                    Repeater {
                        model: root.section === 5 ? root.downloads : null

                        SettingRow {
                            id: downloadRow
                            // Taken through `model` rather than one required
                            // property each: a row's state is one of the
                            // model's roles, and an Item already has a `state`.
                            required property int index
                            required property var model

                            objectName: "recordedDownload-" + index
                            width: pane.width
                            colors: root.colors
                            title: String(downloadRow.model.path)
                            note: {
                                const downloadState = String(downloadRow.model.state);
                                const error = String(downloadRow.model.error || "");
                                const received = Number(downloadRow.model.receivedBytes || 0);
                                const total = Number(downloadRow.model.totalBytes || 0);
                                let line = root.downloadStateLabel(downloadState);
                                if (downloadState === "in-progress" && total > 0)
                                    line += " · " + qsTr("%1%").arg(Math.floor(received * 100
                                                                               / total));
                                else if (downloadState === "in-progress" && received > 0)
                                    line += " · " + root.resourceLabel(received);
                                if (error.length > 0)
                                    line += " · " + error;
                                return line;
                            }

                            Row {
                                spacing: Style.spacing.md

                                ActionButton {
                                    objectName: "cancelDownload-" + index
                                    colors: root.colors
                                    label: qsTr("Cancel")
                                    accessibleName: qsTr("Cancel this download")
                                    visible: String(downloadRow.model.state) === "in-progress"
                                             && String(downloadRow.model.runtimeId || "").length > 0
                                    onClicked: root.downloadCancelled(downloadRow.index)
                                }

                                ActionButton {
                                    objectName: "retryDownload-" + index
                                    colors: root.colors
                                    label: qsTr("Retry")
                                    accessibleName: qsTr("Retry this download")
                                    visible: String(downloadRow.model.state) === "interrupted"
                                    onClicked: root.downloadRetried(downloadRow.index)
                                }

                                ActionButton {
                                    objectName: "revealDownload-" + index
                                    colors: root.colors
                                    label: qsTr("Show")
                                    accessibleName: qsTr("Show where this download landed")
                                    visible: String(downloadRow.model.state) === "completed"
                                    onClicked: root.downloadRevealed(String(downloadRow.model.path))
                                }

                                ActionButton {
                                    objectName: "forgetDownload-" + index
                                    colors: root.colors
                                    label: qsTr("Remove")
                                    accessibleName: qsTr("Remove this download from the history")
                                    visible: String(downloadRow.model.recordId || "").length > 0
                                             && String(downloadRow.model.state) !== "in-progress"
                                    onClicked: root.downloadForgotten(downloadRow.index)
                                }
                            }
                        }
                    }

                    SettingRow {
                        width: pane.width
                        visible: root.downloads ? root.downloads.count === 0 : true
                        colors: root.colors
                        title: qsTr("No recorded downloads")
                        note: qsTr("Downloads Omaweb has recorded in this Space appear here.")
                    }
                }

                // ---- search ------------------------------------------------

                Column {
                    width: pane.width
                    visible: root.section === 6
                    spacing: pane.spacing

                    Column {
                        width: pane.width
                        spacing: 0

                        Repeater {
                            id: searchEngineList
                            objectName: "searchEngineList"
                            model: root.section === 6 ? root.engines : []

                            SettingRow {
                                required property var modelData

                                width: pane.width
                                colors: root.colors
                                title: modelData.default ? qsTr("%1 · default").arg(modelData.name) :
                                                           modelData.name
                                note: modelData.queryUrl

                                Row {
                                    spacing: Style.spacing.lg

                                    Text {
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: modelData.keyword.length > 0 ? qsTr("keyword %1").arg(
                                                                                 modelData.keyword) :
                                                                             qsTr("no keyword")
                                        color: root.colors.mutedText
                                        font.family: Style.font.family
                                        font.pixelSize: Style.font.caption
                                    }

                                    ActionButton {
                                        colors: root.colors
                                        label: qsTr("Default")
                                        visible: !modelData.default
                                        onClicked: root.makeDefaultSearchEngine(modelData.id)
                                    }

                                    ActionButton {
                                        colors: root.colors
                                        label: qsTr("Delete")
                                        destructive: true
                                        enabled: root.engines.length > 1
                                        onClicked: root.deleteSearchEngine(modelData.id)
                                    }
                                }
                            }
                        }
                    }

                    SectionLabel {
                        colors: root.colors
                        text: qsTr("add a provider")
                    }

                    Row {
                        id: providerRow
                        width: pane.width
                        spacing: Style.spacing.lg

                        SettingDropdown {
                            id: providerPreset
                            objectName: "searchProviderPreset"
                            width: providerRow.width - addProvider.width - providerRow.spacing
                            colors: root.colors
                            options: root.enginePresets.map(function (engine) {
                                return {
                                    value: engine.id,
                                    label: engine.name
                                };
                            })
                            value: options.length > 0 ? String(options[0].value) : ""
                            accessibleName: qsTr("Predefined search provider")
                        }

                        ActionButton {
                            id: addProvider
                            objectName: "addSearchProviderButton"
                            colors: root.colors
                            label: root.searchEngineInstalled(providerPreset.value) ? qsTr("Added") :
                                                                                      qsTr("Add")
                            enabled: providerPreset.value !== "" && !root.searchEngineInstalled(
                                         providerPreset.value)
                            onClicked: {
                                if (root.browser.addSearchEnginePreset(providerPreset.value))
                                    root.refresh();
                            }
                        }
                    }

                    SectionLabel {
                        colors: root.colors
                        text: qsTr("add a custom engine")
                    }

                    SettingField {
                        id: engineName
                        objectName: "engineName"
                        width: pane.width
                        colors: root.colors
                        placeholder: qsTr("name", "placeholder: a search engine's name")
                        accessibleName: qsTr("Search engine name")
                    }

                    SettingField {
                        id: engineQueryUrl
                        objectName: "engineQueryUrl"
                        width: pane.width
                        colors: root.colors
                        placeholder: qsTr("query URL with {query}")
                        accessibleName: qsTr("Search engine query URL")
                    }

                    // The field and the caption that says what it must answer
                    // are one block, so the caption sits under its own field
                    // rather than a form's gap away.
                    Column {
                        width: pane.width
                        spacing: Style.spacing.xs

                        SettingField {
                            id: engineSuggestUrl
                            objectName: "engineSuggestUrl"
                            width: parent.width
                            colors: root.colors
                            placeholder: qsTr("optional suggest URL with {query}")
                            accessibleName: qsTr("Search engine suggest URL")
                        }

                        Text {
                            objectName: "engineSuggestUrlCaption"
                            width: parent.width
                            text: qsTr(
                                      "Answers in OpenSearch suggestions JSON. Leave empty if the engine offers none.")
                            color: root.colors.mutedText
                            wrapMode: Text.WordWrap
                            font.family: Style.font.family
                            font.pixelSize: Style.font.caption
                        }
                    }

                    SettingField {
                        id: engineKeyword
                        objectName: "engineKeyword"
                        width: pane.width
                        colors: root.colors
                        placeholder: qsTr("optional keyword")
                        accessibleName: qsTr("Search engine keyword")
                    }

                    ActionButton {
                        objectName: "addSearchEngineButton"
                        colors: root.colors
                        label: qsTr("Add and make default")
                        enabled: engineName.text.trim().length > 0 && engineQueryUrl.text.indexOf(
                                     "{query}") >= 0 && (engineSuggestUrl.text.trim().length === 0
                                                         || engineSuggestUrl.text.indexOf(
                                                             "{query}") >= 0)
                        onClicked: {
                            if (root.browser.addSearchEngine(engineName.text, engineQueryUrl.text,
                                                             engineKeyword.text,
                                                             engineSuggestUrl.text)) {
                                engineName.text = "";
                                engineQueryUrl.text = "";
                                engineSuggestUrl.text = "";
                                engineKeyword.text = "";
                                root.refresh();
                            }
                        }
                    }
                }

                // ---- privacy -----------------------------------------------

                Column {
                    width: pane.width
                    visible: root.section === 7
                    spacing: 0

                    // The section is named for what it will hold rather than
                    // for its one row: the reputation protection, proxy
                    // reporting and third-party cookie allowances the
                    // requirements describe are privacy and are not browsing
                    // data.
                    //
                    // It opens the section, so it takes only the sliver a tall
                    // glyph paints above its box: the lean the label carries by
                    // default is separation from what precedes it, and leaning
                    // away from nothing is dead space.
                    SectionLabel {
                        id: privacyLabel
                        colors: root.colors
                        topPadding: privacyLabel.overshoot
                        text: qsTr("privacy")
                    }

                    // One row and one button, which is the shape Chrome and
                    // Firefox both settled on. What used to be five switches
                    // and a scope switch on this page were arguments to that
                    // button, and they now sit in the dialog it opens (ADR
                    // 0031). The section has room to grow into the reputation
                    // protection, proxy reporting and third-party cookie
                    // allowances Omaweb has not built.
                    SettingRow {
                        objectName: "clearBrowsingDataRow"
                        width: pane.width
                        colors: root.colors
                        title: qsTr("Browsing data")
                        note: qsTr(
                                  "Cookies, site storage, cache, site permissions and history — for the Spaces and the time range chosen when clearing.")

                        ActionButton {
                            objectName: "clearBrowsingDataButton"
                            colors: root.colors
                            destructive: true
                            label: qsTr("Clear browsing data…")
                            onClicked: root.clearDataOpen = true
                        }
                    }

                    // Browser-wide rather than per Space: a Space separates
                    // identities and says nothing about what the reader wants
                    // said about them, so there is one switch and a Private
                    // window is bound by it too.
                    SettingToggle {
                        objectName: "globalPrivacyControl"
                        visible: !!root.globalPrivacyControl
                        width: pane.width
                        colors: root.colors
                        title: qsTr("Global Privacy Control")
                        note: qsTr(
                                  "Tells every site not to sell or share your data, with the Sec-GPC header on every request and navigator.globalPrivacyControl on every page. Sites bound by the CCPA and similar laws must honour it.")
                        accessibleName: qsTr("Global Privacy Control")
                        checked: !!root.globalPrivacyControl && root.globalPrivacyControl.enabled
                        onClicked: {
                            if (root.globalPrivacyControl)
                                root.globalPrivacyControl.enabled = !checked;
                        }
                    }

                    // Browser-wide, like Global Privacy Control, and on by
                    // default: refusing plaintext is the one transport
                    // protection Omaweb offers on its own.
                    SettingToggle {
                        objectName: "httpsOnly"
                        visible: !!root.httpsOnly
                        width: pane.width
                        colors: root.colors
                        title: qsTr("HTTPS-only mode")
                        note: qsTr(
                                  "Sends every page's own address over HTTPS, whoever wrote the link. Where a site cannot be reached that way, Omaweb asks before loading it over plain HTTP. Local development addresses are left alone.")
                        accessibleName: qsTr("HTTPS-only mode")
                        checked: !!root.httpsOnly && root.httpsOnly.enabled
                        onClicked: {
                            if (root.httpsOnly)
                                root.httpsOnly.enabled = !checked;
                        }
                    }

                    // Browser-wide, like the switches beside it: the engine
                    // resolves names once for every profile. Secure mode only,
                    // so a resolver that cannot be reached fails a lookup
                    // rather than sending it to the system in the clear, and
                    // the note says what that costs on a captive portal.
                    SettingRow {
                        id: secureDnsGroup

                        // The dropdown's own choice, which runs ahead of the
                        // model while a typed address has not been taken yet.
                        property string chosen: root.secureDns ? root.secureDns.resolver : ""
                        property bool addressRefused: false

                        objectName: "secureDns"
                        visible: !!root.secureDns
                        width: pane.width
                        colors: root.colors
                        title: qsTr("Secure DNS")
                        note: qsTr(
                                  "Looks up the sites you visit over an encrypted connection to a resolver you choose, instead of your system's. That resolver learns every site you visit. A network that blocks it stops pages loading, so turn it off to sign in to a hotel or airport network.")

                        Column {
                            width: Style.spacing.dropdownWidth
                            spacing: Style.spacing.sm

                            SettingDropdown {
                                objectName: "secureDnsResolver"
                                colors: root.colors
                                accessibleName: qsTr("Secure DNS resolver")
                                options: {
                                    const options = [
                                              {
                                                  value: "",
                                                  label: qsTr("Off")
                                              }
                                          ];
                                    const resolvers = root.secureDns ? root.secureDns.resolvers :
                                                                       [];
                                    for (let i = 0; i < resolvers.length; ++i)
                                        options.push({
                                                         value: resolvers[i].id,
                                                         label: resolvers[i].title
                                                     });
                                    options.push({
                                                     value: "custom",
                                                     label: qsTr("An address you type")
                                                 });
                                    return options;
                                }
                                value: secureDnsGroup.chosen
                                onChanged: function (choice) {
                                    secureDnsGroup.chosen = choice;
                                    secureDnsGroup.addressRefused = false;
                                    if (!root.secureDns)
                                        return;
                                    if (choice === "")
                                        root.secureDns.turnOff();
                                    else if (choice !== "custom")
                                        root.secureDns.useResolver(choice);
                                }
                            }

                            SettingField {
                                objectName: "secureDnsAddress"
                                visible: secureDnsGroup.chosen === "custom"
                                width: parent.width
                                colors: root.colors
                                placeholder: qsTr("https://dns.example/dns-query")
                                accessibleName: qsTr("Secure DNS address")
                                text: root.secureDns ? root.secureDns.customTemplate : ""
                                onAccepted: {
                                    secureDnsGroup.addressRefused = !!root.secureDns &&
                                            !root.secureDns.useCustom(text);
                                }
                            }

                            Text {
                                objectName: "secureDnsAddressRefused"
                                visible: secureDnsGroup.addressRefused
                                width: parent.width
                                text: qsTr(
                                          "That is not an https: address, so names would not be encrypted. Nothing was changed.")
                                color: root.colors.urgent
                                wrapMode: Text.WordWrap
                                font.family: Style.font.family
                                font.pixelSize: Style.font.caption
                            }

                            Text {
                                objectName: "secureDnsInUse"
                                width: parent.width
                                text: !root.secureDns || root.secureDns.resolver === "" ? qsTr(
                                                                                              "Names are looked up by your system's resolver.") :
                                                                                          root.engineSecureDns
                                                                                          && !root.engineSecureDns.applied
                                                                                          ? qsTr("The engine would not take this resolver, so names are looked up by your system's resolver.") :
                                                                                            qsTr("Names are looked up by %1, over an encrypted connection.").arg(
                                                                                                root.secureDns.resolverTitle)
                                color: root.engineSecureDns && !root.engineSecureDns.applied
                                       ? root.colors.urgent : root.colors.mutedText
                                wrapMode: Text.WordWrap
                                font.family: Style.font.family
                                font.pixelSize: Style.font.caption
                            }
                        }
                    }

                    // A page gathers a call's candidate addresses before any
                    // call is placed, on every interface the host has, so
                    // browser-wide for the same reason as the signal above: a
                    // Space says nothing about what the reader wants leaked.
                    SettingToggle {
                        objectName: "webRtcPublicInterfacesOnly"
                        visible: !!root.webRtcPolicy && !root.webRtcPolicyUnreachable
                        width: pane.width
                        colors: root.colors
                        title: qsTr("Keep calls off your other addresses")
                        note: qsTr(
                                  "A page setting up a call learns only the address of the connection it leaves on, not your local network's or the one a VPN hides. Calls still connect. Turn off to reach a peer on your own network directly.")
                        accessibleName: qsTr("Keep calls off your other addresses")
                        checked: !!root.webRtcPolicy && root.webRtcPolicy.publicInterfacesOnly
                        onClicked: {
                            if (root.webRtcPolicy)
                                root.webRtcPolicy.publicInterfacesOnly = !checked;
                        }
                    }

                    // The engine's own default offers every interface, and a
                    // build that cannot reach the profile's settings leaves
                    // it there. Named as a fact rather than hidden behind a
                    // switch that would change nothing.
                    NoticeBox {
                        objectName: "webRtcPolicyNotice"
                        width: pane.width
                        visible: root.webRtcPolicyUnreachable
                        colors: root.colors
                        iconFontFamily: root.iconFontFamily
                        glyph: "call"
                        title: qsTr("This build cannot keep calls off your other addresses")
                        detail: qsTr(
                                    "Omaweb was built against another Qt than the one it is running on, so a page setting up a call is offered every address this machine has, the engine's own default. A rebuild against this Qt brings the setting back.")
                    }

                    // Each row is here only on an engine that lacks what it
                    // names, so a capable build shows none of them.
                    SectionLabel {
                        objectName: "engineCaveatsLabel"
                        colors: root.colors
                        visible: !root.certificateDecisionsAvailable ||
                                 !root.pageCertificatesAvailable ||
                                 !root.thirdPartyCookieControlAvailable || !root.siteDataOnDisk ||
                                 !root.insecureContentBlocked
                        text: qsTr("what this engine cannot do")
                    }

                    SettingRow {
                        objectName: "engineCaveat_certificateFailures"
                        width: pane.width
                        visible: !root.certificateDecisionsAvailable
                        colors: root.colors
                        title: qsTr("This engine cannot report a certificate failure")
                        note: qsTr(
                                  "A page whose certificate fails is not stopped for a question, and Site information cannot say it failed.")
                    }

                    SettingRow {
                        objectName: "engineCaveat_pageCertificates"
                        width: pane.width
                        visible: !root.pageCertificatesAvailable
                        colors: root.colors
                        title: qsTr("This engine cannot show the certificate a page arrived over")
                        note: qsTr(
                                  "Site information shows only the certificate a failure was raised for.")
                    }

                    SettingRow {
                        objectName: "engineCaveat_thirdParties"
                        width: pane.width
                        visible: !root.thirdPartyCookieControlAvailable
                        colors: root.colors
                        title: qsTr("This engine cannot refuse a third party")
                        note: qsTr("Sites embedded in a page keep their cookies and storage there.")
                    }

                    SettingRow {
                        objectName: "engineCaveat_siteData"
                        width: pane.width
                        visible: !root.siteDataOnDisk
                        colors: root.colors
                        title: qsTr("This engine keeps no site data on disk")
                        note: qsTr(
                                  "Cookies and storage last until Omaweb closes, so there is no size for Site information to give.")
                    }

                    SettingRow {
                        objectName: "engineCaveat_insecureContent"
                        width: pane.width
                        visible: !root.insecureContentBlocked
                        colors: root.colors
                        title: qsTr("This engine is not blocking insecure content")
                        note: qsTr("A secure page can load scripts and frames over plain HTTP.")
                    }

                    SectionLabel {
                        colors: root.colors
                        text: qsTr("process isolation")
                    }

                    SettingRow {
                        objectName: "rendererIsolationRow"
                        width: pane.width
                        colors: root.colors
                        title: RuntimeSecurity.rendererIsolated ? qsTr(
                                                                      "Renderer isolation verified") :
                                                                  qsTr("Renderer isolation unverified")
                        note: RuntimeSecurity.rendererIsolation
                    }

                    SettingRow {
                        objectName: "networkServiceRow"
                        width: pane.width
                        colors: root.colors
                        title: qsTr("Network service in the browser process")
                        note: RuntimeSecurity.networkService
                    }

                    SettingRow {
                        objectName: "securityBaselineRow"
                        width: pane.width
                        colors: root.colors
                        title: RuntimeSecurity.meetsSecurityBaseline ? qsTr(
                                                                           "Engine at the approved security baseline") :
                                                                       qsTr("Unsupported preview: engine below the approved baseline")
                        note: RuntimeSecurity.securityBaseline
                    }
                }

                // ---- addresses ----------------------------------------------

                // Saved addresses as rows, and the fields of the one being
                // added or edited in place under them, the way a custom search
                // engine's sit in their section.
                Column {
                    width: pane.width
                    visible: root.section === 8
                    spacing: pane.spacing

                    Column {
                        width: pane.width
                        spacing: 0

                        Repeater {
                            id: addressList
                            objectName: "addressList"
                            model: root.section === 8 ? root.savedAddresses : []

                            SettingRow {
                                required property var modelData
                                readonly property string detail: root.addressDetail(modelData)

                                width: pane.width
                                colors: root.colors
                                title: detail.length > 0 ? qsTr("%1 · %2",
                                                                "an address: its name · street, city").arg(
                                                               modelData.name).arg(detail) :
                                                           modelData.name

                                Row {
                                    spacing: Style.spacing.lg

                                    ActionButton {
                                        objectName: "editAddressButton"
                                        colors: root.colors
                                        label: qsTr("Edit", "verb: edit a saved address")
                                        onClicked: root.editAddress(modelData)
                                    }

                                    ActionButton {
                                        objectName: "removeAddressButton"
                                        colors: root.colors
                                        label: qsTr("Remove")
                                        destructive: true
                                        onClicked: root.removeAddress(modelData.id)
                                    }
                                }
                            }
                        }

                        SettingRow {
                            objectName: "noAddresses"
                            width: pane.width
                            visible: root.savedAddresses.length === 0
                            colors: root.colors
                            title: qsTr("No saved addresses")
                            note: root.browser && root.browser.privateBrowsing ? qsTr(
                                                                                     "Addresses are saved in a regular window.") :
                                                                                 qsTr("Addresses you save here are offered in forms in every Space, never in a Private window.")
                        }
                    }

                    ActionButton {
                        objectName: "addAddressButton"
                        colors: root.colors
                        label: qsTr("Add address")
                        visible: !root.addressEditing && (root.browser ?
                                                              !root.browser.privateBrowsing : false)
                        onClicked: root.editAddress(null)
                    }

                    Column {
                        width: pane.width
                        visible: root.addressEditing
                        spacing: pane.spacing

                        SectionLabel {
                            colors: root.colors
                            text: root.editingAddressId.length > 0 ? qsTr("edit address") : qsTr(
                                                                         "add an address")
                        }

                        SettingField {
                            id: addressName
                            objectName: "addressName"
                            width: pane.width
                            colors: root.colors
                            placeholder: qsTr("name", "placeholder: the name an address is for")
                            accessibleName: qsTr("Address name")
                        }

                        SettingField {
                            id: addressStreet
                            objectName: "addressStreet"
                            width: pane.width
                            colors: root.colors
                            placeholder: qsTr("street")
                            accessibleName: qsTr("Address street")
                        }

                        Row {
                            id: addressPlaceRow
                            width: pane.width
                            spacing: Style.spacing.lg

                            SettingField {
                                id: addressPostalCode
                                objectName: "addressPostalCode"
                                width: (addressPlaceRow.width - addressPlaceRow.spacing) / 3
                                colors: root.colors
                                placeholder: qsTr("postal code")
                                accessibleName: qsTr("Address postal code")
                            }

                            SettingField {
                                id: addressCity
                                objectName: "addressCity"
                                width: addressPlaceRow.width - addressPostalCode.width
                                       - addressPlaceRow.spacing
                                colors: root.colors
                                placeholder: qsTr("city")
                                accessibleName: qsTr("Address city")
                            }
                        }

                        SettingField {
                            id: addressCountry
                            objectName: "addressCountry"
                            width: pane.width
                            colors: root.colors
                            placeholder: qsTr("country")
                            accessibleName: qsTr("Address country")
                        }

                        SettingField {
                            id: addressPhone
                            objectName: "addressPhone"
                            width: pane.width
                            colors: root.colors
                            placeholder: qsTr("phone")
                            accessibleName: qsTr("Address phone")
                        }

                        SettingField {
                            id: addressEmail
                            objectName: "addressEmail"
                            width: pane.width
                            colors: root.colors
                            placeholder: qsTr("email")
                            accessibleName: qsTr("Address email")
                        }

                        Row {
                            spacing: Style.spacing.lg

                            ActionButton {
                                objectName: "saveAddressButton"
                                colors: root.colors
                                label: qsTr("Save")
                                enabled: addressName.text.trim().length > 0
                                onClicked: root.saveAddress()
                            }

                            ActionButton {
                                objectName: "cancelAddressButton"
                                colors: root.colors
                                label: qsTr("Cancel")
                                onClicked: root.addressEditing = false
                            }
                        }
                    }
                }

                // ---- payment cards ------------------------------------------

                // Saved cards as rows, and the fields of the one being added or
                // edited in place under them, as an address's are. They are
                // kept in the desktop's keyring and nowhere else.
                Column {
                    width: pane.width
                    visible: root.section === root.cardsSection
                    spacing: pane.spacing

                    Column {
                        width: pane.width
                        spacing: 0

                        Repeater {
                            id: cardList
                            objectName: "cardList"
                            model: root.section === root.cardsSection ? root.savedCards : []

                            SettingRow {
                                required property var modelData

                                width: pane.width
                                colors: root.colors
                                title: root.cardTitle(modelData)
                                note: root.cardDetail(modelData)

                                Row {
                                    spacing: Style.spacing.lg

                                    ActionButton {
                                        objectName: "editCardButton"
                                        colors: root.colors
                                        label: qsTr("Edit", "verb: edit a saved card")
                                        onClicked: root.editCard(modelData)
                                    }

                                    ActionButton {
                                        objectName: "removeCardButton"
                                        colors: root.colors
                                        label: qsTr("Remove")
                                        destructive: true
                                        onClicked: root.removeCard(modelData.id)
                                    }
                                }
                            }
                        }

                        SettingRow {
                            id: noCards
                            objectName: "noCards"
                            readonly property string state: root.browser
                                                            ? root.browser.paymentCardsState :
                                                              "unavailable"
                            readonly property var said: root.privateWindow ? ({
                                                                                  "title": qsTr(
                                                                                               "No saved cards"),
                                                                                  "note": qsTr(
                                                                                              "Payment cards are saved in a regular window.")
                                                                              }) : root.keyringSays(
                                                                                 state)
                            width: pane.width
                            visible: root.savedCards.length === 0
                            colors: root.colors
                            title: said.title
                            note: said.note

                            ActionButton {
                                objectName: "readCardsAgainButton"
                                visible: noCards.state === "unreachable" || noCards.state
                                         === "locked" || noCards.state === "failed"
                                colors: root.colors
                                label: qsTr("Try again",
                                            "button: ask the keyring for the cards again")
                                onClicked: root.browser.readPaymentCardsAgain()
                            }
                        }
                    }

                    ActionButton {
                        objectName: "addCardButton"
                        colors: root.colors
                        label: qsTr("Add card")
                        visible: !root.cardEditing && !!root.browser && (
                                     root.browser.paymentCardsState === "ready"
                                     || root.browser.paymentCardsState === "locked")
                        onClicked: root.editCard(null)
                    }

                    Column {
                        width: pane.width
                        visible: root.cardEditing
                        spacing: pane.spacing

                        SectionLabel {
                            colors: root.colors
                            text: root.editingCardId.length > 0 ? qsTr("edit card") : qsTr(
                                                                      "add a card")
                        }

                        SettingField {
                            id: cardNumber
                            objectName: "cardNumber"
                            width: pane.width
                            colors: root.colors
                            placeholder: root.editingCardLast4.length > 0 ? "•••• "
                                                                            + root.editingCardLast4 :
                                                                            qsTr("card number")
                            accessibleName: root.editingCardLast4.length > 0 ? qsTr(
                                                                                   "Card number, ending %1, type another to replace it").arg(
                                                                                   root.editingCardLast4) :
                                                                               qsTr("Card number")
                            inputMethodHints: Qt.ImhDigitsOnly | Qt.ImhSensitiveData
                                              | Qt.ImhNoPredictiveText
                        }

                        SettingField {
                            id: cardHolder
                            objectName: "cardHolder"
                            width: pane.width
                            colors: root.colors
                            placeholder: qsTr("name on card")
                            accessibleName: qsTr("Name on card")
                        }

                        Row {
                            id: cardExpiryRow
                            width: pane.width
                            spacing: Style.spacing.lg

                            SettingField {
                                id: cardExpiry
                                objectName: "cardExpiry"
                                width: (cardExpiryRow.width - cardExpiryRow.spacing) / 3
                                colors: root.colors
                                placeholder: qsTr("MM/YY", "placeholder: a card's expiry")
                                accessibleName: qsTr("Card expiry")
                            }

                            SettingField {
                                id: cardNickname
                                objectName: "cardNickname"
                                width: cardExpiryRow.width - cardExpiry.width
                                       - cardExpiryRow.spacing
                                colors: root.colors
                                placeholder: qsTr("nickname")
                                accessibleName: qsTr("Card nickname")
                            }
                        }

                        Text {
                            objectName: "cardError"
                            width: pane.width
                            visible: root.cardRefused
                            text: qsTr(
                                      "That is not a card number, or the expiry is not a month and year.")
                            color: root.colors.urgent
                            font.family: Style.font.family
                            font.pixelSize: Style.font.bodySmall
                            wrapMode: Text.Wrap
                        }

                        Text {
                            objectName: "cardNotKept"
                            width: pane.width
                            visible: root.cardNotKept
                            text: qsTr(
                                      "The keyring stayed locked, so the card was not saved. Saving it again asks the desktop to unlock it.")
                            color: root.colors.urgent
                            font.family: Style.font.family
                            font.pixelSize: Style.font.bodySmall
                            wrapMode: Text.Wrap
                        }

                        Row {
                            spacing: Style.spacing.lg

                            ActionButton {
                                objectName: "saveCardButton"
                                colors: root.colors
                                label: qsTr("Save")
                                enabled: root.unlockingCardId.length === 0 && (cardNumber.text.trim(
                                                                                   ).length > 0
                                                                               || root.editingCardId.length
                                                                               > 0)
                                onClicked: root.saveCard()
                            }

                            ActionButton {
                                objectName: "cancelCardButton"
                                colors: root.colors
                                label: qsTr("Cancel")
                                onClicked: {
                                    cardNumber.text = "";
                                    root.unlockingCardId = "";
                                    root.cardNotKept = false;
                                    root.cardEditing = false;
                                }
                            }
                        }
                    }
                }

                // ---- spaces -------------------------------------------------

                Column {
                    width: pane.width
                    visible: root.section === 10
                    spacing: pane.spacing

                    ActionButton {
                        objectName: "newSpaceButton"
                        colors: root.colors
                        label: qsTr("New Space")
                        visible: root.browser ? !root.browser.privateBrowsing : false
                        onClicked: root.newSpaceRequested()
                    }

                    Column {
                        width: pane.width
                        spacing: 0

                        Repeater {
                            id: spaceList
                            model: root.browser && !root.browser.privateBrowsing
                                   ? root.browser.spaces : null

                            SettingRow {
                                id: spaceRow
                                required property int index
                                required property string spaceId
                                required property string spaceName
                                required property string spaceColor
                                required property bool active
                                // An Agent Space is drawn in no palette colour,
                                // so it is offered none.
                                readonly property bool agentMade: root.browser
                                                                  && root.browser.agentSpaceIds
                                                                  ? root.browser.agentSpaceIds.indexOf(
                                                                        spaceId) >= 0 : false
                                // Whether the row a step away is the same kind,
                                // the reader's or an Agent's: a move never
                                // crosses from one to the other.
                                function besideOwnKind(offset) {
                                    const beside = spaceList.itemAt(index + offset);
                                    return beside !== null && beside.agentMade === agentMade;
                                }
                                // The Space's project (`omaweb dev`), shown
                                // here and never edited here.
                                readonly property var project: root.browser
                                                               && root.browser.spaceProjects
                                                               ? root.browser.spaceProjects[spaceId]
                                                                 || null : null
                                objectName: "settingsSpace-" + spaceId
                                width: pane.width
                                colors: root.colors
                                title: spaceName
                                note: [active ? qsTr("Current Space") : "", root.projectNote(
                                        project)].filter(function (line) {
                                            return line.length > 0;
                                        }).join("\n")
                                height: Math.max(implicitHeight, spaceActions.implicitHeight
                                                 + verticalPadding * 2)

                                Flow {
                                    id: spaceActions
                                    width: Math.min(pane.width * 0.7, spaceSwatches.width
                                                    + renameSpace.width + deleteSpace.width
                                                    + moveSpaceUp.width + moveSpaceDown.width
                                                    + spacing * 4 + (forgetProject.visible
                                                                     ? forgetProject.width
                                                                       + spacing : 0))
                                    spacing: Style.spacing.sm

                                    // The six colours a Space may be drawn in,
                                    // each in the theme's own value. The
                                    // Space's own is the larger square. Two
                                    // Spaces may share a colour.
                                    Row {
                                        id: spaceSwatches
                                        height: 26
                                        spacing: 4

                                        Repeater {
                                            // The theme says which: a light theme
                                            // offers only the plain three.
                                            model: spaceRow.agentMade ? [] :
                                                                        root.colors.spaceColourNames
                                                                        || []

                                            AbstractButton {
                                                id: swatch
                                                required property string modelData
                                                readonly property string spoken: root.colourName(
                                                                                     modelData)
                                                objectName: "spaceSwatch-" + spaceRow.spaceId + "-"
                                                            + modelData
                                                width: 22
                                                height: 26
                                                checkable: false
                                                checked: spaceRow.spaceColor === modelData || (
                                                             spaceRow.spaceColor === "bright_"
                                                             + modelData && (
                                                                 root.colors.spaceColourNames
                                                                 || []).indexOf(
                                                                 spaceRow.spaceColor) < 0)
                                                activeFocusOnTab: true
                                                focusPolicy: Qt.StrongFocus
                                                Accessible.role: Accessible.RadioButton
                                                Accessible.name: qsTr("%1 for %2").arg(
                                                                     swatch.spoken).arg(
                                                                     spaceRow.spaceName)
                                                Accessible.checked: checked
                                                onClicked: root.browser.setSpaceColour(
                                                               spaceRow.spaceId, modelData)

                                                background: Item {}
                                                contentItem: Item {
                                                    // Where the keyboard is, and
                                                    // nothing more: the chosen
                                                    // colour is the larger
                                                    // square, as the Space on
                                                    // show is in the footer.
                                                    Rectangle {
                                                        anchors.centerIn: parent
                                                        width: 20
                                                        height: 20
                                                        radius: 4
                                                        color: "transparent"
                                                        visible: swatch.activeFocus
                                                        border.width: 1
                                                        border.color: root.colors.text
                                                    }
                                                    Rectangle {
                                                        objectName: "spaceSwatchFill"
                                                        anchors.centerIn: parent
                                                        width: swatch.checked ? 14 : 10
                                                        height: width
                                                        radius: 2
                                                        color: root.colors.spaces
                                                               ? root.colors.spaces[swatch.modelData] :
                                                                 root.colors.accent
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    ActionButton {
                                        id: renameSpace
                                        objectName: "renameSpace-" + spaceRow.spaceId
                                        colors: root.colors
                                        label: qsTr("Rename")
                                        accessibleName: qsTr("Rename %1").arg(spaceRow.spaceName)
                                        onClicked: root.spaceActionRequested("rename",
                                                                             spaceRow.spaceId,
                                                                             spaceRow.spaceName)
                                    }

                                    ActionButton {
                                        id: deleteSpace
                                        objectName: "deleteSpace-" + spaceRow.spaceId
                                        colors: root.colors
                                        label: qsTr("Delete")
                                        accessibleName: qsTr("Delete %1").arg(spaceRow.spaceName)
                                        destructive: true
                                        enabled: spaceList.count > 1
                                        onClicked: root.spaceActionRequested("delete",
                                                                             spaceRow.spaceId,
                                                                             spaceRow.spaceName)
                                    }

                                    ActionButton {
                                        id: forgetProject
                                        objectName: "forgetProject-" + spaceRow.spaceId
                                        colors: root.colors
                                        visible: spaceRow.project !== null
                                        label: qsTr("Forget project")
                                        accessibleName: qsTr("Forget the project of %1").arg(
                                                            spaceRow.spaceName)
                                        onClicked: root.browser.forgetSpaceProject(spaceRow.spaceId)
                                    }

                                    // Where the Space sits in the list, at the end of the row:
                                    // an arrow each way, the same pair the find bar steps its
                                    // matches with. A move is one step, so the row at either end
                                    // has nowhere to go that way and says so rather than
                                    // refusing on the click. The name is on the answer rather
                                    // than in it: an arrow reads as up, not as "move Work up".
                                    ChromeButton {
                                        id: moveSpaceUp
                                        objectName: "moveSpaceUp-" + spaceRow.spaceId
                                        width: 28
                                        height: 26
                                        icon: "keyboard_arrow_up"
                                        fontFamily: root.iconFontFamily
                                        foreground: root.colors.mutedText
                                        accent: root.colors.accent
                                        accessibleName: qsTr("Move %1 up").arg(spaceRow.spaceName)
                                        enabled: spaceRow.index > 0 && spaceRow.besideOwnKind(-1)
                                        onClicked: root.browser.moveSpaceBy(spaceRow.spaceId, -1)
                                    }

                                    ChromeButton {
                                        id: moveSpaceDown
                                        objectName: "moveSpaceDown-" + spaceRow.spaceId
                                        width: 28
                                        height: 26
                                        icon: "keyboard_arrow_down"
                                        fontFamily: root.iconFontFamily
                                        foreground: root.colors.mutedText
                                        accent: root.colors.accent
                                        accessibleName: qsTr("Move %1 down").arg(spaceRow.spaceName)
                                        enabled: spaceRow.index < spaceList.count - 1
                                                 && spaceRow.besideOwnKind(1)
                                        onClicked: root.browser.moveSpaceBy(spaceRow.spaceId, 1)
                                    }
                                }
                            }
                        }
                    }

                    Text {
                        width: pane.width
                        visible: root.browser ? root.browser.privateBrowsing : false
                        text: qsTr("Space actions are available in a regular window.")
                        color: root.colors.mutedText
                        font.family: Style.font.family
                        font.pixelSize: Style.font.body
                        wrapMode: Text.WordWrap
                    }
                }

                // ---- agents -------------------------------------------------

                Column {
                    width: pane.width
                    visible: root.section === 11
                    spacing: 0

                    // It opens the section, so it takes only the sliver a tall
                    // glyph paints above its box, as privacy's does.
                    SectionLabel {
                        id: agentsLabel
                        colors: root.colors
                        topPadding: agentsLabel.overshoot
                        text: qsTr("agents")
                    }

                    // The cost of an Agent tab is said here once, beside the
                    // switch that lets one exist, rather than on every mark.
                    SettingToggle {
                        objectName: "allowAgents"
                        visible: !!root.agentControl
                        width: pane.width
                        colors: root.colors
                        title: qsTr("Allow agents")
                        note: qsTr(
                                  "Lets a coding agent on this computer make Agent Spaces and read and act in their pages, and ask once for each of your Spaces it wants to use. An Agent tab stays rendered while an Agent is attached to it, which costs memory and GPU. Turning this off detaches every Agent.")
                        accessibleName: qsTr("Allow agents")
                        checked: !!root.agentControl && root.agentControl.allowAgents
                        onClicked: {
                            if (root.agentControl)
                                root.agentControl.allowAgents = !checked;
                        }
                    }

                    SettingRow {
                        objectName: "agentCommandRow"
                        visible: !!root.agentControl
                        width: pane.width
                        colors: root.colors
                        title: qsTr("Agent command")
                        note: qsTr(
                                  "What :ask runs in your terminal, with the tab on show and your "
                                  + "words added at the end. Write it as you would in a shell.")

                        SettingField {
                            objectName: "agentCommand"
                            width: Style.spacing.dropdownWidth
                            colors: root.colors
                            placeholder: "claude"
                            accessibleName: qsTr("Agent command")
                            text: root.agentControl ? root.agentControl.agentCommand : ""
                            onEditingFinished: {
                                if (root.agentControl)
                                    root.agentControl.agentCommand = text;
                            }
                        }
                    }

                    SectionLabel {
                        visible: !!root.agentControl
                        colors: root.colors
                        text: qsTr("granted spaces")
                    }

                    Text {
                        objectName: "noGrantedSpaces"
                        width: pane.width
                        visible: !!root.agentControl && grantedSpaceList.count === 0
                        topPadding: Style.spacing.md
                        text: qsTr(
                                  "No Space is granted. An Agent that wants one of yours asks first, over the page.")
                        color: root.colors.mutedText
                        wrapMode: Text.WordWrap
                        font.family: Style.font.family
                        font.pixelSize: Style.font.body
                    }

                    Repeater {
                        id: grantedSpaceList
                        model: root.agentControl ? root.agentControl.grantedSpaces : []

                        SettingRow {
                            required property var modelData
                            objectName: "grantedSpace-" + modelData.spaceId
                            width: pane.width
                            colors: root.colors
                            title: modelData.spaceName
                            note: qsTr("Agents may read and act in every tab of this Space.")

                            ActionButton {
                                objectName: "revokeGrant-" + modelData.spaceId
                                colors: root.colors
                                destructive: true
                                label: qsTr("Revoke")
                                accessibleName: qsTr("Revoke the grant to %1").arg(
                                                    modelData.spaceName)
                                onClicked: root.agentControl.revokeGrant(modelData.spaceId)
                            }
                        }
                    }

                    Text {
                        width: pane.width
                        visible: !root.agentControl
                        text: root.privateWindow ? qsTr(
                                                       "Agents are available in a regular window.") :
                                                   qsTr("This window has no Agent socket.")
                        color: root.colors.mutedText
                        font.family: Style.font.family
                        font.pixelSize: Style.font.body
                        wrapMode: Text.WordWrap
                    }
                }

                // ---- extensions --------------------------------------------

                Column {
                    width: pane.width
                    visible: root.section === 12
                    spacing: pane.spacing

                    Text {
                        text: qsTr("Known extensions")
                        color: root.colors.text
                        font.family: Style.font.family
                        font.pixelSize: Style.font.display
                    }

                    NoticeBox {
                        objectName: "extensionFailureNotice"
                        width: pane.width
                        visible: root.extensionFailure.length > 0
                        colors: root.colors
                        iconFontFamily: root.iconFontFamily
                        glyph: "extension_off"
                        title: qsTr("The extension was not installed")
                        detail: root.extensionFailure
                    }

                    NoticeBox {
                        objectName: "extensionsUnavailableNotice"
                        width: pane.width
                        visible: !root.knownExtensionsAvailable
                        colors: root.colors
                        iconFontFamily: root.iconFontFamily
                        glyph: "extension_off"
                        title: qsTr("This build cannot host an extension")
                        detail: qsTr(
                                    "Known extensions need the engine Omaweb builds for itself. This Omaweb runs the engine the system supplies.")
                    }

                    NoticeBox {
                        objectName: "extensionsPrivateNotice"
                        width: pane.width
                        visible: root.knownExtensionsAvailable && root.privateWindow
                        colors: root.colors
                        iconFontFamily: root.iconFontFamily
                        glyph: "extension_off"
                        title: qsTr("This window loads no extension")
                        detail: qsTr(
                                    "A Private window keeps nothing after it closes, and an extension's vault is something to keep. Turn one on from an ordinary window and it is on in every Space there.")
                    }

                    Text {
                        width: pane.width
                        visible: root.knownExtensionsAvailable && !root.privateWindow
                        wrapMode: Text.WordWrap
                        color: root.colors.mutedText
                        font.family: Style.font.family
                        font.pixelSize: Style.font.bodySmall
                        // The reader is told about the download before they ask
                        // for it, because it is the one request Omaweb makes to
                        // Google and they should not find out afterwards.
                        text: qsTr(
                                  "Omaweb names the extensions it has tested and loads no others. One that is on is on in every Space, and each Space keeps its own logins for it, so it is unlocked where it is used. A Private window loads none.\n\nTurning one on downloads it from the Chrome Web Store, which tells Google which extension you are installing. Omaweb accepts it only if the publisher signed it with the key this build carries, and asks once a day whether a newer one has been published.")
                    }

                    Column {
                        width: pane.width
                        spacing: 0
                        visible: root.knownExtensionsAvailable && !root.privateWindow

                        Repeater {
                            model: root.section === 12 ? root.knownExtensions : []

                            SettingToggle {
                                required property var modelData

                                objectName: "knownExtension-" + modelData.key
                                width: pane.width
                                colors: root.colors
                                title: modelData.name
                                // What the reader needs to judge it: who
                                // publishes it, under what terms, and whether
                                // the package is here yet.
                                note: modelData.publisher + " · " + modelData.licence + (
                                          modelData.fetching ? " · " + qsTr("downloading") : (
                                                                   modelData.installed ? "" : " · "
                                                                                         + qsTr("not downloaded yet")))
                                      + "\n" + modelData.summary
                                accessibleName: modelData.name
                                // Turning one on is what fetches it, so a
                                // package that is not here yet is not a reason
                                // to refuse the switch. Only a download already
                                // running is: pressing again would ask for the
                                // same folder twice.
                                enabled: !modelData.fetching
                                checked: modelData.enabled
                                onClicked: root.knownExtensionToggled(modelData.key,
                                                                      !modelData.enabled)
                            }
                        }
                    }
                }

                // ---- sync --------------------------------------------------

                Column {
                    width: pane.width
                    visible: root.section === 13
                    spacing: pane.spacing

                    Text {
                        text: qsTr("Sync")
                        color: root.colors.text
                        font.family: Style.font.family
                        font.pixelSize: Style.font.display
                    }

                    Omarchy.BorderSurface {
                        id: syncAccountCard
                        objectName: "syncAccountCard"
                        width: pane.width
                        visible: root.sync && root.sync.login.length > 0 && (!root.browser ||
                                                                             !root.browser.privateBrowsing)

                        implicitHeight: contentTopInset + Math.max(syncAccountAvatar.height,
                                                                   syncAccountDetails.implicitHeight)
                                        + contentBottomInset
                        height: implicitHeight
                        radius: Style.cornerRadius
                        padding: Style.spacing.huge
                        color: Qt.rgba(root.colors.text.r, root.colors.text.g, root.colors.text.b,
                                       0.04)
                        borderSpec: Border.controlSpec("normal", root.colors.text,
                                                       root.colors.accent)
                        Accessible.role: Accessible.StaticText
                        Accessible.name: root.sync ? qsTr("%1, %2 account. %3").arg(
                                                         root.sync.login).arg(
                                                         root.sync.provider).arg(root.sync.status) :
                                                     ""

                        Rectangle {
                            id: syncAccountAvatar
                            objectName: "syncAccountAvatar"
                            anchors.left: parent.left
                            anchors.leftMargin: syncAccountCard.contentLeftInset
                            anchors.verticalCenter: parent.verticalCenter
                            width: 64
                            height: width
                            radius: width / 2
                            color: root.colors.separator
                            clip: true
                            Accessible.ignored: true

                            Text {
                                objectName: "syncAccountMonogram"
                                anchors.centerIn: parent
                                visible: syncAccountImage.status !== Image.Ready
                                text: root.sync && root.sync.login.length > 0 ? root.sync.login.charAt(
                                                                                    0).toUpperCase(
                                                                                    ) : ""
                                color: root.sync && root.sync.enabled ? root.colors.text :
                                                                        root.colors.mutedText
                                font.family: Style.font.family
                                font.pixelSize: Style.font.display
                                font.bold: true
                            }

                            Image {
                                id: syncAccountImage
                                anchors.fill: parent
                                source: root.sync && root.sync.avatarPath.length > 0 ? "file://"
                                                                                       + root.sync.avatarPath :
                                                                                       ""
                                sourceSize.width: 128
                                sourceSize.height: 128
                                fillMode: Image.PreserveAspectCrop
                                visible: false
                                smooth: true
                            }

                            MultiEffect {
                                anchors.fill: parent
                                source: syncAccountImage
                                visible: syncAccountImage.status === Image.Ready
                                saturation: root.sync && root.sync.enabled ? 0 : -1
                            }
                        }

                        Column {
                            id: syncAccountDetails
                            anchors.left: syncAccountAvatar.right
                            anchors.leftMargin: Style.spacing.huge
                            anchors.right: parent.right
                            anchors.rightMargin: syncAccountCard.contentRightInset
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: Style.spacing.sm
                            Accessible.ignored: true

                            Text {
                                id: syncAccountLogin
                                objectName: "syncAccountLogin"
                                width: parent.width
                                text: root.sync ? root.sync.login : ""
                                color: root.colors.text
                                elide: Text.ElideRight
                                font.family: Style.font.family
                                font.pixelSize: Style.font.subtitle
                                font.bold: true
                            }

                            Text {
                                id: syncAccountProvider
                                objectName: "syncAccountProvider"
                                width: parent.width
                                text: root.sync && root.sync.provider.length > 0 ? qsTr(
                                                                                       "%1 account").arg(
                                                                                       root.sync.provider) :
                                                                                   qsTr("Sync account")
                                color: root.colors.mutedText
                                elide: Text.ElideRight
                                font.family: Style.font.family
                                font.pixelSize: Style.font.body
                            }

                            Text {
                                id: syncAccountState
                                objectName: "syncAccountState"
                                width: parent.width
                                text: root.sync ? root.sync.status : ""
                                color: root.sync && root.sync.errorMessage.length > 0
                                       ? root.colors.urgent : root.sync && root.sync.enabled
                                         ? root.colors.accent : root.colors.mutedText
                                wrapMode: Text.WordWrap
                                font.family: Style.font.family
                                font.pixelSize: Style.font.caption
                            }
                        }
                    }

                    SettingRow {
                        width: pane.width
                        visible: !root.sync || root.sync.login.length === 0 || !root.syncAvailable
                        colors: root.colors
                        title: !root.syncAvailable ? qsTr(
                                                         "Sync is unavailable in a Private window") :
                                                     root.sync ? (root.sync.login.length > 0 ? qsTr(
                                                                                                   "%1 on %2").arg(
                                                                                                   root.sync.login).arg(
                                                                                                   root.sync.provider) :
                                                                                               root.sync.status) :
                                                                 root.providerText.connectAction
                        note: root.sync ? root.sync.status : (root.syncLauncher
                                                              ? root.syncLauncher.errorMessage : "")
                    }

                    Text {
                        id: syncPrivacyBoundary
                        objectName: "syncPrivacyBoundary"
                        width: pane.width
                        text: qsTr(
                                  "Spaces and tabs are end-to-end encrypted. Approved settings, keybindings, and filter subscription addresses are readable in your private repository. Passwords, cookies, browsing history, downloads, site permissions, and every Private window are never synced. ")
                              + root.providerText.observationNote
                        color: root.colors.mutedText
                        wrapMode: Text.WordWrap
                        font.family: Style.font.family
                        font.pixelSize: Style.font.body
                    }

                    NoticeBox {
                        objectName: "syncErrorNotice"
                        width: pane.width
                        visible: root.sync && root.sync.errorMessage.length > 0
                        colors: root.colors
                        iconFontFamily: root.iconFontFamily
                        glyph: "sync_problem"
                        title: root.providerText.failureTitle
                        detail: root.sync ? root.sync.errorMessage : ""
                    }

                    SettingField {
                        id: syncRecoveryKey
                        objectName: "syncRecoveryKey"
                        width: pane.width
                        visible: root.syncAvailable && (!root.sync || root.sync.login.length === 0)
                        colors: root.colors
                        placeholder: qsTr("recovery key (leave empty on the first device)")
                        accessibleName: qsTr("Existing Sync recovery key")
                    }

                    ActionButton {
                        objectName: "connectSyncButton"
                        colors: root.colors
                        visible: root.syncAvailable && (!root.sync || (!root.sync.enabled
                                                                       && root.sync.login.length
                                                                       === 0 &&
                                                                       !root.sync.connecting))
                        label: !root.sync && root.syncLauncher && root.syncLauncher.configured
                               ? qsTr("Resume Sync") : root.providerText.connectAction
                        onClicked: {
                            if (root.syncLauncher && root.syncLauncher.load()
                                    && root.syncLauncher.controller) {
                                if (root.syncLauncher.controller.login.length > 0) {
                                    root.syncLauncher.controller.resume();
                                    return;
                                }
                                root.syncLauncher.controller.beginConnection(syncRecoveryKey.text);
                            }
                        }
                    }

                    SettingRow {
                        width: pane.width
                        visible: root.sync && root.sync.connecting &&
                                 !root.sync.awaitingRepositoryCreation &&
                                 !root.sync.awaitingInstallation
                        colors: root.colors
                        title: root.providerText.codePrompt
                        note: root.providerText.authorizationNote
                    }

                    Flow {
                        width: pane.width
                        spacing: Style.spacing.sm
                        visible: root.sync && root.sync.connecting &&
                                 !root.sync.awaitingRepositoryCreation &&
                                 !root.sync.awaitingInstallation

                        ActionButton {
                            objectName: "copySyncCodeButton"
                            colors: root.colors
                            label: qsTr("Copy code")
                            onClicked: root.copySyncCode()
                        }

                        ActionButton {
                            objectName: "openSyncAuthorizationButton"
                            colors: root.colors
                            label: root.providerText.authorizationAction
                            onClicked: root.openSyncConsent(root.sync.verificationUrl)
                        }
                    }

                    SettingRow {
                        width: pane.width
                        visible: root.sync && root.sync.awaitingRepositoryCreation
                        colors: root.colors
                        title: root.providerText.repositoryTitle
                        note: root.providerText.repositoryNote
                    }

                    ActionButton {
                        objectName: "continueSyncSetupButton"
                        colors: root.colors
                        visible: root.sync && root.sync.awaitingRepositoryCreation
                        label: qsTr("I created the repository")
                        onClicked: root.sync.continueConnection()
                    }

                    SettingRow {
                        width: pane.width
                        visible: root.sync && root.sync.awaitingInstallation
                        colors: root.colors
                        title: root.providerText.installationTitle
                        note: root.providerText.installationNote
                    }

                    SettingRow {
                        width: pane.width
                        visible: root.sync && root.sync.recoveryKey.length > 0
                        colors: root.colors
                        title: qsTr("Save this recovery key now")
                        note: root.sync ? root.sync.recoveryKey : ""
                    }

                    Flow {
                        width: pane.width
                        spacing: Style.spacing.sm
                        visible: root.sync && root.sync.recoveryKey.length > 0

                        ActionButton {
                            colors: root.colors
                            label: qsTr("Copy recovery key")
                            onClicked: SystemClipboard.copyText(root.sync.recoveryKey)
                        }
                        ActionButton {
                            colors: root.colors
                            label: qsTr("Save recovery key")
                            onClicked: recoveryKeySaveDialog.open()
                        }
                        ActionButton {
                            colors: root.colors
                            label: qsTr("I saved it")
                            onClicked: root.sync.clearRecoveryKey()
                        }
                    }

                    Flow {
                        width: pane.width
                        spacing: Style.spacing.sm
                        visible: root.sync && root.sync.login.length > 0

                        ActionButton {
                            colors: root.colors
                            label: qsTr("Sync now")
                            onClicked: root.sync.syncNow()
                        }
                        ActionButton {
                            colors: root.colors
                            label: qsTr("Pause")
                            visible: root.sync && root.sync.enabled
                            onClicked: root.sync.pause()
                        }
                        ActionButton {
                            colors: root.colors
                            label: qsTr("Resume")
                            visible: root.sync && !root.sync.enabled
                            onClicked: root.sync.resume()
                        }
                        ActionButton {
                            colors: root.colors
                            label: qsTr("Disconnect")
                            destructive: true
                            onClicked: root.sync.disconnectProvider()
                        }
                    }
                }

                // ---- about -------------------------------------------------

                Column {
                    width: pane.width
                    visible: root.section === 14
                    spacing: pane.spacing

                    Text {
                        objectName: "aboutName"
                        text: qsTr("Omaweb")
                        color: root.colors.text
                        font.family: Style.font.family
                        font.pixelSize: Style.font.display
                    }

                    // The version carries the git description, so a build made
                    // past a tag reads 0.2.0-14-gabc1234 and a bug report names
                    // the commit it came from.
                    Text {
                        objectName: "aboutVersion"
                        text: qsTr("Version %1").arg(Qt.application.version)
                        color: root.colors.mutedText
                        font.family: Style.font.family
                        font.pixelSize: Style.font.body
                    }

                    // The stage, and where the specifics are. What this build
                    // verifies and what it leaves to the reader is the Security
                    // section's to state, row by row, rather than a caption's
                    // to summarise into advice about what to browse.
                    Text {
                        width: pane.width
                        text: qsTr(
                                  "Alpha. Expect bugs. The Security section states what this build verifies and what it leaves to you.")
                        color: root.colors.mutedText
                        wrapMode: Text.WordWrap
                        font.family: Style.font.family
                        font.pixelSize: Style.font.caption
                    }

                    // What the daily check found. It is stated here as well as
                    // marked in the outline footer, because About is where a
                    // reader comes to ask how old their browser is.
                    Text {
                        objectName: "aboutNewerRelease"
                        visible: !!root.releaseWatch && root.releaseWatch.announcing
                        width: pane.width
                        text: root.releaseWatch && root.releaseWatch.announcing ? qsTr(
                                                                                      "Omaweb %1 is out. %2").arg(
                                                                                      root.releaseWatch.release).arg(
                                                                                      root.releaseWatch.instruction) :
                                                                                  ""
                        color: root.colors.text
                        wrapMode: Text.WordWrap
                        font.family: Style.font.family
                        font.pixelSize: Style.font.body
                    }

                    SettingToggle {
                        objectName: "checkForReleases"
                        visible: !!root.releaseWatch
                        width: pane.width
                        colors: root.colors
                        title: qsTr("Check for new releases")
                        note: qsTr(
                                  "Asks GitHub once a day what the newest release is, and sends nothing about this machine.")
                        accessibleName: qsTr("Check for new releases")
                        checked: !!root.releaseWatch && root.releaseWatch.checkEnabled
                        onClicked: {
                            if (root.releaseWatch)
                                root.releaseWatch.checkEnabled = !checked;
                        }
                    }

                    // Being the default browser is the desktop's setting, so it
                    // is offered here and never taken on a first run. A desktop
                    // with no way to ask does not get an offer that would fail.
                    SectionLabel {
                        visible: DefaultBrowser.available
                        colors: root.colors
                        text: qsTr("default browser")
                    }

                    Text {
                        objectName: "defaultBrowserState"
                        visible: DefaultBrowser.available
                        width: pane.width
                        text: DefaultBrowser.isDefault ? qsTr(
                                                             "Omaweb opens links from other applications.") :
                                                         qsTr("Another browser opens links from other applications.")
                        color: root.colors.mutedText
                        wrapMode: Text.WordWrap
                        font.family: Style.font.family
                        font.pixelSize: Style.font.body
                    }

                    ActionButton {
                        objectName: "makeDefaultBrowserButton"
                        visible: DefaultBrowser.available && !DefaultBrowser.isDefault
                        colors: root.colors
                        label: qsTr("Make Omaweb the default")
                        accessibleName: qsTr("Make Omaweb the default browser")
                        onClicked: DefaultBrowser.makeDefault()
                    }

                    SectionLabel {
                        colors: root.colors
                        text: qsTr("project", "section label: the Omaweb project")
                    }

                    Text {
                        objectName: "aboutLinks"
                        width: pane.width
                        textFormat: Text.StyledText
                        text: '<a href="https://omaweb.app">omaweb.app</a> · '
                              + '<a href="https://github.com/villekivela/omaweb">' + qsTr("source",
                                                                                          "link to the source code")
                              + '</a> · <a href="https://github.com/villekivela/omaweb/issues">'
                              + qsTr("issues", "link to the issue tracker") + '</a>'
                        linkColor: root.colors.accent
                        color: root.colors.mutedText
                        wrapMode: Text.WordWrap
                        font.family: Style.font.family
                        font.pixelSize: Style.font.body
                        onLinkActivated: function (link) {
                            Qt.openUrlExternally(link);
                        }

                        HoverHandler {
                            cursorShape: parent.hoveredLink !== "" ? Qt.PointingHandCursor :
                                                                     Qt.ArrowCursor
                        }
                    }

                    SectionLabel {
                        colors: root.colors
                        text: qsTr("license")
                    }

                    Text {
                        width: pane.width
                        text: qsTr(
                                  "Omaweb is under the Mozilla Public License 2.0. It bundles third-party components under their own licenses, which THIRD_PARTY_NOTICES.md in the source tree lists.")
                        color: root.colors.mutedText
                        wrapMode: Text.WordWrap
                        font.family: Style.font.family
                        font.pixelSize: Style.font.caption
                    }
                }
            }
        }
    }

    ClearBrowsingDataDialog {
        id: clearDataDialog
        objectName: "clearBrowsingDataDialog"
        anchors.fill: parent
        z: 10
        colors: root.colors
        open: root.open && root.clearDataOpen
        spaceName: root.browser ? root.browser.activeSpaceName : ""
        categories: root.clearCategories
        range: root.clearRange

        onCategoryToggled: function (value) {
            root.toggleClearCategory(value);
        }
        onRangeChosen: function (value) {
            root.chooseClearRange(value);
        }
        onDismissed: root.clearDataOpen = false
        onConfirmed: function (categories, since, everySpace, confirmation) {
            root.clearDataOpen = false;
            if (root.browser.clearBrowsingData(categories, since, everySpace, confirmation))
                root.refresh();
        }
    }
}
