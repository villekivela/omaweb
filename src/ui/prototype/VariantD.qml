// PROTOTYPE — Variant D, A and C combined. A's frame: status line at the
// bottom, hairline rules, floats that rise from it. C's ideas: the Spaces
// listed in the status line as numbered windows, a numbered sidebar tree,
// a filled cursor row in the picker. `proto.statusOnTop` flips the line to
// the top so the placement can be compared without a second variant.
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
    readonly property bool onTop: proto ? proto.statusOnTop : false
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
    component Seg: Mono {
        leftPadding: root.cw
        rightPadding: root.cw
        height: root.ch
        verticalAlignment: Text.AlignVCenter
    }
    component VRule: Rectangle {
        width: 1
        height: root.ch
        color: root.colors.separator
    }

    Rectangle {
        anchors.fill: parent
        color: colors.windowOpaque
    }

    // ------------------------------------------------------------ frame
    Rectangle {
        id: sidebar
        anchors.left: parent.left
        anchors.top: root.onTop ? statusLine.bottom : parent.top
        anchors.bottom: root.onTop ? parent.bottom : statusLine.top
        width: proto && proto.sidebarOpen ? 30 * root.cw : 0
        Behavior on width {
            NumberAnimation {
                duration: 140
                easing.type: Easing.OutCubic
            }
        }
        color: colors.sidebarOpaque
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
                        text: (index + 1) + ":" + modelData.name + "/"
                        color: modelData.color
                        height: root.ch
                        verticalAlignment: Text.AlignVCenter
                        font.bold: index === root.proto.activeSpace
                    }
                    Repeater {
                        model: root.proto.tabs.filter(t => t.space === index)
                        delegate: Item {
                            required property var modelData
                            required property int index
                            readonly property bool current: root.proto.tabs.indexOf(modelData)
                                                            === root.proto.activeTab
                            width: sidebar.width - 2 * root.cw
                            height: root.ch
                            Rectangle {
                                anchors.fill: parent
                                anchors.leftMargin: -root.cw
                                anchors.rightMargin: -root.cw
                                color: root.colors.surface
                                visible: parent.current
                            }
                            Mono {
                                anchors.fill: parent
                                verticalAlignment: Text.AlignVCenter
                                textFormat: Text.RichText
                                color: parent.current ? root.colors.text : root.colors.mutedText
                                text: "<font color='" + root.colors.mutedText + "'>" + (index + 1
                                                                                        < 10 ? " "
                                                                                               + (index
                                                                                                  + 1) : index
                                                                                               + 1) + "</font> "
                                      + (modelData.pinned ? "◆ " : "  ") + modelData.title
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
                    Item {
                        width: 1
                        height: root.ch * 0.5
                    }
                }
            }
        }
    }

    FakePage {
        id: page
        anchors.left: sidebar.right
        anchors.right: parent.right
        anchors.top: root.onTop ? statusLine.bottom : parent.top
        anchors.bottom: root.onTop ? parent.bottom : statusLine.top
        colors: root.colors
        title: proto ? proto.activeTabInfo.title : ""
        Repeater {
            model: proto && proto.mode === "HINT" ? 6 : 0
            Rectangle {
                x: 96 + (index * 137) % 500
                y: 140 + index * 62
                width: 2 * root.cw + 6
                height: root.ch - 6
                color: root.colors.accent
                Mono {
                    anchors.centerIn: parent
                    text: ["as", "df", "gh", "jk", "ls", "qw"][index]
                    color: root.colors.windowOpaque
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
        anchors.top: root.onTop ? parent.top : undefined
        anchors.bottom: root.onTop ? undefined : parent.bottom
        height: root.ch
        color: colors.sidebarOpaque
        Rectangle {
            anchors.top: root.onTop ? undefined : parent.top
            anchors.bottom: root.onTop ? parent.bottom : undefined
            width: parent.width
            height: 1
            color: root.colors.separator
        }

        Row {
            id: left
            anchors.left: parent.left
            anchors.top: parent.top
            Rectangle {
                width: modeText.implicitWidth + 2 * root.cw
                height: root.ch
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
            // The Spaces, numbered like tmux windows; the current one in its
            // own colour, the rest quiet. Few enough to always be on show.
            Repeater {
                model: root.proto ? root.proto.spaces : []
                Seg {
                    required property var modelData
                    required property int index
                    readonly property bool current: index === root.proto.activeSpace
                    text: (index + 1) + ":" + modelData.name + (current ? "*" : "")
                    color: current ? modelData.color : root.colors.mutedText
                    font.bold: current
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
            VRule {}
            Seg {
                text: root.proto ? root.proto.activeTabIndexInSpace + "/"
                                   + root.proto.currentTabs.length : ""
                color: root.colors.mutedText
                MouseArea {
                    anchors.fill: parent
                    onClicked: root.proto.leader("t")
                }
            }
            VRule {}
        }

        Item {
            anchors.left: left.right
            anchors.right: right.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            Seg {
                anchors.fill: parent
                visible: root.proto && !root.proto.promptOpen && !root.proto.findOpen
                textFormat: Text.RichText
                text: root.proto ? root.proto.activeTabInfo.title + "  <font color='"
                                   + root.colors.mutedText + "'>" + root.proto.activeTabInfo.origin
                                   + "</font>" : ""
            }
            Seg {
                anchors.fill: parent
                visible: root.proto && root.proto.promptOpen
                textFormat: Text.RichText
                text: "Clear browsing data for <b>work</b>, last hour?  <font color='"
                      + root.colors.accent + "'>[y]</font> yes  <font color='" + root.colors.accent
                      + "'>[n]</font> no"
            }
            Seg {
                anchors.fill: parent
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
            Seg {
                text: root.proto ? "⊘ " + root.proto.blocked : ""
                color: root.colors.mutedText
            }
            VRule {
                visible: root.proto && root.proto.downloads > 0
            }
            Seg {
                visible: root.proto && root.proto.downloads > 0
                text: root.proto ? "↓ " + root.proto.downloads + " " + Math.round(
                                       root.proto.downloadProgress * 100) + "%" : ""
                color: root.colors.accent
            }
            VRule {}
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
            VRule {}
            Seg {
                text: root.proto && root.proto.pendingKeys.length ? root.proto.pendingKeys : "·"
                color: root.colors.accent
                font.bold: true
            }
        }
    }

    // ------------------------------------------------------------ sky
    Rectangle {
        visible: proto && proto.whichKeyOpen
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: root.onTop ? statusLine.bottom : undefined
        anchors.bottom: root.onTop ? undefined : statusLine.top
        height: 6 * root.ch
        color: colors.overlayOpaque
        Rectangle {
            anchors.top: root.onTop ? undefined : parent.top
            anchors.bottom: root.onTop ? parent.bottom : undefined
            width: parent.width
            height: 1
            color: root.colors.separator
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

    Rectangle {
        id: picker
        visible: proto && proto.pickerOpen
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: root.onTop ? statusLine.bottom : undefined
        anchors.bottom: root.onTop ? undefined : statusLine.top
        anchors.margins: 2 * root.ch
        width: Math.min(parent.width - 8 * root.cw, 120 * root.cw)
        height: 20 * root.ch
        color: colors.overlayOpaque
        border.color: colors.border
        Column {
            anchors.fill: parent
            Item {
                width: parent.width
                height: root.ch
                Seg {
                    anchors.fill: parent
                    textFormat: Text.RichText
                    text: "<font color='" + root.colors.accent + "'>&gt;</font> " + (root.proto
                                                                                     ? root.proto.query :
                                                                                       "") + "<font color='"
                          + root.colors.accent + "'>▏</font>"
                }
                Seg {
                    anchors.right: parent.right
                    color: root.colors.mutedText
                    text: root.proto ? (root.proto.pickerSource === "tabs" ? root.proto.filteredTabs(
                                                                                 ).length
                                                                             + " tabs · all Spaces  ^a widen" :
                                                                             root.proto.pickerSource) :
                                       ""
                }
            }
            Rectangle {
                width: parent.width
                height: 1
                color: root.colors.separator
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
                                readonly property bool cur: index === root.proto.cursor
                                readonly property color dim: cur ? root.colors.windowOpaque :
                                                                   root.colors.mutedText
                                width: list.width
                                height: root.ch
                                Rectangle {
                                    anchors.fill: parent
                                    color: root.colors.accent
                                    visible: parent.cur
                                }
                                Seg {
                                    anchors.fill: parent
                                    textFormat: Text.RichText
                                    color: parent.cur ? root.colors.windowOpaque : root.colors.text
                                    text: root.proto.pickerSource === "tabs" ? "<font color='" + (
                                                                                   parent.cur
                                                                                   ? root.colors.windowOpaque :
                                                                                     root.proto.spaces[modelData.space].color)
                                                                               + "'>" + (
                                                                                   modelData.space
                                                                                   + 1) + ":"
                                                                               + root.proto.spaces[modelData.space].name
                                                                               + "</font>  " + (
                                                                                   modelData.pinned
                                                                                   ? "◆ " : "")
                                                                               + modelData.title
                                                                               + "  <font color='"
                                                                               + parent.dim + "'>"
                                                                               + modelData.origin
                                                                               + "</font>" :
                                                                               "<font color='" + (
                                                                                   parent.cur
                                                                                   ? root.colors.windowOpaque :
                                                                                     modelData.color)
                                                                               + "'>" + (index + 1)
                                                                               + ":" + modelData.name
                                                                               + "</font>  <font color='"
                                                                               + parent.dim + "'>"
                                                                               + root.proto.tabs.filter(
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
                        color: root.colors.surface
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
                            text: "space     " + (root.proto.activeSpace + 1) + ":"
                                  + root.proto.spaces[root.proto.activeSpace].name
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

    // Toasts take the corner the status line is not in.
    Column {
        anchors.top: root.onTop ? undefined : parent.top
        anchors.bottom: root.onTop ? parent.bottom : undefined
        anchors.right: parent.right
        anchors.margins: 2 * root.cw
        spacing: root.ch * 0.5
        Repeater {
            model: proto ? proto.toasts : []
            Rectangle {
                required property var modelData
                width: toastText.implicitWidth + 2 * root.cw
                height: root.ch
                color: root.colors.overlayOpaque
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
