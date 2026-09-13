// PROTOTYPE — Variant A, "Lualine". The plainest reading of the direction:
// one status line, hairline rules, floats rise from the status line with a
// single 1px border, nothing decorative.
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
    readonly property real ch: Math.ceil(cell.height * 1.6)
    readonly property color spaceColor: proto ? proto.spaces[proto.activeSpace].color :
                                                colors.accent
    readonly property color modeColor: !proto ? colors.accent : proto.mode === "PAGE"
                                                ? colors.mutedText : proto.mode === "HINT"
                                                  ? colors.urgent : spaceColor

    component Mono: Text {
        font.family: Style.font.family
        font.pixelSize: Style.font.body
        color: root.colors.text
        elide: Text.ElideRight
    }
    component Rule: Rectangle {
        color: root.colors.separator
        height: 1
    }

    // ------------------------------------------------------------ frame
    Rectangle {
        anchors.fill: parent
        color: colors.windowOpaque
    }

    // Sidebar tree, tiled: pushes the page.
    Rectangle {
        id: sidebar
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: statusLine.top
        width: proto && proto.sidebarOpen ? 30 * root.cw : 0
        Behavior on width {
            NumberAnimation {
                duration: 140
                easing.type: Easing.OutCubic
            }
        }
        color: colors.sidebar
        clip: true
        Rectangle {
            anchors.right: parent.right
            width: 1
            height: parent.height
            color: colors.separator
        }
        Column {
            x: root.cw
            y: root.ch * 0.5
            Repeater {
                model: proto ? proto.spaces : []
                delegate: Column {
                    required property var modelData
                    required property int index
                    Mono {
                        text: (index === root.proto.activeSpace ? "▾ " : "▸ ") + modelData.name
                              + "/"

                        color: modelData.color
                        height: root.ch
                        verticalAlignment: Text.AlignVCenter
                    }
                    Repeater {
                        model: root.proto.tabs.filter(t => t.space === index)
                        delegate: Mono {
                            required property var modelData
                            readonly property bool current: root.proto.tabs.indexOf(modelData)
                                                            === root.proto.activeTab
                            text: "  " + (modelData.pinned ? "◆ " : "  ") + modelData.title
                            width: sidebar.width - 2 * root.cw
                            height: root.ch
                            verticalAlignment: Text.AlignVCenter
                            color: current ? root.colors.text : root.colors.mutedText
                            Rectangle {
                                anchors.fill: parent
                                anchors.leftMargin: -root.cw
                                anchors.rightMargin: -root.cw
                                z: -1
                                color: root.colors.surface
                                visible: parent.current
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    root.proto.activeTab = root.proto.tabs.indexOf(modelData);
                                    root.proto.activeSpace = modelData.space;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    FakePage {
        id: page
        anchors.left: sidebar.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: statusLine.top
        colors: root.colors
        title: proto ? proto.activeTabInfo.title : ""
        // HINT mode: labels appear in place, no motion.
        Repeater {
            model: proto && proto.mode === "HINT" ? 6 : 0
            Rectangle {
                x: 96 + (index * 137) % 500
                y: 140 + index * 62
                width: 2 * root.cw + 6
                height: root.ch - 6
                color: root.colors.accent
                radius: 0
                Mono {
                    anchors.centerIn: parent
                    text: ["as", "df", "gh", "jk", "ls", "qw"][index]
                    color: root.colors.windowOpaqueOpaque
                    font.bold: true
                }
            }
        }
    }

    // ------------------------------------------------------------ status line
    Rectangle {
        id: statusLine
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: root.ch
        color: colors.sidebar
        Rule {
            anchors.top: parent.top
            width: parent.width
        }

        Row {
            id: left
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            Rectangle {
                width: modeText.implicitWidth + 2 * root.cw
                height: parent.height
                color: root.modeColor
                Mono {
                    id: modeText
                    anchors.centerIn: parent
                    text: root.proto ? root.proto.mode : ""
                    color: root.colors.windowOpaque
                    font.bold: true
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        root.proto.whichKeyOpen = !root.proto.whichKeyOpen;
                        root.proto.pendingKeys = root.proto.whichKeyOpen ? "SPC" : "";
                    }
                }
            }
            Mono {
                text: root.proto ? (root.proto.mode === "PAGE" ? "PRIVATE" :
                                                                 root.proto.spaces[root.proto.activeSpace].name) :
                                   ""
                leftPadding: root.cw
                rightPadding: root.cw
                height: root.ch
                verticalAlignment: Text.AlignVCenter
                color: root.spaceColor
                MouseArea {
                    anchors.fill: parent
                    onClicked: root.proto.leader("s")
                }
            }
            Rectangle {
                width: 1
                height: root.ch
                color: root.colors.separator
            }
            Mono {
                text: root.proto ? root.proto.activeTabIndexInSpace + "/"
                                   + root.proto.currentTabs.length : ""
                leftPadding: root.cw
                rightPadding: root.cw
                height: root.ch
                verticalAlignment: Text.AlignVCenter
                color: root.colors.mutedText
                MouseArea {
                    anchors.fill: parent
                    onClicked: root.proto.leader("t")
                }
            }
            Rectangle {
                width: 1
                height: root.ch
                color: root.colors.separator
            }
        }

        // Centre: the title at rest; the question or the find prompt when
        // the browser is asking something.
        Item {
            anchors.left: left.right
            anchors.right: right.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            Mono {
                anchors.fill: parent
                leftPadding: root.cw
                rightPadding: root.cw
                verticalAlignment: Text.AlignVCenter
                visible: root.proto && !root.proto.promptOpen && !root.proto.findOpen
                textFormat: Text.RichText
                text: root.proto ? root.proto.activeTabInfo.title + "  <font color='"
                                   + root.colors.mutedText + "'>" + root.proto.activeTabInfo.origin
                                   + "</font>" : ""
            }
            Mono {
                anchors.fill: parent
                leftPadding: root.cw
                verticalAlignment: Text.AlignVCenter
                visible: root.proto && root.proto.promptOpen
                textFormat: Text.RichText
                text: "Clear browsing data for <b>work</b>, last hour?  <font color='"
                      + root.colors.accent + "'>[y]</font> yes  <font color='" + root.colors.accent
                      + "'>[n]</font> no"
            }
            Mono {
                anchors.fill: parent
                leftPadding: root.cw
                verticalAlignment: Text.AlignVCenter
                visible: root.proto && root.proto.findOpen
                textFormat: Text.RichText
                text: "/screenshot<font color='" + root.colors.accent
                      + "'>▏</font>   <font color='" + root.colors.mutedText + "'>3 of 7</font>"
            }
        }

        Row {
            id: right
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            Mono {
                text: root.proto ? "⊘ " + root.proto.blocked : ""
                leftPadding: root.cw
                rightPadding: root.cw
                height: root.ch
                verticalAlignment: Text.AlignVCenter
                color: root.colors.mutedText
            }
            Rectangle {
                width: 1
                height: root.ch
                color: root.colors.separator
                visible: root.proto && root.proto.downloads > 0
            }
            Mono {
                visible: root.proto && root.proto.downloads > 0
                text: root.proto ? "↓ " + root.proto.downloads + " " + Math.round(
                                       root.proto.downloadProgress * 100) + "%" : ""
                leftPadding: root.cw
                rightPadding: root.cw
                height: root.ch
                verticalAlignment: Text.AlignVCenter
                color: root.colors.accent
            }
            Rectangle {
                width: 1
                height: root.ch
                color: root.colors.separator
            }
            // Angular loader: a row of cells filling left to right.
            Row {
                height: root.ch
                leftPadding: root.cw
                rightPadding: root.cw
                spacing: 2
                Repeater {
                    model: 8
                    Rectangle {
                        y: (root.ch - 6) / 2
                        width: root.cw * 0.6
                        height: 6
                        color: root.proto && root.proto.loading && index / 8 < root.proto.loadProgress
                               ? root.colors.accent : root.colors.surface
                    }
                }
            }
            Rectangle {
                width: 1
                height: root.ch
                color: root.colors.separator
            }
            Mono {
                text: root.proto && root.proto.pendingKeys.length ? root.proto.pendingKeys : "·"
                leftPadding: root.cw
                rightPadding: root.cw
                height: root.ch
                verticalAlignment: Text.AlignVCenter
                color: root.colors.accent
                font.bold: true
            }
        }
    }

    // ------------------------------------------------------------ sky
    // which-key: full width, rises from the status line.
    Rectangle {
        visible: proto && proto.whichKeyOpen
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: statusLine.top
        height: 6 * root.ch
        color: colors.overlay
        Rule {
            anchors.top: parent.top
            width: parent.width
        }
        Grid {
            x: 2 * root.cw
            y: root.ch * 0.5
            columns: 6
            columnSpacing: 2 * root.cw
            Repeater {
                model: root.proto ? root.proto.leaderBindings : []
                Mono {
                    required property var modelData
                    width: 22 * root.cw
                    height: root.ch
                    verticalAlignment: Text.AlignVCenter
                    textFormat: Text.RichText
                    text: "<font color='" + root.colors.accent + "'><b>" + modelData.key
                          + "</b></font> → " + modelData.label
                }
            }
        }
        Mono {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: root.cw
            text: "SPC"
            color: root.colors.mutedText
        }
    }

    // Picker: rises from the status line, bottom-anchored, prompt on top.
    Rectangle {
        id: picker
        visible: proto && proto.pickerOpen
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: statusLine.top
        anchors.bottomMargin: 2 * root.ch
        width: Math.min(parent.width - 8 * root.cw, 120 * root.cw)
        height: 20 * root.ch
        color: colors.overlay
        border.color: colors.border
        Column {
            anchors.fill: parent
            Item {
                width: parent.width
                height: root.ch
                Mono {
                    anchors.fill: parent
                    leftPadding: root.cw
                    verticalAlignment: Text.AlignVCenter
                    textFormat: Text.RichText
                    text: "<font color='" + root.colors.accent + "'>&gt;</font> " + (root.proto
                                                                                     ? root.proto.query :
                                                                                       "") + "<font color='"
                          + root.colors.accent + "'>▏</font>"
                }
                Mono {
                    anchors.right: parent.right
                    anchors.rightMargin: root.cw
                    anchors.verticalCenter: parent.verticalCenter
                    color: root.colors.mutedText
                    text: root.proto ? (root.proto.pickerSource === "tabs" ? root.proto.filteredTabs(
                                                                                 ).length
                                                                             + " tabs · all Spaces  ^a widen" :
                                                                             root.proto.pickerSource) :
                                       ""
                }
            }
            Rule {
                width: parent.width
            }
            Row {
                width: parent.width
                height: parent.height - root.ch - 1
                Item {
                    id: list
                    width: parent.width * 0.55
                    height: parent.height
                    Column {
                        width: parent.width
                        Repeater {
                            model: root.proto ? (root.proto.pickerSource === "tabs" ? root.proto.filteredTabs() :
                                                                                      root.proto.pickerSource
                                                                                      === "spaces"
                                                                                      ? root.proto.spaces :
                                                                                        []) : []
                            delegate: Item {
                                required property var modelData
                                required property int index
                                width: list.width
                                height: root.ch
                                Rectangle {
                                    anchors.fill: parent
                                    color: root.colors.surface
                                    visible: index === root.proto.cursor
                                }
                                Mono {
                                    anchors.fill: parent
                                    leftPadding: root.cw
                                    verticalAlignment: Text.AlignVCenter
                                    textFormat: Text.RichText
                                    text: root.proto.pickerSource === "tabs" ? "<font color='"
                                                                               + root.proto.spaces[modelData.space].color
                                                                               + "'>" + root.proto.spaces[modelData.space].name
                                                                               + "</font> " + (
                                                                                   modelData.pinned
                                                                                   ? "◆ " : "  ")
                                                                               + modelData.title
                                                                               + "  <font color='"
                                                                               + root.colors.mutedText
                                                                               + "'>" + modelData.origin
                                                                               + "</font>" :
                                                                               "<font color='"
                                                                               + modelData.color
                                                                               + "'>■</font> "
                                                                               + modelData.name
                                                                               + "  <font color='"
                                                                               + root.colors.mutedText
                                                                               + "'>" + root.proto.tabs.filter(
                                                                                   t => t.space
                                                                                        === index).length
                                                                               + " tabs</font>"
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: root.proto.cursor = index
                                }
                            }
                        }
                    }
                }
                Rectangle {
                    width: 1
                    height: parent.height
                    color: root.colors.separator
                }
                // Preview: the frozen thumbnail with its age, or a text card
                // for the tab on show.
                Item {
                    width: parent.width - list.width - 1
                    height: parent.height
                    readonly property var row: root.proto && root.proto.pickerSource === "tabs"
                                               ? root.proto.filteredTabs()[root.proto.cursor] : null
                    readonly property bool onShow: row && row.index === root.proto.activeTab
                    FakePage {
                        visible: parent.row && !parent.onShow
                        anchors.fill: parent
                        anchors.margins: root.cw
                        colors: root.colors
                        title: parent.row ? parent.row.title : ""
                        miniature: true
                    }
                    Rectangle {
                        visible: parent.row && !parent.onShow
                        anchors.bottom: parent.bottom
                        anchors.right: parent.right
                        anchors.margins: root.cw
                        width: ageText.implicitWidth + 2 * root.cw
                        height: root.ch
                        color: root.colors.overlay
                        Mono {
                            id: ageText
                            anchors.centerIn: parent
                            text: parent.parent.row ? "as of " + parent.parent.row.age : ""
                            color: root.colors.mutedText
                        }
                    }
                    Column {
                        visible: parent.onShow
                        x: root.cw
                        y: root.ch * 0.5
                        Mono {
                            text: parent.parent.row ? parent.parent.row.title : ""
                            font.bold: true
                        }
                        Mono {
                            text: parent.parent.row ? parent.parent.row.origin : ""
                            color: root.colors.mutedText
                        }
                        Item {
                            width: 1
                            height: root.ch * 0.5
                        }
                        Mono {
                            text: "space     " + root.proto.spaces[root.proto.activeSpace].name
                            color: root.colors.mutedText
                        }
                        Mono {
                            text: "blocked   " + root.proto.blocked + " requests"
                            color: root.colors.mutedText
                        }
                        Mono {
                            text: "state     on show, not frozen"
                            color: root.colors.mutedText
                        }
                        Mono {
                            text: "visited   just now"
                            color: root.colors.mutedText
                        }
                    }
                }
            }
        }
    }

    // Toasts: top-right, plain.
    Column {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 2 * root.cw
        spacing: root.ch * 0.5
        Repeater {
            model: proto ? proto.toasts : []
            Rectangle {
                required property var modelData
                width: toastText.implicitWidth + 2 * root.cw
                height: root.ch
                color: root.colors.overlay
                border.color: root.colors.border
                Mono {
                    id: toastText
                    anchors.centerIn: parent
                    text: modelData.text
                }
            }
        }
    }
}
