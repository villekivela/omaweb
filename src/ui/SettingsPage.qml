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
        anchors.leftMargin: 48
        anchors.rightMargin: 48
        anchors.topMargin: 40
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
            width: 30
            height: 30
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
        anchors.leftMargin: 48
        anchors.rightMargin: 48
        anchors.topMargin: 28
        anchors.bottomMargin: 24
        spacing: 40
        // A dialog over this page is modal, so the page under it takes neither
        // a click nor a Tab while it stands. Qt has no tab fence a QML item can
        // raise, so the way to keep the ring inside the dialog is to take the
        // rest of the page out of it.
        enabled: !root.clearDataOpen

        Column {
            id: rail
            width: root.railWidth
            spacing: 2

            Repeater {
                model: root.sections

                Text {
                    required property int index
                    required property string modelData

                    objectName: "settingsSection" + index
                    text: modelData
                    color: index === root.section ? root.colors.accent : root.colors.mutedText
                    topPadding: 6
                    bottomPadding: 6
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
                width: scroll.availableWidth

                // ---- tabs ---------------------------------------------------

                SettingToggle {
                    objectName: "useFavicons"
                    width: pane.width
                    visible: root.section === 0
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
                    visible: root.section === 0
                    colors: root.colors
                    title: "Tint favicons"
                    note: "Recolor site artwork to match the sidebar palette."
                    accessibleName: "Tint favicons"
                    enabled: root.useFavicons
                    checked: root.tintFavicons
                    onClicked: root.tintFaviconsToggled(!checked)
                }

                SectionLabel {
                    visible: root.section === 0
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
                        title: modelData.title.length > 0 ? modelData.title : String(modelData.url)
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
                    visible: root.section === 0 && root.retainedTabs.length === 0
                    colors: root.colors
                    title: "Nothing is kept active"
                    note: "A Pinned tab set to Keep active, or a tab with Developer tools attached, keeps running while its Space is inactive and is listed here with what it costs."
                }

                // ---- keyboard ----------------------------------------------

                SettingToggle {
                    objectName: "keyboardNavigationEnabled"
                    width: pane.width
                    visible: root.section === 1
                    colors: root.colors
                    title: "Keyboard navigation"
                    note: "Omaweb's own command layer. It gives the same commands with every "
                        + "engine, and lets sites receive the keys they need."
                    accessibleName: "Enable Keyboard navigation"
                    checked: root.keyboard ? root.keyboard.enabled === true : false
                    onClicked: if (root.keyboard) root.keyboard.setEnabled(!checked)
                }
                // A binding this build cannot honour is dropped rather than
                // taking the whole keymap with it, so this notice is the only
                // thing that says a configured key is missing. It waits beside
                // the setting it is about: over the page it would be a banner
                // the reader cannot dismiss and cannot act on while browsing.
                NoticeBox {
                    objectName: "keyboardBindingNotice"
                    width: pane.width
                    visible: root.section === 1 && root.keyboardReport.length > 0
                    colors: root.colors
                    iconFontFamily: root.iconFontFamily
                    glyph: "keyboard_alt"
                    title: "Some bindings were ignored"
                    detail: root.keyboardReport
                }

                // ---- content blocking --------------------------------------

                SettingToggle {
                    objectName: "siteBlockingEnabled"
                    width: pane.width
                    visible: root.section === 2
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

                Item { width: 1; height: root.section === 2 ? 28 : 0; visible: root.section === 2 }

                Column {
                    width: pane.width
                    visible: root.section === 2
                    spacing: 8

                    SectionLabel {
                        colors: root.colors
                        text: "add a list"
                    }

                    Grid {
                        columns: 2
                        columnSpacing: 10
                        rowSpacing: 8

                        SettingField {
                            id: subscriptionTitle
                            width: (pane.width - 10) / 2
                            colors: root.colors
                            placeholder: "list name"
                            accessibleName: "Subscription name"
                        }

                        SettingField {
                            id: subscriptionLicense
                            width: (pane.width - 10) / 2
                            colors: root.colors
                            placeholder: "license"
                            accessibleName: "Subscription license"
                        }

                        SettingField {
                            id: subscriptionSource
                            width: (pane.width - 10) / 2
                            colors: root.colors
                            placeholder: "source page"
                            accessibleName: "Subscription source page"
                        }

                        SettingField {
                            id: subscriptionUpdate
                            width: (pane.width - 10) / 2
                            colors: root.colors
                            placeholder: "update address"
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

                    Item { width: 1; height: 20 }

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

                SettingRow {
                    width: pane.width
                    visible: root.section === 3
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
                    visible: root.section === 3
                    colors: root.colors
                    title: "Filter-list updates"

                    Text {
                        objectName: "automaticRequestsStatus"
                        width: Math.min(260, pane.width / 2)
                        text: "Enabled filter-list subscriptions make automatic network requests "
                            + "to their displayed update address when Omaweb starts. Remote search "
                            + "suggestions remain off."
                        color: root.colors.mutedText
                        wrapMode: Text.WordWrap
                        font.family: Style.font.family
                        font.pixelSize: Style.font.caption
                    }
                }

                // ---- downloads ---------------------------------------------

                SettingRow {
                    width: pane.width
                    visible: root.section === 4
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
                        required property var modelData

                        width: pane.width
                        colors: root.colors
                        title: modelData.path
                        note: modelData.state
                            + (modelData.error.length > 0 ? " · " + modelData.error : "")
                    }
                }

                SettingRow {
                    width: pane.width
                    visible: root.section === 4 && root.downloads.length === 0
                    colors: root.colors
                    title: "No recorded downloads"
                    note: "Downloads Omaweb has recorded in this Space appear here."
                }

                // ---- search ------------------------------------------------

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
                            spacing: 8
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

                Column {
                    width: pane.width
                    visible: root.section === 5
                    spacing: 8

                    SectionLabel { colors: root.colors; text: "add a provider" }
                    Row {
                        width: parent.width
                        spacing: 8

                        SettingDropdown {
                            id: providerPreset
                            objectName: "searchProviderPreset"
                            width: parent.width - addProvider.width - 8
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

                    Item { width: 1; height: 12 }
                    SectionLabel { colors: root.colors; text: "add a custom engine" }
                    SettingField {
                        id: engineName
                        width: parent.width
                        colors: root.colors
                        placeholder: "name"
                        accessibleName: "Search engine name"
                    }
                    SettingField {
                        id: engineQueryUrl
                        width: parent.width
                        colors: root.colors
                        placeholder: "query URL with {query}"
                        accessibleName: "Search engine query URL"
                    }
                    SettingField {
                        id: engineKeyword
                        width: parent.width
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

                // The section is named for what it will hold rather than for
                // its one row: the reputation protection, proxy reporting and
                // third-party cookie allowances the requirements describe are
                // privacy and are not browsing data.
                SectionLabel {
                    visible: root.section === 6
                    colors: root.colors
                    text: "privacy"
                }

                // One row and one button, which is the shape Chrome and Firefox
                // both settled on. What used to be five switches and a scope
                // switch on this page were arguments to that button, and they
                // now sit in the dialog it opens (ADR 0031). The section has
                // room to grow into the reputation protection, proxy reporting
                // and third-party cookie allowances Omaweb has not built.
                SettingRow {
                    objectName: "clearBrowsingDataRow"
                    width: pane.width
                    visible: root.section === 6
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

                Column {
                    width: pane.width
                    visible: root.section === 7
                    spacing: 8

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

                    Item { width: 1; height: 4 }

                    Text {
                        width: pane.width
                        text: "Pre-alpha. Do not use it for sensitive browsing yet."
                        color: root.colors.mutedText
                        wrapMode: Text.WordWrap
                        font.family: Style.font.family
                        font.pixelSize: Style.font.caption
                    }

                    Item { width: 1; height: 12 }
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

                    Item { width: 1; height: 12 }
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
