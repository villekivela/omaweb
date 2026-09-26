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
                                                                                  "name": "Sync",
                                                                                  "connectAction":
                                                                                  "Connect Sync",
                                                                                  "authorizationAction":
                                                                                  "Open the authorization page",
                                                                                  "failureTitle":
                                                                                  "Sync failed",
                                                                                  "codeCopiedNotice":
                                                                                  "Code copied",
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
    property bool easeChrome: true
    property bool glanceEnabled: true
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
    property var clearCategories: ["cookies", "storage", "cache", "permissions", "history"]
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

    readonly property var sections: ["tabs", "interface", "keyboard", "content blocking", "network",
        "downloads", "search", "privacy", "spaces", "extensions", "sync", "about"]

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
            if (root.sections[index].charAt(0).toLowerCase() !== letter)
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
                                                                   root.sections[index])));


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
                                                         title: "list name",
                                                         license: "license",
                                                         source: "source page",
                                                         update: "update address"
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

    // Bytes as the reader reads them. A retained tab whose renderer the
    // platform cannot account for says so rather than claiming nothing.
    function resourceLabel(bytes) {
        if (!(bytes > 0))
            return "size unavailable";
        const megabytes = bytes / (1024 * 1024);
        return (megabytes >= 100 ? Math.round(megabytes) : Math.round(megabytes * 10) / 10) + " MB";
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
    signal easeChromeToggled(bool enabled)
    signal glanceToggled(bool enabled)

    Dialogs.FileDialog {
        id: recoveryKeySaveDialog
        title: "Save Sync recovery key"
        fileMode: Dialogs.FileDialog.SaveFile
        nameFilters: ["Text files (*.txt)"]
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

    function refresh() {
        if (!root.browser)
            return;
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
                                              "cookies,storage,cache,permissions,history");
        root.clearCategories = saved.length > 0 ? saved.split(",") : [];
        root.clearRange = root.browser.preference("clear-data-range", "86400000");
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
            root.browser.setPreference("clear-data-categories", next.join(","));
    }

    function chooseClearRange(value) {
        root.clearRange = value;
        if (root.browser)
            root.browser.setPreference("clear-data-range", value);
    }

    function makeDefaultSearchEngine(id) {
        if (root.browser.setDefaultSearchEngine(id))
            root.refresh();
    }

    function deleteSearchEngine(id) {
        if (root.browser.deleteSearchEngine(id))
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
            text: "browsing · esc closes"
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
            anchors.left: parent.left
            anchors.top: settingsEyebrow.bottom
            anchors.topMargin: Style.spacing.md
            text: "Settings"
            color: root.colors.text
            font.family: Style.font.family
            font.pixelSize: Style.font.display
            Accessible.role: Accessible.Heading
            Accessible.name: "Settings"
        }

        ChromeButton {
            objectName: "closeSettingsButton"
            anchors.right: parent.right
            anchors.verticalCenter: settingsHeading.verticalCenter
            width: root.closeSize
            height: root.closeSize
            icon: "close"
            accessibleName: "Close settings"
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
                    text: modelData
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
                    Accessible.name: modelData
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
                        title: "Use site favicons"
                        note: "When off, tabs show a two-letter tile in the favicon's colour."
                        accessibleName: "Use site favicons"
                        checked: root.useFavicons
                        onClicked: root.useFaviconsToggled(!checked)
                    }

                    SettingToggle {
                        objectName: "tintFavicons"
                        width: pane.width
                        colors: root.colors
                        title: "Tint favicons"
                        note: "Recolor site artwork to match the sidebar palette."
                        accessibleName: "Tint favicons"
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
                            text: "kept active"
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
                                note: modelData.spaceName + " · " + (modelData.inspected
                                                                     ? "Developer tools" :
                                                                       "Keep active") + " · " + (
                                          modelData.running ? root.resourceLabel(
                                                                  modelData.residentBytes) :
                                                              "not running")

                                ActionButton {
                                    visible: !modelData.inspected
                                    colors: root.colors
                                    label: "stop"
                                    accessibleName: "Stop keeping " + modelData.title + " active"
                                    onClicked: root.retainedTabReleased(modelData.tabId)
                                }
                            }
                        }

                        SettingRow {
                            width: pane.width
                            visible: root.retainedTabs.length === 0
                            colors: root.colors
                            title: "Nothing is kept active"
                            note: "A Pinned tab set to Keep active, or a tab with Developer tools attached, keeps running while its Space is inactive and is listed here with what it costs."
                        }
                    }
                }

                // ---- interface ---------------------------------------------

                Column {
                    width: pane.width
                    visible: root.section === 1
                    spacing: pane.spacing

                    SettingToggle {
                        objectName: "floatingControls"
                        width: pane.width
                        colors: root.colors
                        title: "Floating controls"
                        note: "With the sidebar hidden, keep the navigation controls over the page. Pause at the left edge to peek at the sidebar; it hides when the pointer leaves."
                        accessibleName: "Floating controls"
                        checked: root.floatingControls
                        onClicked: root.floatingControlsToggled(!checked)
                    }

                    SettingToggle {
                        objectName: "easeChrome"
                        width: pane.width
                        colors: root.colors
                        title: "Ease the chrome"
                        note: "Slide the sidebar, the outline, the page and every panel and sheet into place as they open, switch and close. When off, everything arrives at once."
                        accessibleName: "Ease the chrome"
                        checked: root.easeChrome
                        onClicked: root.easeChromeToggled(!checked)
                    }

                    SettingToggle {
                        objectName: "glanceEnabled"
                        width: pane.width
                        colors: root.colors
                        title: "Glance at a page's new tabs"
                        note: "A link that asks for a new tab opens over the page instead, for a look. Escape closes it; one command keeps it as a tab. When off, the link opens a tab."
                        accessibleName: "Glance at a page's new tabs"
                        checked: root.glanceEnabled
                        onClicked: root.glanceToggled(!checked)
                    }

                    SectionLabel {
                        visible: !!root.fontSettings
                        colors: root.colors
                        text: "type"
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
                        title: "Interface font size"
                        note: "The size Omaweb's own type is drawn at; the theme's is " + (
                                  root.fontSettings ? root.fontSettings.themeFontSize : 0)
                              + "px. Pages and their zoom are not changed by it."

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
                            accessibleName: "Interface font size"
                            defaultName: "the theme's"
                            onIncreased: root.fontSettings.increaseInterfaceFontSize()
                            onDecreased: root.fontSettings.decreaseInterfaceFontSize()
                            onReset: root.fontSettings.resetInterfaceFontSize()
                        }
                    }

                    SectionLabel {
                        visible: !!root.fontSettings
                        colors: root.colors
                        text: "page fonts"
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
                        title: "This build cannot reach the engine's fonts"
                        detail: "Omaweb was built against another Qt than the one it is running "
                                + "on, so pages are drawn in the engine's own fonts. A rebuild "
                                + "against this Qt brings the controls back."
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
                            const engineLabel = engineFamily.length > 0 ? "Engine default ("
                                                                          + engineFamily + ")" :
                                                                          "Engine default";
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
                                                 label: "Interface's (" + Style.font.family + ")"
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
                            title: "Standard font"
                            note: "A page that names no font is drawn in this one."

                            SettingDropdown {
                                objectName: "pageStandardFamily"
                                width: pageFontsGroup.familyControlWidth(options)
                                colors: root.colors
                                options: pageFontsGroup.familyOptions(
                                             root.pageFontsMap.standardFamily
                                             ? root.pageFontsMap.standardFamily.engine : "", false)
                                value: pageFontsGroup.chosenFamily(root.pageFontsMap.standardFamily)
                                accessibleName: "Standard font for pages"
                                onChanged: function (family) {
                                    root.fontSettings.setPageFamily(FontSettings.Standard, family);
                                }
                            }
                        }

                        SettingRow {
                            width: pane.width
                            colors: root.colors
                            title: "Fixed-width font"
                            note: "Code on a page, and any text a page asks to have drawn "
                                  + "monospaced."

                            SettingDropdown {
                                objectName: "pageFixedFamily"
                                width: pageFontsGroup.familyControlWidth(options)
                                colors: root.colors
                                options: pageFontsGroup.familyOptions(root.pageFontsMap.fixedFamily
                                                                      ? root.pageFontsMap.fixedFamily.engine :
                                                                        "", true)
                                value: pageFontsGroup.chosenFamily(root.pageFontsMap.fixedFamily)
                                accessibleName: "Fixed-width font for pages"
                                onChanged: function (family) {
                                    root.fontSettings.setPageFamily(FontSettings.Fixed, family);
                                }
                            }
                        }

                        SettingRow {
                            width: pane.width
                            colors: root.colors
                            title: "Font size"
                            note: "The size a page that names none is read at. Code follows it "
                                  + "a step smaller, as the engine keeps it."

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
                                accessibleName: "Page font size"
                                defaultName: "the engine's"
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
                            title: "Minimum font size"
                            note: "No text on a page is drawn smaller than this, whatever size "
                                  + "the page asks for. A tab's zoom multiplies it. A page "
                                  + "already open takes it when it next lays out; reload to "
                                  + "see it now."

                            SettingStepper {
                                objectName: "pageMinimumFontSize"
                                colors: root.colors
                                value: root.pageFontsMap.minimumFontSize
                                       ? root.pageFontsMap.minimumFontSize.value : 0
                                minimum: 0
                                maximum: root.fontSettings
                                         ? root.fontSettings.maximumPageMinimumFontSize : 0
                                zeroLabel: "none"
                                overridden: !!root.pageFontsMap.minimumFontSize
                                            && root.pageFontsMap.minimumFontSize.overridden
                                accessibleName: "Minimum page font size"
                                defaultName: "the engine's"
                                onIncreased: root.fontSettings.setPageSize(FontSettings.Minimum,
                                                                           value + 1)
                                onDecreased: root.fontSettings.setPageSize(FontSettings.Minimum,
                                                                           value - 1)
                                onReset: root.fontSettings.setPageSize(FontSettings.Minimum, 0)
                            }
                        }
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
                        title: "Keyboard navigation"
                        note: "Omaweb's own command layer. It gives the same commands with every "
                              + "engine, and lets sites receive the keys they need."
                        accessibleName: "Enable Keyboard navigation"
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
                        title: "Some bindings were ignored"
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
                        title: "This desktop's input method is not installed"
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
                        title: "Block requests on this site"
                        note: root.refusals.sentence + (root.activeHost.length > 0
                                                        ? ", and the switch covers every page on "
                                                          + root.activeHost : "") + "."
                        accessibleName: "Enable content blocking for this site"
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
                            note: modelData.updateStatus + " · " + modelData.license + "\nSource "
                                  + modelData.source + "\nUpdates from " + modelData.updateAddress
                            accessibleName: "Enable " + modelData.title
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
                            text: "No filter lists. Network rules only block what the user rules "
                                  + "below say to block."
                            color: root.colors.mutedText
                            wrapMode: Text.WordWrap
                            font.family: Style.font.family
                            font.pixelSize: Style.font.body
                        }

                        ActionButton {
                            objectName: "restoreDefaultListsButton"
                            colors: root.colors
                            label: "Add EasyList and EasyPrivacy"
                            accessibleName: "Add the default filter lists"
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
                                note: "Not subscribed · " + modelData.license + "\nSource "
                                      + modelData.source + "\nUpdates from "
                                      + modelData.updateAddress

                                ActionButton {
                                    objectName: "subscribeKnownListButton"
                                    colors: root.colors
                                    label: "Subscribe"
                                    accessibleName: "Subscribe to " + modelData.title
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
                        text: "add a list"
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
                            accessibleName: "Subscription name"
                        }

                        SettingField {
                            id: subscriptionLicense
                            width: subscriptionFields.fieldWidth
                            colors: root.colors
                            placeholder: root.subscriptionPlaceholders.license
                            accessibleName: "Subscription license"
                        }

                        SettingField {
                            id: subscriptionSource
                            width: subscriptionFields.fieldWidth
                            colors: root.colors
                            placeholder: root.subscriptionPlaceholders.source
                            accessibleName: "Subscription source page"
                        }

                        SettingField {
                            id: subscriptionUpdate
                            width: subscriptionFields.fieldWidth
                            colors: root.colors
                            placeholder: root.subscriptionPlaceholders.update
                            accessibleName: "Subscription update address"
                        }
                    }

                    ActionButton {
                        objectName: "addSubscriptionButton"
                        colors: root.colors
                        label: "Add subscription"
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
                        text: "user rules"
                    }

                    MultilineField {
                        id: userRules
                        objectName: "userRulesInput"
                        width: pane.width
                        colors: root.colors
                        placeholder: "one rule per line"
                        accessibleName: "User rules"
                    }

                    Text {
                        objectName: "contentBlockingSupport"
                        width: pane.width
                        text: "Network rules, plain CSS cosmetic rules, parameter "
                              + "stripping, and the scriptlets and substitute resources "
                              + "in the bundled uBlock Origin library are supported; the "
                              + "scriptlets uBlock Origin gates behind trust are refused. " + (
                                  root.proceduralCosmeticFilteringAvailable
                                  ? "Procedural cosmetic rules written for a site are "
                                    + "applied, except on the Ladybird engine. " :
                                    "Procedural cosmetic rules are not applied: the Ladybird "
                                    + "engine lacks them. ") + (root.cnameUncloakingAvailable
                                                                ? "Trackers behind a CNAME are refused, but a $cname rule "
                                                                  + "that turns that off is not honoured. Response "
                                                                  + "rewriting, content security policies, HTML filtering "
                                                                  + "and dynamic rules are not supported." :
                                                                  "Response rewriting, content security policies, HTML "
                                                                  + "filtering, dynamic rules, $cname rules and CNAME "
                                                                  + "uncloaking are not supported; CNAME uncloaking needs "
                                                                  + "Omaweb's own build of the Qt engine.")
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
                        text: "Unsupported rules in the active lists: " + JSON.stringify(
                                  root.blocker ? root.blocker.compilationReport.unsupported : {})
                        color: root.colors.mutedText
                        wrapMode: Text.WordWrap
                        font.family: Style.font.family
                        font.pixelSize: Style.font.caption
                    }

                    ActionButton {
                        objectName: "saveUserRulesButton"
                        colors: root.colors
                        label: root.blocker && root.blocker.compiling ? "Compiling…" :
                                                                        "Save user rules"
                        accessibleName: "Save user rules"
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

                    SettingRow {
                        width: pane.width
                        colors: root.colors
                        separated: false
                        title: "Remote search suggestions"
                        note: "Typing in the Omnibar never leaves the machine. Suggestions come "
                              + "from this Space's own history."

                        Text {
                            objectName: "remoteSuggestionsStatus"
                            text: "Remote search suggestions: Off"
                            color: root.colors.mutedText
                            font.family: Style.font.family
                            font.pixelSize: Style.font.body
                        }
                    }

                    SettingRow {
                        width: pane.width
                        colors: root.colors
                        title: "Filter-list updates"

                        Text {
                            objectName: "automaticRequestsStatus"
                            width: Math.min(root.noteMeasure, pane.width / 2)
                            text: "Enabled filter-list subscriptions make automatic network requests "
                                  + "to their displayed update address when Omaweb starts. Remote search "
                                  + "suggestions remain off."
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
                        title: "Download directory"
                        note: root.browser ? root.browser.downloadDirectory : ""

                        ActionButton {
                            objectName: "chooseDownloadDirectory"
                            colors: root.colors
                            label: "Change"
                            accessibleName: "Change the download directory"
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
                                let line = downloadState;
                                if (downloadState === "in-progress" && total > 0)
                                    line += " · " + Math.floor(received * 100 / total) + "%";
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
                                    label: "Cancel"
                                    accessibleName: "Cancel this download"
                                    visible: String(downloadRow.model.state) === "in-progress"
                                             && String(downloadRow.model.runtimeId || "").length > 0
                                    onClicked: root.downloadCancelled(downloadRow.index)
                                }

                                ActionButton {
                                    objectName: "retryDownload-" + index
                                    colors: root.colors
                                    label: "Retry"
                                    accessibleName: "Retry this download"
                                    visible: String(downloadRow.model.state) === "interrupted"
                                    onClicked: root.downloadRetried(downloadRow.index)
                                }

                                ActionButton {
                                    objectName: "revealDownload-" + index
                                    colors: root.colors
                                    label: "Show"
                                    accessibleName: "Show where this download landed"
                                    visible: String(downloadRow.model.state) === "completed"
                                    onClicked: root.downloadRevealed(String(downloadRow.model.path))
                                }

                                ActionButton {
                                    objectName: "forgetDownload-" + index
                                    colors: root.colors
                                    label: "Remove"
                                    accessibleName: "Remove this download from the history"
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
                        title: "No recorded downloads"
                        note: "Downloads Omaweb has recorded in this Space appear here."
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
                                title: modelData.name + (modelData.default ? " · default" : "")
                                note: modelData.queryUrl

                                Row {
                                    spacing: Style.spacing.lg

                                    Text {
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: modelData.keyword.length > 0 ? "keyword "
                                                                             + modelData.keyword :
                                                                             "no keyword"
                                        color: root.colors.mutedText
                                        font.family: Style.font.family
                                        font.pixelSize: Style.font.caption
                                    }

                                    ActionButton {
                                        colors: root.colors
                                        label: "Default"
                                        visible: !modelData.default
                                        onClicked: root.makeDefaultSearchEngine(modelData.id)
                                    }

                                    ActionButton {
                                        colors: root.colors
                                        label: "Delete"
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
                        text: "add a provider"
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
                            accessibleName: "Predefined search provider"
                        }

                        ActionButton {
                            id: addProvider
                            objectName: "addSearchProviderButton"
                            colors: root.colors
                            label: root.searchEngineInstalled(providerPreset.value) ? "Added" :
                                                                                      "Add"
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
                        text: "add a custom engine"
                    }

                    SettingField {
                        id: engineName
                        width: pane.width
                        colors: root.colors
                        placeholder: "name"
                        accessibleName: "Search engine name"
                    }

                    SettingField {
                        id: engineQueryUrl
                        width: pane.width
                        colors: root.colors
                        placeholder: "query URL with {query}"
                        accessibleName: "Search engine query URL"
                    }

                    SettingField {
                        id: engineKeyword
                        width: pane.width
                        colors: root.colors
                        placeholder: "optional keyword"
                        accessibleName: "Search engine keyword"
                    }

                    ActionButton {
                        colors: root.colors
                        label: "Add and make default"
                        enabled: engineName.text.trim().length > 0 && engineQueryUrl.text.indexOf(
                                     "{query}") >= 0
                        onClicked: {
                            if (root.browser.addSearchEngine(engineName.text, engineQueryUrl.text,
                                                             engineKeyword.text)) {
                                engineName.text = "";
                                engineQueryUrl.text = "";
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
                        text: "privacy"
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
                        title: "Browsing data"
                        note: "Cookies, site storage, cache, site permissions and history — "
                              + "for the Spaces and the time range chosen when clearing."

                        ActionButton {
                            objectName: "clearBrowsingDataButton"
                            colors: root.colors
                            destructive: true
                            label: "Clear browsing data…"
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
                        title: "Global Privacy Control"
                        note: "Tells every site not to sell or share your data, with the Sec-GPC "
                              + "header on every request and navigator.globalPrivacyControl on "
                              + "every page. Sites bound by the CCPA and similar laws must honour it."
                        accessibleName: "Global Privacy Control"
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
                        title: "HTTPS-only mode"
                        note: "Sends every page's own address over HTTPS, whoever wrote the link. "
                              + "Where a site cannot be reached that way, Omaweb asks before "
                              + "loading it over plain HTTP. Local development addresses are left "
                              + "alone."
                        accessibleName: "HTTPS-only mode"
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
                        title: "Secure DNS"
                        note: "Looks up the sites you visit over an encrypted connection to a "
                              + "resolver you choose, instead of your system's. That resolver "
                              + "learns every site you visit. A network that blocks it stops "
                              + "pages loading, so turn it off to sign in to a hotel or airport "
                              + "network."

                        Column {
                            width: Style.spacing.dropdownWidth
                            spacing: Style.spacing.sm

                            SettingDropdown {
                                objectName: "secureDnsResolver"
                                colors: root.colors
                                accessibleName: "Secure DNS resolver"
                                options: {
                                    const options = [
                                              {
                                                  value: "",
                                                  label: "Off"
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
                                                     label: "An address you type"
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
                                placeholder: "https://dns.example/dns-query"
                                accessibleName: "Secure DNS address"
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
                                text: "That is not an https: address, so names would not be "
                                      + "encrypted. Nothing was changed."
                                color: root.colors.urgent
                                wrapMode: Text.WordWrap
                                font.family: Style.font.family
                                font.pixelSize: Style.font.caption
                            }

                            Text {
                                objectName: "secureDnsInUse"
                                width: parent.width
                                text: !root.secureDns || root.secureDns.resolver === ""
                                      ? "Names are looked up by your system's resolver." :
                                        root.engineSecureDns && !root.engineSecureDns.applied
                                        ? "The engine would not take this resolver, so names are "
                                          + "looked up by your system's resolver." :
                                          "Names are looked up by " + root.secureDns.resolverTitle
                                          + ", over an encrypted connection."
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
                        title: "Keep calls off your other addresses"
                        note: "A page setting up a call learns only the address of the "
                              + "connection it leaves on, not your local network's or the one a "
                              + "VPN hides. Calls still connect. Turn off to reach a peer on your "
                              + "own network directly."
                        accessibleName: "Keep calls off your other addresses"
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
                        title: "This build cannot keep calls off your other addresses"
                        detail: "Omaweb was built against another Qt than the one it is running "
                                + "on, so a page setting up a call is offered every address this "
                                + "machine has, the engine's own default. A rebuild against this "
                                + "Qt brings the setting back."
                    }

                    SectionLabel {
                        colors: root.colors
                        text: "process isolation"
                    }

                    SettingRow {
                        objectName: "rendererIsolationRow"
                        width: pane.width
                        colors: root.colors
                        title: RuntimeSecurity.rendererIsolated ? "Renderer isolation verified" :
                                                                  "Renderer isolation unverified"
                        note: RuntimeSecurity.rendererIsolation
                    }

                    SettingRow {
                        objectName: "networkServiceRow"
                        width: pane.width
                        colors: root.colors
                        title: "Network service in the browser process"
                        note: RuntimeSecurity.networkService
                    }

                    SettingRow {
                        objectName: "securityBaselineRow"
                        width: pane.width
                        colors: root.colors
                        title: RuntimeSecurity.meetsSecurityBaseline
                               ? "Engine at the approved security baseline" :
                                 "Unsupported preview: engine below the approved baseline"
                        note: RuntimeSecurity.securityBaseline
                    }
                }

                // ---- spaces -------------------------------------------------

                Column {
                    width: pane.width
                    visible: root.section === 8
                    spacing: pane.spacing

                    ActionButton {
                        objectName: "newSpaceButton"
                        colors: root.colors
                        label: "New Space"
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
                                required property bool active
                                objectName: "settingsSpace-" + spaceId
                                width: pane.width
                                colors: root.colors
                                title: spaceName
                                note: active ? "Current Space" : ""
                                height: Math.max(implicitHeight, spaceActions.implicitHeight
                                                 + verticalPadding * 2)

                                Flow {
                                    id: spaceActions
                                    width: Math.min(pane.width * 0.7, renameSpace.implicitWidth
                                                    + deleteSpace.implicitWidth + moveSpaceUp.width
                                                    + moveSpaceDown.width + spacing * 3)
                                    spacing: Style.spacing.sm

                                    ActionButton {
                                        id: renameSpace
                                        objectName: "renameSpace-" + spaceRow.spaceId
                                        colors: root.colors
                                        label: "Rename"
                                        accessibleName: "Rename " + spaceRow.spaceName
                                        onClicked: root.spaceActionRequested("rename",
                                                                             spaceRow.spaceId,
                                                                             spaceRow.spaceName)
                                    }

                                    ActionButton {
                                        id: deleteSpace
                                        objectName: "deleteSpace-" + spaceRow.spaceId
                                        colors: root.colors
                                        label: "Delete"
                                        accessibleName: "Delete " + spaceRow.spaceName
                                        destructive: true
                                        enabled: spaceList.count > 1
                                        onClicked: root.spaceActionRequested("delete",
                                                                             spaceRow.spaceId,
                                                                             spaceRow.spaceName)
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
                                        accessibleName: "Move " + spaceRow.spaceName + " up"
                                        enabled: spaceRow.index > 0
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
                                        accessibleName: "Move " + spaceRow.spaceName + " down"
                                        enabled: spaceRow.index < spaceList.count - 1
                                        onClicked: root.browser.moveSpaceBy(spaceRow.spaceId, 1)
                                    }
                                }
                            }
                        }
                    }

                    Text {
                        width: pane.width
                        visible: root.browser ? root.browser.privateBrowsing : false
                        text: "Space actions are available in a regular window."
                        color: root.colors.mutedText
                        font.family: Style.font.family
                        font.pixelSize: Style.font.body
                        wrapMode: Text.WordWrap
                    }
                }

                // ---- extensions --------------------------------------------

                Column {
                    width: pane.width
                    visible: root.section === 9
                    spacing: pane.spacing

                    Text {
                        text: "Known extensions"
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
                        title: "The extension was not installed"
                        detail: root.extensionFailure
                    }

                    NoticeBox {
                        objectName: "extensionsUnavailableNotice"
                        width: pane.width
                        visible: !root.knownExtensionsAvailable
                        colors: root.colors
                        iconFontFamily: root.iconFontFamily
                        glyph: "extension_off"
                        title: "This build cannot host an extension"
                        detail: "Known extensions need the engine Omaweb builds for itself. "
                                + "This Omaweb runs the engine the system supplies."
                    }

                    NoticeBox {
                        objectName: "extensionsPrivateNotice"
                        width: pane.width
                        visible: root.knownExtensionsAvailable && root.privateWindow
                        colors: root.colors
                        iconFontFamily: root.iconFontFamily
                        glyph: "extension_off"
                        title: "This window loads no extension"
                        detail: "A Private window keeps nothing after it closes, and an "
                                + "extension's vault is something to keep. Turn one on from an "
                                + "ordinary window and it is on in every Space there."
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
                        text: "Omaweb names the extensions it has tested and loads no others. "
                              + "One that is on is on in every Space, and each Space keeps its "
                              + "own logins for it, so it is unlocked where it is used. A "
                              + "Private window loads none.\n\nTurning one on downloads it from "
                              + "the Chrome Web Store, which tells Google which extension you "
                              + "are installing. Omaweb accepts it only if the publisher signed "
                              + "it with the key this build carries, and asks once a day whether "
                              + "a newer one has been published."
                    }

                    Column {
                        width: pane.width
                        spacing: 0
                        visible: root.knownExtensionsAvailable && !root.privateWindow

                        Repeater {
                            model: root.section === 9 ? root.knownExtensions : []

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
                                          modelData.fetching ? " · downloading" : (
                                                                   modelData.installed ? "" :
                                                                                         " · not downloaded yet"))
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
                    visible: root.section === 10
                    spacing: pane.spacing

                    Text {
                        text: "Sync"
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
                        Accessible.name: root.sync ? root.sync.login + ", " + root.sync.provider
                                                     + " account. " + root.sync.status : ""

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
                                text: root.sync && root.sync.provider.length > 0
                                      ? root.sync.provider + " account" : "Sync account"
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
                        title: !root.syncAvailable ? "Sync is unavailable in a Private window" :
                                                     root.sync ? (root.sync.login.length > 0
                                                                  ? root.sync.login + " on "
                                                                    + root.sync.provider :
                                                                    root.sync.status) :
                                                                 root.providerText.connectAction
                        note: root.sync ? root.sync.status : (root.syncLauncher
                                                              ? root.syncLauncher.errorMessage : "")
                    }

                    Text {
                        id: syncPrivacyBoundary
                        objectName: "syncPrivacyBoundary"
                        width: pane.width
                        text: "Spaces and tabs are end-to-end encrypted. Approved settings, keybindings, and filter subscription addresses are readable in your private repository. Passwords, cookies, browsing history, downloads, site permissions, and every Private window are never synced. "
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
                        placeholder: "recovery key (leave empty on the first device)"
                        accessibleName: "Existing Sync recovery key"
                    }

                    ActionButton {
                        objectName: "connectSyncButton"
                        colors: root.colors
                        visible: root.syncAvailable && (!root.sync || (!root.sync.enabled
                                                                       && root.sync.login.length
                                                                       === 0 &&
                                                                       !root.sync.connecting))
                        label: !root.sync && root.syncLauncher && root.syncLauncher.configured
                               ? "Resume Sync" : root.providerText.connectAction
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
                            label: "Copy code"
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
                        label: "I created the repository"
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
                        title: "Save this recovery key now"
                        note: root.sync ? root.sync.recoveryKey : ""
                    }

                    Flow {
                        width: pane.width
                        spacing: Style.spacing.sm
                        visible: root.sync && root.sync.recoveryKey.length > 0

                        ActionButton {
                            colors: root.colors
                            label: "Copy recovery key"
                            onClicked: SystemClipboard.copyText(root.sync.recoveryKey)
                        }
                        ActionButton {
                            colors: root.colors
                            label: "Save recovery key"
                            onClicked: recoveryKeySaveDialog.open()
                        }
                        ActionButton {
                            colors: root.colors
                            label: "I saved it"
                            onClicked: root.sync.clearRecoveryKey()
                        }
                    }

                    Flow {
                        width: pane.width
                        spacing: Style.spacing.sm
                        visible: root.sync && root.sync.login.length > 0

                        ActionButton {
                            colors: root.colors
                            label: "Sync now"
                            onClicked: root.sync.syncNow()
                        }
                        ActionButton {
                            colors: root.colors
                            label: "Pause"
                            visible: root.sync && root.sync.enabled
                            onClicked: root.sync.pause()
                        }
                        ActionButton {
                            colors: root.colors
                            label: "Resume"
                            visible: root.sync && !root.sync.enabled
                            onClicked: root.sync.resume()
                        }
                        ActionButton {
                            colors: root.colors
                            label: "Disconnect"
                            destructive: true
                            onClicked: root.sync.disconnectProvider()
                        }
                    }
                }

                // ---- about -------------------------------------------------

                Column {
                    width: pane.width
                    visible: root.section === 11
                    spacing: pane.spacing

                    Text {
                        objectName: "aboutName"
                        text: "Omaweb"
                        color: root.colors.text
                        font.family: Style.font.family
                        font.pixelSize: Style.font.display
                    }

                    // The version carries the git description, so a build made
                    // past a tag reads 0.2.0-14-gabc1234 and a bug report names
                    // the commit it came from.
                    Text {
                        objectName: "aboutVersion"
                        text: "Version " + Qt.application.version
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
                        text: "Alpha. Expect bugs. The Security section states what this build "
                              + "verifies and what it leaves to you."
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
                        text: root.releaseWatch && root.releaseWatch.announcing ? "Omaweb "
                                                                                  + root.releaseWatch.release
                                                                                  + " is out. "
                                                                                  + root.releaseWatch.instruction :
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
                        title: "Check for new releases"
                        note: "Asks GitHub once a day what the newest release is, and sends nothing about this machine."
                        accessibleName: "Check for new releases"
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
                        text: "default browser"
                    }

                    Text {
                        objectName: "defaultBrowserState"
                        visible: DefaultBrowser.available
                        width: pane.width
                        text: DefaultBrowser.isDefault
                              ? "Omaweb opens links from other applications." :
                                "Another browser opens links from other applications."
                        color: root.colors.mutedText
                        wrapMode: Text.WordWrap
                        font.family: Style.font.family
                        font.pixelSize: Style.font.body
                    }

                    ActionButton {
                        objectName: "makeDefaultBrowserButton"
                        visible: DefaultBrowser.available && !DefaultBrowser.isDefault
                        colors: root.colors
                        label: "Make Omaweb the default"
                        accessibleName: "Make Omaweb the default browser"
                        onClicked: DefaultBrowser.makeDefault()
                    }

                    SectionLabel {
                        colors: root.colors
                        text: "project"
                    }

                    Text {
                        objectName: "aboutLinks"
                        width: pane.width
                        textFormat: Text.StyledText
                        text: '<a href="https://omaweb.app">omaweb.app</a> · '
                              + '<a href="https://github.com/villekivela/omaweb">source</a> · '
                              + '<a href="https://github.com/villekivela/omaweb/issues">issues</a>'
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
                        text: "license"
                    }

                    Text {
                        width: pane.width
                        text: "Omaweb is under the Mozilla Public License 2.0. It bundles "
                              + "third-party components under their own licenses, which "
                              + "THIRD_PARTY_NOTICES.md in the source tree lists."
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
