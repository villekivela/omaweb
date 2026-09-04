import QtQuick
import QtQuick.Controls
import qs.Commons

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
    property bool open: false
    property int section: 0
    property var downloads: []
    property var subscriptions: []
    property int blockedRequestCount: 0
    property bool useFavicons: true
    property bool tintFavicons: true
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

    // Whether anything in here is waiting on the reader. The sidebar's settings
    // button reads this, so a notice that lives on one section is still
    // findable from outside it.
    readonly property bool needsAttention: keyboardReport.length > 0

    readonly property var sections: ["tabs", "keyboard", "content blocking", "network",
        "downloads", "search", "privacy", "about"]

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
        return name.replace(/(^|\s)\S/g, function(first) { return first.toUpperCase() })
    }

    readonly property int railWidth: {
        // Read for the dependency alone: advanceWidth() measures in C++ off a
        // font this binding never otherwise touches.
        void (railMetrics.font.pixelSize)
        void (railMetrics.font.family)
        let widest = 0
        for (let index = 0; index < root.sections.length; ++index)
            widest = Math.max(widest, railMetrics.advanceWidth(root.railLabel(root.sections[index])))
        return Math.ceil(widest)
    }

    // -------------------------------------------------------------- geometry
    //
    // Nothing about this page's frame is written down. `Style.space()` keeps
    // the proportions the page already had while letting the theme make the
    // shell denser or roomier, and `Style.spacing.*` is the shared rhythm
    // everything else in the kit is set on. A pixel count here would be right
    // at one theme font size and crowd or clip at the next (#85).
    readonly property int sideMargin: Style.space(48)
    readonly property int topInset: Style.space(40)
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
    readonly property int noteMeasure: Math.ceil(
        noteMetrics.averageCharacterWidth * root.noteCharacters)

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
        void (fieldMetrics.font.pixelSize)
        void (fieldMetrics.font.family)
        let widest = 0
        for (const field in root.subscriptionPlaceholders)
            widest = Math.max(widest,
                fieldMetrics.advanceWidth(root.subscriptionPlaceholders[field]))
        return Math.ceil(widest) + Style.spacing.controlPaddingX * 2
    }

    // about:blank and other opaque addresses have no host to name, and saying
    // "blocked on about" would be worse than saying nothing.
    readonly property string activeHost: {
        if (!browser) return ""
        const value = String(browser.activeUrl)
        if (!/^[a-z]+:\/\//.test(value)) return ""
        return value.replace(/^[a-z]+:\/\//, "").split("/")[0].split(":")[0]
    }

    // Bytes as the reader reads them. A retained tab whose renderer the
    // platform cannot account for says so rather than claiming nothing.
    function resourceLabel(bytes) {
        if (!(bytes > 0)) return "size unavailable"
        const megabytes = bytes / (1024 * 1024)
        return (megabytes >= 100 ? Math.round(megabytes)
            : Math.round(megabytes * 10) / 10) + " MB"
    }

    signal closed()
    signal retainedTabReleased(string tabId)
    signal useFaviconsToggled(bool enabled)
    signal tintFaviconsToggled(bool enabled)

    visible: open
    // Settings is a place over the page rather than instead of it, so the page
    // stays visible through it: blurred, under the same translucency the
    // sidebar beside it has.
    color: "transparent"
    // The dialog over this page owns the keyboard while it is open, so Escape
    // there closes the dialog rather than the place behind it.
    focus: open && !clearDataOpen

    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Escape) {
            root.closed()
            event.accepted = true
        }
    }

    function refresh() {
        if (!root.browser) return
        root.downloads = root.browser.downloadHistory()
        root.engines = root.browser.searchEngines()
        root.enginePresets = root.browser.searchEnginePresets()
        root.subscriptions = root.blocker ? root.blocker.subscriptions : []
        root.blockedRequestCount = root.blocker
            ? root.blocker.blockedRequestCount(root.browser.activeUrl) : 0
        userRules.text = root.blocker ? root.blocker.userRules : ""
        root.loadBrowsingDataSelection()
    }

    // The categories and the range are remembered through the same preference
    // store `use-favicons` uses. Scope is not: it is the one argument with no
    // prior art to borrow, and a choice inherited from a config file written
    // weeks ago is a default rather than a choice.
    function loadBrowsingDataSelection() {
        if (!root.browser) return
        const saved = root.browser.preference("clear-data-categories",
            "cookies,storage,cache,permissions,history")
        root.clearCategories = saved.length > 0 ? saved.split(",") : []
        root.clearRange = root.browser.preference("clear-data-range", "86400000")
    }

    function toggleClearCategory(value) {
        const next = root.clearCategories.slice()
        const at = next.indexOf(value)
        if (at >= 0) next.splice(at, 1)
        else next.push(value)
        root.clearCategories = next
        if (root.browser) root.browser.setPreference("clear-data-categories", next.join(","))
    }

    function chooseClearRange(value) {
        root.clearRange = value
        if (root.browser) root.browser.setPreference("clear-data-range", value)
    }

    function makeDefaultSearchEngine(id) {
        if (root.browser.setDefaultSearchEngine(id)) root.refresh()
    }

    function deleteSearchEngine(id) {
        if (root.browser.deleteSearchEngine(id)) root.refresh()
    }

    function searchEngineInstalled(id) {
        return root.engines.some(function(engine) { return engine.id === id })
    }

    onOpenChanged: if (open) refresh()
    // The selection is state the page holds whether or not it has been opened,
    // so it is read once rather than on the first visit.
    Component.onCompleted: loadBrowsingDataSelection()

    PageBackdrop {
        objectName: "settingsBackdrop"
        anchors.fill: parent
        source: root.pageSource
        tint: root.colors.sheet
    }

    Item {
        id: header
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
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        anchors.leftMargin: root.sideMargin
        anchors.rightMargin: root.sideMargin
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

                    Keys.onPressed: function(event) {
                        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter
                                || event.key === Qt.Key_Space) {
                            root.section = index
                            event.accepted = true
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            parent.forceActiveFocus()
                            root.section = index
                        }
                    }
                }
            }
        }

        ScrollView {
            id: scroll
            width: body.width - rail.width - body.spacing
            height: parent.height
            contentWidth: availableWidth
            clip: true

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
                                title: modelData.title.length > 0
                                    ? modelData.title : String(modelData.url)
                                note: modelData.spaceName + " · "
                                    + (modelData.inspected ? "Developer tools" : "Keep active")
                                    + " · " + (modelData.running
                                        ? root.resourceLabel(modelData.residentBytes)
                                        : "not running")

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

                // ---- keyboard ----------------------------------------------

                Column {
                    width: pane.width
                    visible: root.section === 1
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
                        onClicked: if (root.keyboard) root.keyboard.setEnabled(!checked)
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
                }

                // ---- content blocking --------------------------------------

                Column {
                    width: pane.width
                    visible: root.section === 2
                    spacing: pane.spacing

                    SettingToggle {
                        objectName: "siteBlockingEnabled"
                        width: pane.width
                        colors: root.colors
                        title: "Block requests on this site"
                        note: root.blockedRequestCount + " requests blocked"
                            + (root.activeHost.length > 0 ? " on " + root.activeHost : "")
                            + " so far."
                        accessibleName: "Enable content blocking for this site"
                        checked: root.blocker && root.browser
                            ? root.blocker.siteEnabled(root.browser.activeUrl) : false
                        onClicked: if (root.blocker) root.blocker.setSiteEnabled(
                            root.browser.activeUrl, !checked)
                    }

                    Repeater {
                        model: root.section === 2 ? root.subscriptions : []

                        SettingToggle {
                            required property var modelData

                            width: pane.width
                            colors: root.colors
                            title: modelData.title
                            note: modelData.updateStatus + " · " + modelData.license
                                + "\nSource " + modelData.source
                                + "\nUpdates from " + modelData.updateAddress
                            accessibleName: "Enable " + modelData.title
                            checked: modelData.enabled
                            onClicked: root.blocker.setSubscriptionEnabled(modelData.id, !checked)
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
                            ? Math.max(1, (pane.width - columnSpacing) / 2)
                            : pane.width

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
                                subscriptionSource.text, subscriptionLicense.text,
                                subscriptionUpdate.text)
                            root.refresh()
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
                        width: pane.width
                        text: "Network rules, plain CSS cosmetic rules, parameter "
                            + "stripping, and the scriptlets and substitute resources "
                            + "in the bundled uBlock Origin library are supported; the "
                            + "scriptlets uBlock Origin gates behind trust are refused. "
                            + "Procedural selectors, response rewriting, content "
                            + "security policies, HTML filtering, dynamic rules and "
                            + "CNAME uncloaking are not supported."
                        color: root.colors.mutedText
                        wrapMode: Text.WordWrap
                        font.family: Style.font.family
                        font.pixelSize: Style.font.caption
                    }

                    Text {
                        width: pane.width
                        visible: root.blocker !== null && root.blocker !== undefined
                            && root.blocker.compilationReport.unsupported !== undefined
                            && Object.keys(root.blocker.compilationReport.unsupported).length > 0
                        text: "Unsupported rules in the active lists: "
                            + JSON.stringify(root.blocker
                                ? root.blocker.compilationReport.unsupported : {})
                        color: root.colors.mutedText
                        wrapMode: Text.WordWrap
                        font.family: Style.font.family
                        font.pixelSize: Style.font.caption
                    }

                    ActionButton {
                        objectName: "saveUserRulesButton"
                        colors: root.colors
                        label: root.blocker && root.blocker.compiling
                            ? "Compiling…" : "Save user rules"
                        accessibleName: "Save user rules"
                        primary: true
                        enabled: root.blocker ? !root.blocker.compiling : false
                        onClicked: root.blocker.userRules = userRules.text
                    }
                }

                // ---- network -----------------------------------------------

                Column {
                    width: pane.width
                    visible: root.section === 3
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
                    visible: root.section === 4
                    spacing: 0

                    SettingRow {
                        width: pane.width
                        colors: root.colors
                        separated: false
                        title: "Download directory"
                        note: root.browser ? root.browser.downloadDirectory : ""

                        Text {
                            text: root.browser && root.browser.acceptDownloads
                                ? "accepting" : "blocked"
                            color: root.colors.mutedText
                            font.family: Style.font.family
                            font.pixelSize: Style.font.body
                        }
                    }

                    Repeater {
                        model: root.section === 4 ? root.downloads : []

                        SettingRow {
                            required property int index
                            required property var modelData

                            objectName: "recordedDownload-" + index
                            width: pane.width
                            colors: root.colors
                            title: modelData.path
                            note: modelData.state
                                + (modelData.error.length > 0 ? " · " + modelData.error : "")
                        }
                    }

                    SettingRow {
                        width: pane.width
                        visible: root.downloads.length === 0
                        colors: root.colors
                        title: "No recorded downloads"
                        note: "Downloads Omaweb has recorded in this Space appear here."
                    }
                }

                // ---- search ------------------------------------------------

                Column {
                    width: pane.width
                    visible: root.section === 5
                    spacing: pane.spacing

                    Column {
                        width: pane.width
                        spacing: 0

                        Repeater {
                            id: searchEngineList
                            objectName: "searchEngineList"
                            model: root.section === 5 ? root.engines : []

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
                                        text: modelData.keyword.length > 0
                                            ? "keyword " + modelData.keyword : "no keyword"
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

                    SectionLabel { colors: root.colors; text: "add a provider" }

                    Row {
                        id: providerRow
                        width: pane.width
                        spacing: Style.spacing.lg

                        SettingDropdown {
                            id: providerPreset
                            objectName: "searchProviderPreset"
                            width: providerRow.width - addProvider.width - providerRow.spacing
                            colors: root.colors
                            options: root.enginePresets.map(function(engine) {
                                return { value: engine.id, label: engine.name }
                            })
                            value: options.length > 0 ? String(options[0].value) : ""
                            accessibleName: "Predefined search provider"
                        }

                        ActionButton {
                            id: addProvider
                            objectName: "addSearchProviderButton"
                            colors: root.colors
                            label: root.searchEngineInstalled(providerPreset.value)
                                ? "Added" : "Add"
                            enabled: providerPreset.value !== ""
                                && !root.searchEngineInstalled(providerPreset.value)
                            onClicked: {
                                if (root.browser.addSearchEnginePreset(providerPreset.value))
                                    root.refresh()
                            }
                        }
                    }

                    SectionLabel { colors: root.colors; text: "add a custom engine" }

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
                        enabled: engineName.text.trim().length > 0
                            && engineQueryUrl.text.indexOf("{query}") >= 0
                        onClicked: {
                            if (root.browser.addSearchEngine(engineName.text,
                                    engineQueryUrl.text, engineKeyword.text)) {
                                engineName.text = ""
                                engineQueryUrl.text = ""
                                engineKeyword.text = ""
                                root.refresh()
                            }
                        }
                    }
                }

                // ---- privacy -----------------------------------------------

                Column {
                    width: pane.width
                    visible: root.section === 6
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
                }

                // ---- about -------------------------------------------------

                Column {
                    width: pane.width
                    visible: root.section === 7
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

                    Text {
                        width: pane.width
                        text: "Pre-alpha. Do not use it for sensitive browsing yet."
                        color: root.colors.mutedText
                        wrapMode: Text.WordWrap
                        font.family: Style.font.family
                        font.pixelSize: Style.font.caption
                    }

                    SectionLabel { colors: root.colors; text: "project" }

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
                        onLinkActivated: function(link) { Qt.openUrlExternally(link) }

                        HoverHandler {
                            cursorShape: parent.hoveredLink !== ""
                                ? Qt.PointingHandCursor : Qt.ArrowCursor
                        }
                    }

                    SectionLabel { colors: root.colors; text: "license" }

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

        onCategoryToggled: function(value) { root.toggleClearCategory(value) }
        onRangeChosen: function(value) { root.chooseClearRange(value) }
        onDismissed: root.clearDataOpen = false
        onConfirmed: function(categories, since, everySpace, confirmation) {
            root.clearDataOpen = false
            if (root.browser.clearBrowsingData(categories, since, everySpace, confirmation))
                root.refresh()
        }
    }
}
