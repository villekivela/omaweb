// PROTOTYPE — Variant E, "Phosphor". The browser as a terminal in a bezel:
// the page is the screen, the chrome is a prompt line with a blinking block
// cursor, text glows in the theme's accent, scanlines run over the chrome
// and never over the page. Tabs are a `ls`-style listing summoned over the
// screen. Everything is uppercase, everything is the theme's colours.
import QtQuick
import qs.Commons

Item {
    id: root
    property var proto
    property var colors

    TextMetrics {
        id: cell
        font.family: Style.font.family
        font.pixelSize: Style.font.body
        text: "M"
    }
    readonly property real cw: cell.advanceWidth
    readonly property real ch: Math.ceil(cell.height * 1.5)
    readonly property color spaceColor: proto ? proto.spaces[proto.activeSpace].color :
                                                colors.accent
    readonly property real bezel: 3 * root.ch

    // Glowing text: the same string drawn three times, the two under it in
    // the accent at low alpha and slightly larger, then the text itself.
    component Glow: Item {
        property string text
        property color tint: root.spaceColor
        property real size: Style.font.body
        property bool bold: false
        implicitWidth: top.implicitWidth
        implicitHeight: top.implicitHeight
        Text {
            anchors.centerIn: top
            text: parent.text
            color: parent.tint
            opacity: 0.18
            font.family: Style.font.family
            font.pixelSize: parent.size * 1.08
            font.bold: parent.bold
            font.letterSpacing: 1
        }
        Text {
            anchors.centerIn: top
            text: parent.text
            color: parent.tint
            opacity: 0.35
            font.family: Style.font.family
            font.pixelSize: parent.size * 1.03
            font.bold: parent.bold
            font.letterSpacing: 1
        }
        Text {
            id: top
            text: parent.text
            color: parent.tint
            font.family: Style.font.family
            font.pixelSize: parent.size
            font.bold: parent.bold
            font.letterSpacing: 1
        }
    }
    component Scan: Item {
        clip: true
        Repeater {
            model: Math.max(0, Math.floor(parent.height / 3))
            Rectangle {
                y: index * 3
                width: parent.width
                height: 1
                color: root.colors.separator
                opacity: 0.55
            }
        }
    }
    component Mono: Text {
        font.family: Style.font.family
        font.pixelSize: Style.font.body
        color: root.colors.text
        font.letterSpacing: 1
    }

    Rectangle {
        anchors.fill: parent
        color: colors.sidebarOpaque
    }
    Scan {
        anchors.fill: parent
    }

    // ------------------------------------------------------------ the screen
    Rectangle {
        id: screen
        anchors.fill: parent
        anchors.leftMargin: root.bezel
        anchors.rightMargin: root.bezel
        anchors.topMargin: 2.2 * root.ch
        anchors.bottomMargin: 2.2 * root.ch
        radius: 14
        color: colors.windowOpaque
        border.color: root.spaceColor
        border.width: 1
        clip: true
        // Inner glow off the frame, in the Space colour.
        Rectangle {
            anchors.fill: parent
            anchors.margins: 1
            radius: 13
            color: "transparent"
            border.color: root.spaceColor
            border.width: 3
            opacity: 0.12
        }
        FakePage {
            anchors.fill: parent
            anchors.margins: 4
            colors: root.colors
            title: root.proto ? root.proto.activeTabInfo.title : ""
        }
        Repeater {
            model: root.proto && root.proto.mode === "HINT" ? 6 : 0
            Item {
                x: 100 + (index * 137) % 500
                y: 140 + index * 62
                width: 3 * root.cw + 4
                height: root.ch - 4
                Rectangle {
                    anchors.fill: parent
                    color: root.colors.accent
                    opacity: 0.25
                }
                Glow {
                    anchors.centerIn: parent
                    text: ["AS", "DF", "GH", "JK", "LS", "QW"][index]
                    tint: root.colors.accent
                    bold: true
                }
            }
        }
    }

    // ------------------------------------------------------------ prompt line, above the screen
    Item {
        id: promptLine
        anchors.left: screen.left
        anchors.right: screen.right
        anchors.bottom: screen.top
        height: 2.2 * root.ch
        Row {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            spacing: root.cw
            Glow {
                text: root.proto ? root.proto.spaces[root.proto.activeSpace].name.toUpperCase() : ""
                bold: true
                MouseArea {
                    anchors.fill: parent
                    onClicked: root.proto.leader("s")
                }
            }
            Mono {
                text: "@OMAWEB"
                color: root.colors.mutedText
            }
            Glow {
                text: root.proto ? root.proto.mode === "NORMAL" ? "❯" : root.proto.mode : ""
                tint: root.proto && root.proto.mode === "HINT" ? root.colors.urgent :
                                                                 root.spaceColor

                bold: true
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        root.proto.whichKeyOpen = !root.proto.whichKeyOpen;
                        root.proto.pendingKeys = root.proto.whichKeyOpen ? "SPC" : "";
                    }
                }
            }
            Mono {
                text: root.proto ? (root.proto.promptOpen
                                    ? "CLEAR BROWSING DATA FOR WORK, LAST HOUR? [Y/N]" :
                                      root.proto.findOpen ? "/SCREENSHOT   3 OF 7" :
                                                            root.proto.activeTabInfo.origin + "  "
                                                            + root.proto.activeTabInfo.title.toUpperCase(
                                                                )) : ""
                color: root.colors.text
            }
            // The block cursor. It blinks; this is the one ambient motion.
            Rectangle {
                width: root.cw
                height: root.ch * 0.8
                anchors.verticalCenter: parent.verticalCenter
                color: root.spaceColor
                SequentialAnimation on opacity {
                    loops: Animation.Infinite
                    NumberAnimation {
                        to: 0
                        duration: 0
                    }
                    PauseAnimation {
                        duration: 500
                    }
                    NumberAnimation {
                        to: 1
                        duration: 0
                    }
                    PauseAnimation {
                        duration: 500
                    }
                }
            }
        }
        Row {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2 * root.cw
            Mono {
                text: root.proto ? "TAB " + root.proto.activeTabIndexInSpace + "/"
                                   + root.proto.currentTabs.length : ""
                color: root.colors.mutedText
                MouseArea {
                    anchors.fill: parent
                    onClicked: root.proto.leader("t")
                }
            }
            Mono {
                text: root.proto ? "BLK " + root.proto.blocked : ""
                color: root.colors.mutedText
            }
            Glow {
                visible: root.proto && root.proto.downloads > 0
                text: root.proto ? "DL " + Math.round(root.proto.downloadProgress * 100) + "%" : ""
                tint: root.colors.accent
            }
            Mono {
                text: root.proto && root.proto.loading ? "█".repeat(Math.round(
                                                                        root.proto.loadProgress
                                                                        * 8)) + "░".repeat(8
                                                                                           - Math.round(
                                                                                               root.proto.loadProgress
                                                                                               * 8)) : "░░░░░░░░"
                color: root.proto && root.proto.loading ? root.spaceColor : root.colors.surface
            }
            Glow {
                text: root.proto && root.proto.pendingKeys.length ? root.proto.pendingKeys : ""
                tint: root.colors.accent
                bold: true
            }
        }
    }

    // ------------------------------------------------------------ below the screen: the Spaces as function keys
    Row {
        anchors.left: screen.left
        anchors.top: screen.bottom
        anchors.topMargin: root.ch * 0.5
        spacing: root.cw
        Repeater {
            model: root.proto ? root.proto.spaces : []
            Item {
                required property var modelData
                required property int index
                readonly property bool current: index === root.proto.activeSpace
                width: keyText.implicitWidth + 3 * root.cw
                height: root.ch * 1.3
                Rectangle {
                    anchors.fill: parent
                    radius: 3
                    color: parent.current ? modelData.color : "transparent"
                    opacity: parent.current ? 0.2 : 1
                    border.color: modelData.color
                    border.width: 1
                }
                Mono {
                    id: keyText
                    anchors.centerIn: parent
                    text: "F" + (index + 1) + " " + modelData.name.toUpperCase()
                    color: parent.current ? modelData.color : root.colors.mutedText
                    font.bold: parent.current
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        root.proto.activeSpace = index;
                        root.proto.activeTab = root.proto.tabs.indexOf(root.proto.tabs.filter(t
                                                                                              => t.space
                                                                                                 === index)[0]);
                    }
                }
            }
        }
    }
    Glow {
        anchors.right: screen.right
        anchors.top: screen.bottom
        anchors.topMargin: root.ch * 0.6
        text: "OMAWEB ∷ " + (root.proto && root.proto.sidebarOpen ? "TREE" : "SCREEN 1")
        tint: root.colors.mutedText
        size: Style.font.caption
    }

    // ------------------------------------------------------------ overlays on the screen
    // The tab listing: `ls -l` over the page, dimming it.
    Item {
        visible: proto && (proto.pickerOpen || proto.sidebarOpen)
        anchors.fill: screen
        Rectangle {
            anchors.fill: parent
            radius: 14
            color: root.colors.windowOpaque
            opacity: 0.92
        }
        Scan {
            anchors.fill: parent
        }
        Column {
            x: 3 * root.cw
            y: 1.5 * root.ch
            spacing: 0
            Row {
                spacing: root.cw
                Glow {
                    text: "❯"
                    bold: true
                }
                Glow {
                    text: root.proto ? "LS --ALL-SPACES " + (root.proto.query.length ? "| GREP "
                                                                                       + root.proto.query.toUpperCase(
                                                                                           ) : "") : ""
                    tint: root.colors.text
                }
                Rectangle {
                    width: root.cw
                    height: root.ch * 0.8
                    color: root.spaceColor
                }
            }
            Item {
                width: 1
                height: root.ch * 0.7
            }
            Repeater {
                model: root.proto ? root.proto.filteredTabs() : []
                delegate: Item {
                    required property var modelData
                    required property int index
                    readonly property bool cur: root.proto.pickerOpen && index === root.proto.cursor
                    readonly property bool onShow: modelData.index === root.proto.activeTab
                    width: screen.width - 6 * root.cw
                    height: root.ch
                    Rectangle {
                        anchors.fill: parent
                        anchors.leftMargin: -root.cw
                        color: root.spaceColor
                        opacity: 0.18
                        visible: parent.cur
                    }
                    Row {
                        spacing: root.cw
                        height: root.ch
                        Mono {
                            text: (modelData.pinned ? "p" : "-") + (parent.parent.onShow ? "rwx" :
                                                                                           "r--") + "  "
                            color: root.colors.mutedText
                            width: 8 * root.cw
                        }
                        Mono {
                            text: root.proto.spaces[modelData.space].name.toUpperCase()
                            color: root.proto.spaces[modelData.space].color
                            width: 6 * root.cw
                        }
                        Mono {
                            text: modelData.age.padEnd(18).toUpperCase()
                            color: root.colors.mutedText
                            width: 20 * root.cw
                        }
                        Glow {
                            text: modelData.title.toUpperCase()
                            tint: parent.parent.cur ? root.spaceColor : root.colors.text
                            bold: parent.parent.cur
                        }
                        Mono {
                            text: "→ " + modelData.origin
                            color: root.colors.mutedText
                        }
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            root.proto.activeTab = modelData.index;
                            root.proto.activeSpace = modelData.space;
                            root.proto.closeAll();
                        }
                    }
                }
            }
        }
        // The frozen picture: bottom-right, like a preview pane in a file manager.
        Item {
            readonly property var row: root.proto && root.proto.pickerOpen ? root.proto.filteredTabs(
                                                                                 )[root.proto.cursor] :
                                                                             null
            visible: row && row.index !== root.proto.activeTab
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 2 * root.ch
            width: 44 * root.cw
            height: 12 * root.ch
            FakePage {
                anchors.fill: parent
                colors: root.colors
                title: parent.row ? parent.row.title : ""
                miniature: true
                opacity: 0.85
            }
            Rectangle {
                anchors.fill: parent
                color: "transparent"
                border.color: root.spaceColor
                radius: 4
            }
            Scan {
                anchors.fill: parent
            }
            Glow {
                anchors.left: parent.left
                anchors.top: parent.bottom
                anchors.topMargin: 4
                text: parent.row ? "AS OF " + parent.row.age.toUpperCase() : ""
                tint: root.colors.mutedText
                size: Style.font.caption
            }
        }
    }

    // which-key: a `man` page sliding over the bottom of the screen.
    Item {
        visible: proto && proto.whichKeyOpen
        anchors.left: screen.left
        anchors.right: screen.right
        anchors.bottom: screen.bottom
        height: 7 * root.ch
        Rectangle {
            anchors.fill: parent
            color: root.colors.windowOpaque
            opacity: 0.94
        }
        Rectangle {
            anchors.top: parent.top
            width: parent.width
            height: 1
            color: root.spaceColor
            opacity: 0.6
        }
        Scan {
            anchors.fill: parent
        }
        Glow {
            x: 3 * root.cw
            y: root.ch * 0.6
            text: "OMAWEB(1)                        KEYS                        OMAWEB(1)"
            tint: root.colors.mutedText
        }
        Grid {
            x: 3 * root.cw
            y: 2.2 * root.ch
            columns: 6
            columnSpacing: 2 * root.cw
            Repeater {
                model: root.proto ? root.proto.leaderBindings : []
                Row {
                    required property var modelData
                    width: 22 * root.cw
                    height: root.ch
                    spacing: root.cw
                    Glow {
                        text: modelData.key.toUpperCase()
                        bold: true
                        width: 4 * root.cw
                    }
                    Mono {
                        text: modelData.label.toUpperCase()
                        color: root.colors.text
                    }
                }
            }
        }
    }

    // Toasts: a line printed under the prompt, like `wall`.
    Column {
        anchors.right: screen.right
        anchors.top: screen.top
        anchors.margins: root.ch
        Repeater {
            model: proto ? proto.toasts : []
            Item {
                required property var modelData
                width: t.implicitWidth + 2 * root.cw
                height: root.ch * 1.3
                Rectangle {
                    anchors.fill: parent
                    color: root.colors.windowOpaque
                    opacity: 0.9
                    border.color: root.colors.accent
                    radius: 3
                }
                Glow {
                    id: t
                    anchors.centerIn: parent
                    text: "*** " + modelData.text.toUpperCase() + " ***"
                    tint: root.colors.accent
                }
            }
        }
    }
}
