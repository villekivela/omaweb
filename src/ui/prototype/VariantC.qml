// PROTOTYPE — Variant C, "Tmux". The status line sits on top and lists the
// Spaces the way tmux lists windows, in filled blocks; floats hang from it
// like dropdowns; toasts take the opposite corner. Tests whether the
// bottom-anchored model is the right one by building the other one.
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
    component Block: Rectangle {
        property alias text: label.text
        property color fg: root.colors.text
        width: label.implicitWidth + 2 * root.cw
        height: root.ch
        Mono {
            id: label
            anchors.centerIn: parent
            color: parent.fg
        }
    }

    Rectangle {
        anchors.fill: parent
        color: colors.windowOpaque
    }

    // ------------------------------------------------------------ status line, on top
    Rectangle {
        id: statusLine
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: root.ch
        color: colors.sidebarOpaque
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: root.colors.separator
        }

        Row {
            id: left
            anchors.left: parent.left
            anchors.top: parent.top
            Block {
                text: root.proto ? root.proto.mode : ""
                color: root.modeColor
                fg: root.colors.windowOpaque
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        root.proto.whichKeyOpen = !root.proto.whichKeyOpen;
                        root.proto.pendingKeys = root.proto.whichKeyOpen ? "SPC" : "";
                    }
                }
            }
            // Spaces as tmux windows: `1:work*  2:home  3:lab`.
            Repeater {
                model: root.proto ? root.proto.spaces : []
                Block {
                    required property var modelData
                    required property int index
                    readonly property bool current: index === root.proto.activeSpace
                    text: (index + 1) + ":" + modelData.name + (current ? "*" : "")
                    color: current ? root.colors.surface : "transparent"
                    fg: current ? modelData.color : root.colors.mutedText
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
            Block {
                text: root.proto ? "⌗ " + root.proto.activeTabIndexInSpace + "/"
                                   + root.proto.currentTabs.length : ""
                color: "transparent"
                fg: root.colors.mutedText
                MouseArea {
                    anchors.fill: parent
                    onClicked: root.proto.leader("t")
                }
            }
        }
        Item {
            anchors.left: left.right
            anchors.right: right.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            Mono {
                anchors.fill: parent
                leftPadding: root.cw
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
                textFormat: Text.RichText
                visible: root.proto && !root.proto.promptOpen && !root.proto.findOpen
                text: root.proto ? "\"" + root.proto.activeTabInfo.title + "\"  <font color='"
                                   + root.colors.mutedText + "'>" + root.proto.activeTabInfo.origin
                                   + "</font>" : ""
            }
            Mono {
                anchors.fill: parent
                leftPadding: root.cw
                verticalAlignment: Text.AlignVCenter
                textFormat: Text.RichText
                visible: root.proto && root.proto.promptOpen
                text: "clear browsing data for work (last hour)? <font color='"
                      + root.colors.accent + "'>(y/n)</font>"
            }
            Mono {
                anchors.fill: parent
                leftPadding: root.cw
                verticalAlignment: Text.AlignVCenter
                textFormat: Text.RichText
                visible: root.proto && root.proto.findOpen
                text: "(search down) screenshot<font color='" + root.colors.accent
                      + "'>▏</font>  <font color='" + root.colors.mutedText + "'>3/7</font>"
            }
        }
        Row {
            id: right
            anchors.right: parent.right
            anchors.top: parent.top
            Block {
                text: root.proto ? "⊘ " + root.proto.blocked : ""
                color: "transparent"
                fg: root.colors.mutedText
            }
            Block {
                visible: root.proto && root.proto.downloads > 0
                text: root.proto ? "↓ " + root.proto.downloads + " " + Math.round(
                                       root.proto.downloadProgress * 100) + "%" : ""
                color: "transparent"
                fg: root.colors.accent
            }
            // Angular loader: a block that fills.
            Item {
                width: 10 * root.cw
                height: root.ch
                Rectangle {
                    x: root.cw
                    y: (root.ch - 8) / 2
                    width: 8 * root.cw
                    height: 8
                    color: root.colors.surface
                }
                Rectangle {
                    x: root.cw
                    y: (root.ch - 8) / 2
                    width: 8 * root.cw * (root.proto && root.proto.loading
                                          ? root.proto.loadProgress : 0)
                    height: 8
                    color: root.colors.accent
                }
            }
            Block {
                text: root.proto && root.proto.pendingKeys.length ? root.proto.pendingKeys :
                                                                    "omaweb"
                color: root.proto && root.proto.pendingKeys.length ? root.colors.accent :
                                                                     root.colors.surface
                fg: root.proto && root.proto.pendingKeys.length ? root.colors.windowOpaque :
                                                                  root.colors.mutedText
            }
        }
    }

    // ------------------------------------------------------------ frame
    Rectangle {
        id: sidebar
        anchors.left: parent.left
        anchors.top: statusLine.bottom
        anchors.bottom: parent.bottom
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
                        text: (index + 1) + ":" + modelData.name
                        color: modelData.color
                        height: root.ch
                        verticalAlignment: Text.AlignVCenter
                        font.bold: true
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
        anchors.top: statusLine.bottom
        anchors.bottom: parent.bottom
        colors: root.colors
        title: proto ? proto.activeTabInfo.title : ""
        Repeater {
            model: proto && proto.mode === "HINT" ? 6 : 0
            Rectangle {
                x: 96 + (index * 137) % 500
                y: 140 + index * 62
                width: 2 * root.cw + 6
                height: root.ch - 6
                color: root.colors.urgent
                Mono {
                    anchors.centerIn: parent
                    text: ["as", "df", "gh", "jk", "ls", "qw"][index]
                    color: root.colors.windowOpaque
                    font.bold: true
                }
            }
        }
    }

    // ------------------------------------------------------------ sky, hanging from the top
    Rectangle {
        visible: proto && proto.whichKeyOpen
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: statusLine.bottom
        height: 6 * root.ch
        color: colors.overlayOpaque
        Rectangle {
            anchors.bottom: parent.bottom
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
                    text: "<font color='" + root.colors.windowOpaque + "' style='background-color:"
                          + root.colors.accent + "'>&nbsp;" + modelData.key + "&nbsp;</font>  "
                          + modelData.label
                }
            }
        }
    }

    Rectangle {
        id: picker
        visible: proto && proto.pickerOpen
        anchors.left: sidebar.right
        anchors.right: parent.right
        anchors.top: statusLine.bottom
        height: 20 * root.ch
        color: colors.overlayOpaque
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: root.colors.separator
        }
        Row {
            anchors.fill: parent
            Item {
                id: list
                width: parent.width * 0.5
                height: parent.height
                Column {
                    width: parent.width
                    Item {
                        width: parent.width
                        height: root.ch
                        Rectangle {
                            anchors.fill: parent
                            color: root.colors.surface
                        }
                        Mono {
                            anchors.fill: parent
                            leftPadding: root.cw
                            verticalAlignment: Text.AlignVCenter
                            textFormat: Text.RichText
                            text: (root.proto ? root.proto.pickerSource : "") + " › " + (root.proto
                                                                                         ? root.proto.query :
                                                                                           "") + "<font color='"
                                  + root.colors.accent + "'>▏</font>"
                        }
                        Mono {
                            anchors.right: parent.right
                            anchors.rightMargin: root.cw
                            anchors.verticalCenter: parent.verticalCenter
                            color: root.colors.mutedText
                            text: root.proto ? root.proto.filteredTabs().length + "  ^a all spaces" :
                                               ""
                        }
                    }
                    Repeater {
                        model: root.proto ? (root.proto.pickerSource === "tabs" ? root.proto.filteredTabs(
                                                                                      ) : root.proto.pickerSource
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
                                color: root.colors.accent
                                visible: index === root.proto.cursor
                            }
                            Mono {
                                anchors.fill: parent
                                leftPadding: root.cw
                                verticalAlignment: Text.AlignVCenter
                                textFormat: Text.RichText
                                readonly property bool cur: index === root.proto.cursor
                                readonly property color dim: cur ? root.colors.windowOpaque :
                                                                   root.colors.mutedText
                                color: cur ? root.colors.windowOpaque : root.colors.text
                                text: root.proto.pickerSource === "tabs" ? "<font color='" + dim
                                                                           + "'>" + (
                                                                               modelData.space + 1)
                                                                           + ":" + root.proto.spaces[modelData.space].name
                                                                           + "</font>  " + (
                                                                               modelData.pinned
                                                                               ? "◆ " : "")
                                                                           + modelData.title
                                                                           + "  <font color='"
                                                                           + dim + "'>"
                                                                           + modelData.origin
                                                                           + "</font>" : (index
                                                                                          + 1) + ":"
                                                                           + modelData.name
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
                    anchors.margins: root.ch
                    colors: root.colors
                    title: parent.row ? parent.row.title : ""
                    miniature: true
                }
                Rectangle {
                    visible: parent.row && !parent.onShow
                    anchors.top: parent.top
                    anchors.left: parent.left
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
                        text: parent.parent.row ? "\"" + parent.parent.row.title + "\"" : ""
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
                        text: "space=" + root.proto.spaces[root.proto.activeSpace].name
                        + " blocked=" + root.proto.blocked + " state=on-show visited=now"
                        color: root.colors.mutedText
                    }
                }
            }
        }
    }

    // Toasts: bottom-right, since the top belongs to the status line.
    Column {
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.margins: 2 * root.cw
        spacing: root.ch * 0.5
        Repeater {
            model: proto ? proto.toasts : []
            Rectangle {
                required property var modelData
                width: toastText.implicitWidth + 2 * root.cw
                height: root.ch
                color: root.colors.accent
                Mono {
                    id: toastText
                    anchors.centerIn: parent
                    text: modelData.text
                    color: root.colors.windowOpaque
                }
            }
        }
    }
}
