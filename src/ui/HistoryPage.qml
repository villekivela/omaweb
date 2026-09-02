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
    property Item pageSource: null
    property var rows: []

    signal closed()

    visible: open
    color: "transparent"
    focus: open

    function refresh() {
        rows = browser ? browser.history(search.text) : []
    }

    function origin(address) {
        return String(address).replace(/^(https?:\/\/[^/]+).*$/, "$1")
    }

    onOpenChanged: if (open) {
        search.text = ""
        refresh()
        search.forceActiveFocus()
    }

    Connections {
        target: root.browser
        function onActiveSpaceChanged() { if (root.open) root.refresh() }
    }

    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Escape) {
            root.closed()
            event.accepted = true
        }
    }

    PageBackdrop {
        anchors.fill: parent
        source: root.pageSource
        tint: root.colors.sheet
    }

    Column {
        anchors.fill: parent
        anchors.margins: 48
        spacing: 18

        Item {
            width: parent.width
            height: historyEyebrow.height + Style.spacing.md + historyHeading.height

            Text {
                id: historyEyebrow
                objectName: "historyEyebrow"
                anchors.left: parent.left
                anchors.top: parent.top
                text: (root.browser ? root.browser.activeSpaceName : "") + " · esc closes"
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
                text: "History"
                color: root.colors.text
                font.family: Style.font.family
                font.pixelSize: Style.font.display
                Accessible.role: Accessible.Heading
                Accessible.name: "History"
            }

            ChromeButton {
                id: closeButton
                objectName: "closeHistoryButton"
                anchors.right: parent.right
                anchors.verticalCenter: historyHeading.verticalCenter
                width: 30
                height: 30
                icon: "close"
                accessibleName: "Close History"
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
                placeholder: "search this Space"
                accessibleName: "Search History"
                onTextChanged: root.refresh()
            }

            ActionButton {
                id: clearAll
                objectName: "clearHistoryAllButton"
                colors: root.colors
                label: "Clear this Space"
                destructive: true
                onClicked: {
                    root.browser.deleteHistorySince(0)
                    root.refresh()
                }
            }
        }

        Row {
            spacing: 8
            ActionButton {
                colors: root.colors
                label: "Delete last hour"
                destructive: true
                onClicked: {
                    root.browser.deleteHistorySince(Date.now() - 3600000)
                    root.refresh()
                }
            }
            ActionButton {
                colors: root.colors
                label: "Delete last day"
                destructive: true
                onClicked: {
                    root.browser.deleteHistorySince(Date.now() - 86400000)
                    root.refresh()
                }
            }
            ActionButton {
                colors: root.colors
                label: "Delete last week"
                destructive: true
                onClicked: {
                    root.browser.deleteHistorySince(Date.now() - 604800000)
                    root.refresh()
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
                        label: "Origin"
                        accessibleName: "Delete visits to " + root.origin(modelData.url)
                        onClicked: {
                            root.browser.deleteHistoryOrigin(modelData.url)
                            root.refresh()
                        }
                    }
                    ActionButton {
                        colors: root.colors
                        label: "Delete"
                        destructive: true
                        accessibleName: "Delete this visit"
                        onClicked: {
                            root.browser.deleteHistoryVisit(modelData.id)
                            root.refresh()
                        }
                    }
                }
            }

            Text {
                anchors.centerIn: parent
                visible: historyList.count === 0
                text: search.text.length > 0 ? "No matching visits" : "No History in this Space"
                color: root.colors.mutedText
                font.family: Style.font.family
                font.pixelSize: Style.font.body
            }
        }
    }
}
