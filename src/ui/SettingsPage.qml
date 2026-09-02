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
    property bool clearCookiesSelected: true
    property bool clearStorageSelected: true
    property bool clearCacheSelected: true
    property bool clearPermissionsSelected: true
    property bool clearHistorySelected: true
    property bool clearEverySpaceSelected: false

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
        "downloads", "search", "privacy"]

    // about:blank and other opaque addresses have no host to name, and saying
    // "blocked on about" would be worse than saying nothing.
    readonly property string activeHost: {
        if (!browser) return ""
        const value = String(browser.activeUrl)
        if (!/^[a-z]+:\/\//.test(value)) return ""
        return value.replace(/^[a-z]+:\/\//, "").split("/")[0].split(":")[0]
    }

    signal closed()
    signal useFaviconsToggled(bool enabled)
    signal tintFaviconsToggled(bool enabled)

    visible: open
    // Settings is a place over the page rather than instead of it, so the page
    // stays visible through it: blurred, under the same translucency the
    // sidebar beside it has.
    color: "transparent"
    focus: open

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
        root.subscriptions = root.blocker ? root.blocker.subscriptions : []
        root.blockedRequestCount = root.blocker
            ? root.blocker.blockedRequestCount(root.browser.activeUrl) : 0
        userRules.text = root.blocker ? root.blocker.userRules : ""
    }

    function makeDefaultSearchEngine(id) {
        if (root.browser.saveSearchEngines(root.engines, id)) root.refresh()
    }

    function deleteSearchEngine(id, wasDefault) {
        const next = root.engines.filter(function(engine) { return engine.id !== id })
        if (next.length === 0) return
        const defaultId = wasDefault ? next[0].id
            : root.engines.filter(function(engine) { return engine.default })[0].id
        if (root.browser.saveSearchEngines(next, defaultId)) root.refresh()
    }

    onOpenChanged: if (open) refresh()

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
        height: 40

        Text {
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            text: "Settings"
            color: root.colors.text
            font.family: Style.font.family
            font.pixelSize: 26
            Accessible.role: Accessible.Heading
            Accessible.name: "Settings"
        }

        ChromeButton {
            objectName: "closeSettingsButton"
            anchors.right: parent.right
            anchors.bottom: parent.bottom
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
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        anchors.leftMargin: 48
        anchors.rightMargin: 48
        anchors.topMargin: 28
        anchors.bottomMargin: 24
        spacing: 40

        Column {
            id: rail
            width: 150
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
                    font.pixelSize: Style.font.body
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
            width: parent.width - rail.width - 40
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

                // ---- keyboard ----------------------------------------------

                SettingToggle {
                    objectName: "keyboardNavigationEnabled"
                    width: pane.width
                    visible: root.section === 1
                    colors: root.colors
                    title: "Keyboard navigation"
                    note: "Tanto's own command layer. It gives the same commands with every "
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
                            + "to their displayed update address when Tanto starts. Remote search "
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
                    note: "Downloads Tanto has recorded in this Space appear here."
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
                                onClicked: root.deleteSearchEngine(modelData.id, modelData.default)
                            }
                        }
                    }
                }

                Column {
                    width: pane.width
                    visible: root.section === 5
                    spacing: 8

                    SectionLabel { colors: root.colors; text: "add a search engine" }
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
                            const id = engineName.text.trim().toLowerCase().replace(/[^a-z0-9]+/g, "-")
                            const next = root.engines.slice()
                            next.push({"id": id, "name": engineName.text.trim(),
                                "queryUrl": engineQueryUrl.text.trim(),
                                "keyword": engineKeyword.text.trim()})
                            if (root.browser.saveSearchEngines(next, id)) {
                                engineName.text = ""
                                engineQueryUrl.text = ""
                                engineKeyword.text = ""
                                root.refresh()
                            }
                        }
                    }
                }

                // ---- privacy -----------------------------------------------

                SectionLabel {
                    visible: root.section === 6
                    colors: root.colors
                    text: "clear browsing data"
                }

                SettingToggle {
                    objectName: "clearCookies"
                    width: pane.width
                    visible: root.section === 6
                    colors: root.colors
                    title: "Cookies"
                    checked: root.clearCookiesSelected
                    onClicked: root.clearCookiesSelected = !checked
                }
                SettingToggle {
                    objectName: "clearStorage"
                    width: pane.width
                    visible: root.section === 6
                    colors: root.colors
                    title: "Site storage"
                    checked: root.clearStorageSelected
                    onClicked: root.clearStorageSelected = !checked
                }
                SettingToggle {
                    objectName: "clearCache"
                    width: pane.width
                    visible: root.section === 6
                    colors: root.colors
                    title: "Cache"
                    checked: root.clearCacheSelected
                    onClicked: root.clearCacheSelected = !checked
                }
                SettingToggle {
                    objectName: "clearPermissions"
                    width: pane.width
                    visible: root.section === 6
                    colors: root.colors
                    title: "Site permissions"
                    checked: root.clearPermissionsSelected
                    onClicked: root.clearPermissionsSelected = !checked
                }
                SettingToggle {
                    objectName: "clearHistory"
                    width: pane.width
                    visible: root.section === 6
                    colors: root.colors
                    title: "History"
                    checked: root.clearHistorySelected
                    onClicked: root.clearHistorySelected = !checked
                }

                SettingRow {
                    width: pane.width
                    visible: root.section === 6
                    colors: root.colors
                    title: "Time range"
                    note: "Applied within the selected Space by default."

                    ComboBox {
                        id: clearRange
                        objectName: "clearTimeRange"
                        model: ["last hour", "last day", "last week", "all time"]
                        currentIndex: 1
                        Accessible.name: "Browsing data time range"
                    }
                }

                SettingToggle {
                    objectName: "clearEverySpace"
                    width: pane.width
                    visible: root.section === 6
                    colors: root.colors
                    title: "Every Space"
                    note: "Off clears only " + (root.browser ? root.browser.activeSpaceName : "this Space") + "."
                    checked: root.clearEverySpaceSelected
                    onClicked: root.clearEverySpaceSelected = !checked
                }

                SettingField {
                    id: clearEverySpaceConfirmation
                    objectName: "clearEverySpaceConfirmation"
                    width: pane.width
                    visible: root.section === 6 && root.clearEverySpaceSelected
                    colors: root.colors
                    destructive: true
                    placeholder: "type CLEAR ALL"
                    accessibleName: "Confirm clearing every Space"
                }

                ActionButton {
                    objectName: "clearBrowsingDataButton"
                    visible: root.section === 6
                    colors: root.colors
                    label: root.clearEverySpaceSelected ? "Clear every Space" : "Clear this Space"
                    destructive: true
                    enabled: !root.clearEverySpaceSelected
                        || clearEverySpaceConfirmation.text === "CLEAR ALL"
                    onClicked: {
                        const selected = []
                        if (root.clearCookiesSelected) selected.push("cookies")
                        if (root.clearStorageSelected) selected.push("storage")
                        if (root.clearCacheSelected) selected.push("cache")
                        if (root.clearPermissionsSelected) selected.push("permissions")
                        if (root.clearHistorySelected) selected.push("history")
                        const durations = [3600000, 86400000, 604800000, 0]
                        const duration = durations[clearRange.currentIndex]
                        const since = duration === 0 ? 0 : Date.now() - duration
                        if (root.browser.clearBrowsingData(selected, since,
                                root.clearEverySpaceSelected,
                                clearEverySpaceConfirmation.text)) {
                            clearEverySpaceConfirmation.text = ""
                            root.refresh()
                        }
                    }
                }
            }
        }
    }
}
