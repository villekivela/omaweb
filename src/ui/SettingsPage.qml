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

    readonly property var sections: ["tabs", "keyboard", "content blocking", "network", "downloads"]

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
    color: colors.windowOpaque
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
        root.subscriptions = root.blocker ? root.blocker.subscriptions : []
        root.blockedRequestCount = root.blocker
            ? root.blocker.blockedRequestCount(root.browser.activeUrl) : 0
        userRules.text = root.blocker ? root.blocker.userRules : ""
    }

    onOpenChanged: if (open) refresh()

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
                    checked: root.keyboard ? root.keyboard.enabled : false
                    onClicked: if (root.keyboard) root.keyboard.setEnabled(!checked)
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
                        text: "Network rules, plain CSS cosmetic rules and the scriptlets "
                            + "in the bundled uBlock Origin library are supported; the "
                            + "scriptlets uBlock Origin gates behind trust are refused. "
                            + "Procedural selectors, response rewriting, HTML filtering, "
                            + "dynamic rules, CNAME uncloaking, redirects and resource "
                            + "replacement are not supported."
                        color: root.colors.mutedText
                        wrapMode: Text.WordWrap
                        font.family: Style.font.family
                        font.pixelSize: Style.font.caption
                    }

                    Text {
                        width: pane.width
                        visible: root.blocker
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
            }
        }
    }
}
