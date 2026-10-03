import QtQuick
import QtQuick.Controls
import qs.Commons

Rectangle {
    id: root
    objectName: "historySurface"

    property var colors
    property string iconFontFamily
    property var browser
    property bool open: false
    // How far below its place the sheet's content stands, in pixels, while
    // the sheet arrives or leaves. The backdrop stays where it is: a ground
    // that moved would show the page's edge above it for the length of the
    // lift.
    property real lift: 0
    property Item pageSource: null
    property var rows: []
    // The tabs Omaweb put away in this Space that the search holds, newest
    // first. They lead the sheet, above the visits.
    property var putAwayRows: []

    signal closed

    visible: open
    color: "transparent"
    focus: open

    function refresh() {
        rows = browser ? browser.history(search.text) : [];
        putAwayRows = browser ? matchingPutAway(search.text) : [];
    }

    // The search reads a put-away tab where it reads a visit: its title and
    // its address.
    function matchingPutAway(text) {
        const needle = text.trim().toLowerCase();
        return browser.putAwayTabs.filter(function (tab) {
            return needle.length === 0 || tab.title.toLowerCase().indexOf(needle) >= 0 || String(
                        tab.url).toLowerCase().indexOf(needle) >= 0;
        });
    }

    // How long ago a tab was put away, in the largest whole unit. One of a
    // unit has a source of its own, so English never reads "1 days".
    function ago(time) {
        const minutes = Math.max(0, Math.floor((Date.now() - time) / 60000));
        const hours = Math.floor(minutes / 60);
        const days = Math.floor(hours / 24);
        if (days > 0)
            return days === 1 ? qsTr("a day ago") : qsTr("%n days ago", "", days);
        if (hours > 0)
            return hours === 1 ? qsTr("an hour ago") : qsTr("%n hours ago", "", hours);
        if (minutes > 0)
            return minutes === 1 ? qsTr("a minute ago") : qsTr("%n minutes ago", "", minutes);
        return qsTr("just now");
    }

    function origin(address) {
        return String(address).replace(/^(https?:\/\/[^/]+).*$/, "$1");
    }

    onOpenChanged: if (open) {
                       search.text = "";
                       refresh();
                       search.forceActiveFocus();
                   }

    Connections {
        target: root.browser
        function onActiveSpaceChanged() {
            if (root.open)
                root.refresh();
        }
        function onPutAwayTabsChanged() {
            if (root.open)
                root.refresh();
        }
    }

    Keys.onPressed: function (event) {
        if (event.key === Qt.Key_Escape) {
            root.closed();
            event.accepted = true;
        }
    }

    SheetFloor {}

    PageBackdrop {
        anchors.fill: parent
        source: root.pageSource
        tint: root.colors.sheet
    }

    SheetInsets {
        id: sheetInsets
    }

    Column {
        transform: Translate {
            y: root.lift
        }
        anchors.fill: parent
        // Every full-window sheet leaves the same gap above its heading; the
        // rest of the frame is this sheet's own and follows the theme's
        // spacing rather than a raw pixel count.
        anchors.topMargin: sheetInsets.top
        anchors.leftMargin: Style.space(48)
        anchors.rightMargin: Style.space(48)
        anchors.bottomMargin: Style.space(48)
        spacing: Style.space(18)

        Item {
            width: parent.width
            height: historyEyebrow.height + Style.spacing.md + historyHeading.height

            Text {
                id: historyEyebrow
                objectName: "historyEyebrow"
                anchors.left: parent.left
                anchors.top: parent.top
                text: qsTr("%1 · esc closes").arg(root.browser ? root.browser.activeSpaceName : "")
                color: root.colors.mutedText
                font.family: Style.font.family
                font.pixelSize: Style.font.caption
                font.bold: true
                font.capitalization: Font.AllUppercase
                font.letterSpacing: 1.6
                Accessible.ignored: true
            }

            Text {
                id: historyHeading
                anchors.left: parent.left
                anchors.top: historyEyebrow.bottom
                anchors.topMargin: Style.spacing.md
                text: qsTr("History")
                color: root.colors.text
                font.family: Style.font.family
                font.pixelSize: Style.font.display
                Accessible.role: Accessible.Heading
                Accessible.name: qsTr("History")
            }

            ChromeButton {
                id: closeButton
                objectName: "closeHistoryButton"
                anchors.right: parent.right
                anchors.verticalCenter: historyHeading.verticalCenter
                width: 30
                height: 30
                icon: "close"
                accessibleName: qsTr("Close History")
                fontFamily: root.iconFontFamily
                foreground: root.colors.mutedText
                accent: root.colors.accent
                onClicked: root.closed()
            }
        }

        Row {
            width: parent.width
            spacing: 10

            SettingField {
                id: search
                objectName: "historySearch"
                width: parent.width - clearAll.width - 10
                colors: root.colors
                placeholder: qsTr("search this Space")
                accessibleName: qsTr("Search History")
                onTextChanged: root.refresh()
            }

            ActionButton {
                id: clearAll
                objectName: "clearHistoryAllButton"
                colors: root.colors
                label: qsTr("Clear this Space")
                destructive: true
                onClicked: {
                    root.browser.deleteHistorySince(0);
                    root.refresh();
                }
            }
        }

        Row {
            spacing: 8
            ActionButton {
                colors: root.colors
                label: qsTr("Delete last hour")
                destructive: true
                onClicked: {
                    root.browser.deleteHistorySince(Date.now() - 3600000);
                    root.refresh();
                }
            }
            ActionButton {
                colors: root.colors
                label: qsTr("Delete last day")
                destructive: true
                onClicked: {
                    root.browser.deleteHistorySince(Date.now() - 86400000);
                    root.refresh();
                }
            }
            ActionButton {
                colors: root.colors
                label: qsTr("Delete last week")
                destructive: true
                onClicked: {
                    root.browser.deleteHistorySince(Date.now() - 604800000);
                    root.refresh();
                }
            }
        }

        ListView {
            id: historyList
            objectName: "historyList"
            width: parent.width
            height: parent.height - y
            clip: true
            spacing: 6
            model: root.rows

            // Above the visits, so the tabs Omaweb closed for the reader are
            // the first thing the sheet offers back.
            header: Column {
                id: putAwayGroup
                objectName: "historyPutAwayGroup"
                width: historyList.width
                visible: root.putAwayRows.length > 0
                height: visible ? implicitHeight + Style.space(18) : 0
                spacing: 6
                // A header that grows after the list has laid out its rows is
                // left above the view, so the view goes back to its top.
                onHeightChanged: historyList.positionViewAtBeginning()

                SectionLabel {
                    objectName: "historyPutAwayHeading"
                    colors: root.colors
                    text: qsTr("put away", "History sheet group: tabs Omaweb put away")
                }

                Repeater {
                    model: root.putAwayRows

                    Rectangle {
                        required property var modelData
                        objectName: "historyPutAwayRow"
                        width: putAwayGroup.width
                        height: 68
                        radius: 8
                        color: root.colors.surface
                        border.width: 1
                        border.color: root.colors.border

                        Column {
                            anchors.left: parent.left
                            anchors.right: reopen.left
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: 14
                            anchors.rightMargin: 12
                            spacing: 3

                            Text {
                                objectName: "putAwayTitle"
                                width: parent.width
                                text: modelData.title
                                color: root.colors.text
                                elide: Text.ElideRight
                                font.family: Style.font.family
                                font.pixelSize: Style.font.body
                            }
                            Row {
                                width: parent.width
                                spacing: 10

                                Text {
                                    objectName: "putAwayHost"
                                    width: Math.min(implicitWidth, parent.width - age.width
                                                    - parent.spacing)
                                    text: modelData.host
                                    color: root.colors.mutedText
                                    elide: Text.ElideRight
                                    font.family: Style.font.family
                                    font.pixelSize: Style.font.caption
                                }
                                Text {
                                    id: age
                                    objectName: "putAwayAge"
                                    text: root.ago(modelData.putAwayAt)
                                    color: root.colors.mutedText
                                    font.family: Style.font.family
                                    font.pixelSize: Style.font.caption
                                }
                            }
                        }

                        ActionButton {
                            id: reopen
                            objectName: "reopenPutAwayButton"
                            anchors.right: parent.right
                            anchors.rightMargin: 10
                            anchors.verticalCenter: parent.verticalCenter
                            colors: root.colors
                            label: qsTr("Reopen", "verb: open a put-away tab again")
                            primary: true
                            accessibleName: qsTr("Reopen %1").arg(modelData.title)
                            // Reopening takes the row out of the list, and this
                            // button with it, so the sheet is closed first.
                            onClicked: {
                                const sheet = root;
                                const id = modelData.id;
                                sheet.closed();
                                sheet.browser.reopenPutAwayTab(id);
                            }
                        }
                    }
                }

                SectionLabel {
                    visible: historyList.count > 0
                    colors: root.colors
                    text: qsTr("visits", "History sheet group: pages visited")
                }
            }

            delegate: Rectangle {
                required property var modelData
                width: historyList.width
                height: 68
                radius: 8
                color: root.colors.surface
                border.width: 1
                border.color: root.colors.border

                Column {
                    anchors.left: parent.left
                    anchors.right: buttons.left
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 14
                    anchors.rightMargin: 12
                    spacing: 3

                    Text {
                        width: parent.width
                        text: modelData.title
                        color: root.colors.text
                        elide: Text.ElideRight
                        font.family: Style.font.family
                        font.pixelSize: Style.font.body
                    }
                    Text {
                        width: parent.width
                        text: String(modelData.url)
                        color: root.colors.mutedText
                        elide: Text.ElideRight
                        font.family: Style.font.family
                        font.pixelSize: Style.font.caption
                    }
                }

                Row {
                    id: buttons
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 8

                    ActionButton {
                        colors: root.colors
                        label: qsTr("Origin")
                        accessibleName: qsTr("Delete visits to %1").arg(root.origin(modelData.url))
                        onClicked: {
                            root.browser.deleteHistoryOrigin(modelData.url);
                            root.refresh();
                        }
                    }
                    ActionButton {
                        colors: root.colors
                        label: qsTr("Delete")
                        destructive: true
                        accessibleName: qsTr("Delete this visit")
                        onClicked: {
                            root.browser.deleteHistoryVisit(modelData.id);
                            root.refresh();
                        }
                    }
                }
            }

            Text {
                anchors.centerIn: parent
                visible: historyList.count === 0 && root.putAwayRows.length === 0
                text: search.text.length > 0 ? qsTr("No matching visits") : qsTr(
                                                   "No History in this Space")
                color: root.colors.mutedText
                font.family: Style.font.family
                font.pixelSize: Style.font.body
            }
        }
    }
}
