// PROTOTYPE — Variant B, "HUD". The synthwave attempt within the rules:
// corner brackets instead of full borders, a ruled ground behind floats, a
// framed page in the Space's colour, tracked titles, an angular loader.
// No glow, no gradient, no colour the theme did not name.
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
    component Tracked: Text {
        font.family: Style.font.family
        font.pixelSize: Style.font.caption
        font.letterSpacing: 2
        font.capitalization: Font.AllUppercase
        color: root.colors.mutedText
    }
    // Four corner brackets: the HUD's border.
    component Brackets: Item {
        property color tint: root.colors.border
        property int arm: 10
        Rectangle {
            x: 0
            y: 0
            width: parent.arm
            height: 1
            color: parent.tint
        }
        Rectangle {
            x: 0
            y: 0
            width: 1
            height: parent.arm
            color: parent.tint
        }
        Rectangle {
            x: parent.width - parent.arm
            y: 0
            width: parent.arm
            height: 1
            color: parent.tint
        }
        Rectangle {
            x: parent.width - 1
            y: 0
            width: 1
            height: parent.arm
            color: parent.tint
        }
        Rectangle {
            x: 0
            y: parent.height - 1
            width: parent.arm
            height: 1
            color: parent.tint
        }
        Rectangle {
            x: 0
            y: parent.height - parent.arm
            width: 1
            height: parent.arm
            color: parent.tint
        }
        Rectangle {
            x: parent.width - parent.arm
            y: parent.height - 1
            width: parent.arm
            height: 1
            color: parent.tint
        }
        Rectangle {
            x: parent.width - 1
            y: parent.height - parent.arm
            width: 1
            height: parent.arm
            color: parent.tint
        }
    }
    // The ruled ground: a faint cell grid at separator strength, floats only.
    component Ruled: Item {
        clip: true
        Repeater {
            model: Math.ceil(parent.width / (4 * root.cw))
            Rectangle {
                x: index * 4 * root.cw
                width: 1
                height: parent.height
                color: root.colors.separator
                opacity: 0.5
            }
        }
        Repeater {
            model: Math.ceil(parent.height / root.ch)
            Rectangle {
                y: index * root.ch
                height: 1
                width: parent.width
                color: root.colors.separator
                opacity: 0.5
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: colors.windowOpaque
    }

    // ------------------------------------------------------------ frame
    Item {
        id: sidebar
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: statusLine.top
        width: proto && proto.sidebarOpen ? 32 * root.cw : 0
        Behavior on width {
            NumberAnimation {
                duration: 140
                easing.type: Easing.OutCubic
            }
        }
        clip: true
        Rectangle {
            anchors.fill: parent
            color: colors.sidebarOpaque
        }
        Tracked {
            x: 2 * root.cw
            y: root.ch * 0.6
            text: "spaces"
        }
        Column {
            x: 2 * root.cw
            y: 2 * root.ch
            Repeater {
                model: proto ? proto.spaces : []
                delegate: Column {
                    required property var modelData
                    required property int index
                    Row {
                        height: root.ch
                        spacing: root.cw
                        Rectangle {
                            y: (root.ch - 8) / 2
                            width: 8
                            height: 8
                            color: modelData.color
                            rotation: 45
                        }
                        Mono {
                            text: modelData.name
                            color: modelData.color
                            height: root.ch
                            verticalAlignment: Text.AlignVCenter
                            font.bold: index === root.proto.activeSpace
                        }
                        Mono {
                            text: root.proto.tabs.filter(t => t.space === index).length
                            color: root.colors.mutedText
                            height: root.ch
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                    Repeater {
                        model: root.proto.tabs.filter(t => t.space === index)
                        delegate: Item {
                            required property var modelData
                            readonly property bool current: root.proto.tabs.indexOf(modelData)
                                                            === root.proto.activeTab
                            width: sidebar.width - 4 * root.cw
                            height: root.ch
                            Rectangle {
                                x: 3
                                width: 1
                                height: parent.height
                                color: root.colors.separator
                            }
                            Rectangle {
                                x: 0
                                y: 0
                                width: 7
                                height: parent.height
                                color: parent.parent.modelData.color
                                visible: parent.current
                            }
                            Mono {
                                x: 2 * root.cw
                                width: parent.width - 2 * root.cw
                                height: root.ch
                                verticalAlignment: Text.AlignVCenter
                                text: (modelData.pinned ? "◆ " : "") + modelData.title
                                color: parent.current ? root.colors.text : root.colors.mutedText
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

    // The page sits in a one-cell frame drawn in the Space's colour: the
    // window border Hyprland draws, continued inside.
    Item {
        id: frame
        anchors.left: sidebar.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: statusLine.top
        FakePage {
            id: page
            anchors.fill: parent
            anchors.margins: 6
            colors: root.colors
            title: proto ? proto.activeTabInfo.title : ""
            Repeater {
                model: proto && proto.mode === "HINT" ? 6 : 0
                Item {
                    x: 96 + (index * 137) % 500
                    y: 140 + index * 62
                    width: 2 * root.cw + 10
                    height: root.ch - 4
                    Rectangle {
                        anchors.fill: parent
                        color: root.colors.overlayOpaque
                    }
                    Brackets {
                        anchors.fill: parent
                        tint: root.colors.accent
                        arm: 5
                    }
                    Mono {
                        anchors.centerIn: parent
                        text: ["as", "df", "gh", "jk", "ls", "qw"][index]
                        color: root.colors.accent
                        font.bold: true
                    }
                }
            }
        }
        Brackets {
            anchors.fill: parent
            anchors.margins: 2
            tint: root.spaceColor
            arm: 18
        }
    }

    // ------------------------------------------------------------ status line
    Rectangle {
        id: statusLine
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: root.ch
        color: colors.sidebarOpaque
        Rectangle {
            anchors.top: parent.top
            width: parent.width
            height: 1
            color: root.spaceColor
            opacity: 0.6
        }

        Row {
            id: left
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            leftPadding: root.cw
            Mono {
                text: "["
                color: root.modeColor
                height: root.ch
                verticalAlignment: Text.AlignVCenter
            }
            Mono {
                text: root.proto ? " " + root.proto.mode + " " : ""
                color: root.modeColor
                font.bold: true
                height: root.ch
                verticalAlignment: Text.AlignVCenter
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        root.proto.whichKeyOpen = !root.proto.whichKeyOpen;
                        root.proto.pendingKeys = root.proto.whichKeyOpen ? "SPC" : "";
                    }
                }
            }
            Mono {
                text: "]"
                color: root.modeColor
                height: root.ch
                verticalAlignment: Text.AlignVCenter
            }
            Mono {
                text: root.proto ? "  " + root.proto.spaces[root.proto.activeSpace].name : ""
                color: root.spaceColor
                height: root.ch
                verticalAlignment: Text.AlignVCenter
                MouseArea {
                    anchors.fill: parent
                    onClicked: root.proto.leader("s")
                }
            }
            Mono {
                text: root.proto ? "  " + root.proto.activeTabIndexInSpace + "∕"
                                   + root.proto.currentTabs.length : ""
                color: root.colors.mutedText
                height: root.ch
                verticalAlignment: Text.AlignVCenter
                MouseArea {
                    anchors.fill: parent
                    onClicked: root.proto.leader("t")
                }
            }
            Mono {
                text: "  ┃  "
                color: root.colors.separator
                height: root.ch
                verticalAlignment: Text.AlignVCenter
            }
        }
        Item {
            anchors.left: left.right
            anchors.right: right.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            Mono {
                anchors.fill: parent
                verticalAlignment: Text.AlignVCenter
                textFormat: Text.RichText
                visible: root.proto && !root.proto.promptOpen && !root.proto.findOpen
                text: root.proto ? root.proto.activeTabInfo.title + "  <font color='"
                                   + root.colors.mutedText + "'>⟨"
                                   + root.proto.activeTabInfo.origin + "⟩</font>" : ""
            }
            Mono {
                anchors.fill: parent
                verticalAlignment: Text.AlignVCenter
                textFormat: Text.RichText
                visible: root.proto && root.proto.promptOpen
                text: "<font color='" + root.colors.accent
                      + "'>?</font> clear browsing data for <b>work</b>, last hour   <font color='"
                      + root.colors.accent + "'>y</font>·yes  <font color='" + root.colors.accent
                      + "'>n</font>·no"
            }
            Mono {
                anchors.fill: parent
                verticalAlignment: Text.AlignVCenter
                textFormat: Text.RichText
                visible: root.proto && root.proto.findOpen
                text: "<font color='" + root.colors.accent + "'>/</font>screenshot<font color='"
                      + root.colors.accent + "'>▏</font>   <font color='" + root.colors.mutedText
                      + "'>[3/7]</font>"
            }
        }
        Row {
            id: right
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            rightPadding: root.cw
            spacing: 2 * root.cw
            Mono {
                text: root.proto ? "⊘" + root.proto.blocked : ""
                color: root.colors.mutedText
                height: root.ch
                verticalAlignment: Text.AlignVCenter
            }
            Mono {
                visible: root.proto && root.proto.downloads > 0
                text: root.proto ? "↓" + root.proto.downloads + " " + Math.round(
                                       root.proto.downloadProgress * 100) + "%" : ""
                color: root.colors.accent
                height: root.ch
                verticalAlignment: Text.AlignVCenter
            }
            // Angular loader: chevrons lighting up.
            Row {
                height: root.ch
                Repeater {
                    model: 6
                    Mono {
                        text: "›"
                        height: root.ch
                        verticalAlignment: Text.AlignVCenter
                        color: root.proto && root.proto.loading && index / 6 < root.proto.loadProgress
                               ? root.colors.accent : root.colors.surface
                    }
                }
            }
            Mono {
                text: root.proto && root.proto.pendingKeys.length ? "⌥ " + root.proto.pendingKeys :
                                                                    "⌥"

                color: root.proto && root.proto.pendingKeys.length ? root.colors.accent :
                                                                     root.colors.surface
                height: root.ch
                verticalAlignment: Text.AlignVCenter
            }
        }
    }

    // ------------------------------------------------------------ sky
    Item {
        visible: proto && proto.whichKeyOpen
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: statusLine.top
        height: 7 * root.ch
        Rectangle {
            anchors.fill: parent
            color: root.colors.overlayOpaque
        }
        Ruled {
            anchors.fill: parent
        }
        Brackets {
            anchors.fill: parent
            anchors.margins: 4
            tint: root.colors.border
            arm: 14
        }
        Tracked {
            x: 3 * root.cw
            y: root.ch * 0.6
            text: "leader ▸ "
        }
        Grid {
            x: 3 * root.cw
            y: 2 * root.ch
            columns: 6
            columnSpacing: 2 * root.cw
            Repeater {
                model: root.proto ? root.proto.leaderBindings : []
                Item {
                    required property var modelData
                    width: 22 * root.cw
                    height: root.ch
                    Rectangle {
                        width: keyText.implicitWidth + root.cw
                        height: root.ch - 6
                        y: 3
                        color: root.colors.surface
                        Mono {
                            id: keyText
                            anchors.centerIn: parent
                            text: modelData.key
                            color: root.colors.accent
                            font.bold: true
                        }
                    }
                    Mono {
                        x: keyText.implicitWidth + 2 * root.cw
                        height: root.ch
                        verticalAlignment: Text.AlignVCenter
                        text: modelData.label
                    }
                }
            }
        }
    }

    Item {
        id: picker
        visible: proto && proto.pickerOpen
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: statusLine.top
        anchors.bottomMargin: 2 * root.ch
        width: Math.min(parent.width - 8 * root.cw, 124 * root.cw)
        height: 21 * root.ch
        Rectangle {
            anchors.fill: parent
            color: root.colors.overlayOpaque
        }
        Ruled {
            anchors.fill: parent
        }
        Brackets {
            anchors.fill: parent
            tint: root.colors.accent
            arm: 16
        }
        Tracked {
            x: 2 * root.cw
            y: -root.ch * 0.9
            text: root.proto ? (root.proto.pickerSource === "tabs" ? "tabs ∙ all spaces" :
                                                                     root.proto.pickerSource) : ""
        }
        Tracked {
            anchors.right: parent.right
            anchors.rightMargin: 2 * root.cw
            y: -root.ch * 0.9
            text: root.proto ? root.proto.filteredTabs().length + " results ∙ ^a widen ∙ ⏎ open" :
                               ""
        }
        Column {
            anchors.fill: parent
            anchors.margins: root.ch * 0.5
            Item {
                width: parent.width
                height: root.ch
                Mono {
                    anchors.fill: parent
                    leftPadding: root.cw
                    verticalAlignment: Text.AlignVCenter
                    textFormat: Text.RichText
                    text: "<font color='" + root.colors.accent + "'>❯</font> " + (root.proto
                                                                                  ? root.proto.query :
                                                                                    "") + "<font color='"
                          + root.colors.accent + "'>▏</font>"
                }
                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: root.colors.accent
                    opacity: 0.5
                }
            }
            Item {
                width: 1
                height: root.ch * 0.5
            }
            Row {
                width: parent.width
                height: parent.height - root.ch * 1.5
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
                                Rectangle {
                                    width: 3
                                    height: parent.height
                                    color: root.colors.accent
                                    visible: index === root.proto.cursor
                                }
                                Mono {
                                    anchors.fill: parent
                                    leftPadding: root.cw * 1.5
                                    verticalAlignment: Text.AlignVCenter
                                    textFormat: Text.RichText
                                    text: root.proto.pickerSource === "tabs" ? "<font color='"
                                                                               + root.proto.spaces[modelData.space].color
                                                                               + "'>" + (
                                                                                   modelData.pinned
                                                                                   ? "◆" : "▪")
                                                                               + "</font> "
                                                                               + modelData.title
                                                                               + "  <font color='"
                                                                               + root.colors.mutedText
                                                                               + "'>" + modelData.origin
                                                                               + "</font>" :
                                                                               "<font color='"
                                                                               + modelData.color
                                                                               + "'>◆</font> "
                                                                               + modelData.name
                                }
                                Mono {
                                    anchors.right: parent.right
                                    anchors.rightMargin: root.cw
                                    anchors.verticalCenter: parent.verticalCenter
                                    color: root.proto.spaces[root.proto.pickerSource === "tabs"
                                                             ? modelData.space : index].color
                                    font.pixelSize: Style.font.caption
                                    text: root.proto.pickerSource === "tabs"
                                          ? root.proto.spaces[modelData.space].name : ""
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: root.proto.cursor = index
                                }
                            }
                        }
                    }
                }
                Item {
                    width: 2 * root.cw
                    height: 1
                }
                Item {
                    width: parent.width - list.width - 2 * root.cw
                    height: parent.height
                    readonly property var row: root.proto && root.proto.pickerSource === "tabs"
                                               ? root.proto.filteredTabs()[root.proto.cursor] : null
                    readonly property bool onShow: row && row.index === root.proto.activeTab
                    Item {
                        visible: parent.row && !parent.onShow
                        anchors.fill: parent
                        FakePage {
                            anchors.fill: parent
                            anchors.margins: 6
                            colors: root.colors
                            title: parent.parent.row ? parent.parent.row.title : ""
                            miniature: true
                        }
                        Brackets {
                            anchors.fill: parent
                            tint: root.colors.border
                            arm: 10
                        }
                        Tracked {
                            anchors.bottom: parent.bottom
                            anchors.right: parent.right
                            anchors.margins: root.cw * 1.5
                            text: parent.parent.row ? "as of " + parent.parent.row.age : ""
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
                            text: parent.parent.row ? "⟨" + parent.parent.row.origin + "⟩" : ""
                            color: root.colors.mutedText
                        }
                        Item {
                            width: 1
                            height: root.ch * 0.5
                        }
                        Tracked {
                            text: "space"
                        }
                        Mono {
                            text: root.proto.spaces[root.proto.activeSpace].name
                            color: root.spaceColor
                        }
                        Tracked {
                            text: "blocked"
                        }
                        Mono {
                            text: root.proto.blocked + " requests"
                        }
                        Tracked {
                            text: "state"
                        }
                        Mono {
                            text: "on show ∙ not frozen"
                        }
                        Tracked {
                            text: "visited"
                        }
                        Mono {
                            text: "just now"
                        }
                    }
                }
            }
        }
    }

    Column {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 2 * root.cw
        spacing: root.ch * 0.5
        Repeater {
            model: proto ? proto.toasts : []
            Item {
                required property var modelData
                width: toastText.implicitWidth + 4 * root.cw
                height: root.ch * 1.4
                Rectangle {
                    anchors.fill: parent
                    color: root.colors.overlayOpaque
                }
                Brackets {
                    anchors.fill: parent
                    tint: root.colors.accent
                    arm: 8
                }
                Mono {
                    id: toastText
                    anchors.centerIn: parent
                    text: modelData.text
                }
            }
        }
    }
}
