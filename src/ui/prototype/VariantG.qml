// PROTOTYPE — Variant G, "Deck". Cassette futurism: the browser as a piece
// of hardware. Bevelled panels, a rail of square keys for the Spaces with
// LED dots, a recessed readout for the title, tabs as stepped cards, LED
// bars for the counters. The theme's surfaces are the plastic; the accent
// and the Space colours are the lights.
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
    readonly property real rail: 9 * root.cw

    component Mono: Text {
        font.family: Style.font.family
        font.pixelSize: Style.font.body
        color: root.colors.text
        elide: Text.ElideRight
    }
    component Label: Text {
        font.family: Style.font.family
        font.pixelSize: Style.font.caption
        font.letterSpacing: 2
        font.capitalization: Font.AllUppercase
        color: root.colors.mutedText
    }
    // A raised panel: light edge top-left, dark edge bottom-right.
    component Raised: Rectangle {
        color: root.colors.surface
        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.right: parent.right
            height: 2
            color: root.colors.surfaceHover
        }
        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 2
            color: root.colors.surfaceHover
        }
        Rectangle {
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            height: 2
            color: root.colors.windowOpaque
        }
        Rectangle {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 2
            color: root.colors.windowOpaque
        }
    }
    // A recessed well: the reverse.
    component Well: Rectangle {
        color: root.colors.windowOpaque
        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.right: parent.right
            height: 2
            color: root.colors.windowOpaque
            opacity: 0.001
        }
        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.right: parent.right
            height: 2
            color: Qt.darker(root.colors.sidebarOpaque, 1.4)
        }
        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 2
            color: Qt.darker(root.colors.sidebarOpaque, 1.4)
        }
        Rectangle {
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            height: 2
            color: root.colors.surfaceHover
        }
        Rectangle {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 2
            color: root.colors.surfaceHover
        }
    }
    component Led: Rectangle {
        property bool on: true
        property color tint: root.colors.accent
        width: 8
        height: 8
        radius: 4
        color: on ? tint : root.colors.windowOpaque
        border.color: Qt.darker(root.colors.surface, 1.3)
        Rectangle {
            anchors.centerIn: parent
            width: 16
            height: 16
            radius: 8
            color: parent.tint
            opacity: parent.on ? 0.25 : 0
            z: -1
        }
    }

    Rectangle {
        anchors.fill: parent
        color: colors.sidebarOpaque
    }

    // ------------------------------------------------------------ left rail: Space keys
    Raised {
        id: railPanel
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: root.rail
        Column {
            anchors.horizontalCenter: parent.horizontalCenter
            y: root.ch
            spacing: 8
            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "space"
            }
            Repeater {
                model: root.proto ? root.proto.spaces : []
                Item {
                    required property var modelData
                    required property int index
                    readonly property bool current: index === root.proto.activeSpace
                    width: root.rail - 2 * root.cw
                    height: width
                    Raised {
                        anchors.fill: parent
                        anchors.margins: current ? 2 : 0
                        color: current ? root.colors.surfaceHover : root.colors.surface
                    }
                    Led {
                        anchors.top: parent.top
                        anchors.right: parent.right
                        anchors.margins: 8
                        on: parent.current
                        tint: modelData.color
                    }
                    Mono {
                        anchors.centerIn: parent
                        text: (index + 1)
                        font.pixelSize: Style.font.heading
                        font.bold: true
                        color: parent.current ? modelData.color : root.colors.mutedText
                    }
                    Label {
                        anchors.bottom: parent.bottom
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottomMargin: 6
                        text: modelData.name
                        color: parent.current ? root.colors.text : root.colors.mutedText
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
            Item {
                width: 1
                height: root.ch
            }
            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "mode"
            }
            Item {
                width: root.rail - 2 * root.cw
                height: root.ch * 1.6
                Well {
                    anchors.fill: parent
                }
                Mono {
                    anchors.centerIn: parent
                    text: root.proto ? root.proto.mode : ""
                    color: root.proto && root.proto.mode === "HINT" ? root.colors.urgent :
                                                                      root.spaceColor
                    font.bold: true
                    font.pixelSize: Style.font.caption
                    font.letterSpacing: 1
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        root.proto.whichKeyOpen = !root.proto.whichKeyOpen;
                        root.proto.pendingKeys = root.proto.whichKeyOpen ? "SPC" : "";
                    }
                }
            }
            Item {
                width: 1
                height: root.ch
            }
            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "tree"
            }
            Item {
                width: root.rail - 2 * root.cw
                height: root.ch * 1.6
                Raised {
                    anchors.fill: parent
                    anchors.margins: root.proto && root.proto.sidebarOpen ? 2 : 0
                }
                Led {
                    anchors.centerIn: parent
                    on: root.proto && root.proto.sidebarOpen
                    tint: root.colors.accent
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: root.proto.sidebarOpen = !root.proto.sidebarOpen
                }
            }
        }
        // Bottom of the rail: the LED bars.
        Column {
            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottomMargin: root.ch
            spacing: 6
            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "load"
            }
            Column {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 3
                Repeater {
                    model: 8
                    Rectangle {
                        width: root.rail - 4 * root.cw
                        height: 4
                        color: root.proto && root.proto.loading && (7 - index) / 8
                               < root.proto.loadProgress ? root.spaceColor :
                                                           root.colors.windowOpaque
                    }
                }
            }
            Item {
                width: 1
                height: root.ch * 0.5
            }
            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "dl"
            }
            Column {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 3
                Repeater {
                    model: 8
                    Rectangle {
                        width: root.rail - 4 * root.cw
                        height: 4
                        color: root.proto && root.proto.downloads > 0 && (7 - index) / 8
                               < root.proto.downloadProgress ? root.colors.accent :
                                                               root.colors.windowOpaque
                    }
                }
            }
        }
    }

    // ------------------------------------------------------------ top: the readout and the tab cards
    Item {
        id: top
        anchors.left: railPanel.right
        anchors.right: parent.right
        anchors.top: parent.top
        height: 4.2 * root.ch
        Well {
            id: readout
            x: 2 * root.cw
            y: root.ch * 0.7
            width: parent.width - 4 * root.cw
            height: root.ch * 1.5
            Mono {
                anchors.left: parent.left
                anchors.leftMargin: root.cw * 1.5
                anchors.verticalCenter: parent.verticalCenter
                color: root.spaceColor
                font.letterSpacing: 1
                font.bold: true
                text: root.proto ? (root.proto.promptOpen
                                    ? "CLEAR BROWSING DATA FOR WORK, LAST HOUR?   Y/N" :
                                      root.proto.findOpen ? "/SCREENSHOT   3 OF 7" :
                                                            root.proto.activeTabInfo.title.toUpperCase(
                                                                )) : ""
            }
            Row {
                anchors.right: parent.right
                anchors.rightMargin: root.cw * 1.5
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2 * root.cw
                Mono {
                    text: root.proto ? root.proto.activeTabInfo.origin : ""
                    color: root.colors.mutedText
                }
                Mono {
                    text: root.proto ? "⊘" + root.proto.blocked : ""
                    color: root.colors.mutedText
                }
                Mono {
                    text: root.proto ? String(root.proto.activeTabIndexInSpace).padStart(2, "0") + "/"
                                       + String(root.proto.currentTabs.length).padStart(2, "0") : ""
                    color: root.spaceColor
                    font.bold: true
                    MouseArea {
                        anchors.fill: parent
                        onClicked: root.proto.leader("t")
                    }
                }
                Mono {
                    text: root.proto && root.proto.pendingKeys.length ? root.proto.pendingKeys :
                                                                        "   "
                    color: root.colors.accent
                    font.bold: true
                }
            }
            // A blinking dot: the deck is on.
            Led {
                anchors.left: parent.left
                anchors.leftMargin: -root.cw * 1.2
                anchors.verticalCenter: parent.verticalCenter
                tint: root.spaceColor
                width: 6
                height: 6
                radius: 3
            }
        }
        Row {
            anchors.left: parent.left
            anchors.leftMargin: 2 * root.cw
            anchors.bottom: parent.bottom
            spacing: 3
            Repeater {
                model: root.proto ? root.proto.currentTabs : []
                Item {
                    required property var modelData
                    required property int index
                    readonly property bool current: root.proto.tabs.indexOf(modelData)
                                                    === root.proto.activeTab
                    width: Math.min(22 * root.cw, cardText.implicitWidth + 3 * root.cw)
                    height: current ? root.ch * 1.5 : root.ch * 1.3
                    anchors.bottom: parent.bottom
                    Raised {
                        anchors.fill: parent
                        color: parent.current ? root.colors.surfaceHover : root.colors.surface
                    }
                    Rectangle {
                        anchors.top: parent.top
                        anchors.topMargin: 2
                        x: 2
                        width: parent.width - 4
                        height: 2
                        color: parent.current ? root.proto.spaces[modelData.space].color :
                                                "transparent"
                    }
                    Mono {
                        id: cardText
                        anchors.centerIn: parent
                        width: parent.width - 2 * root.cw
                        text: (modelData.pinned ? "◆ " : "") + modelData.title
                        color: parent.current ? root.colors.text : root.colors.mutedText
                        horizontalAlignment: Text.AlignHCenter
                        font.pixelSize: Style.font.bodySmall
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: root.proto.activeTab = root.proto.tabs.indexOf(modelData)
                    }
                }
            }
        }
    }

    // ------------------------------------------------------------ the screen in its bezel
    Raised {
        id: bezel
        anchors.left: railPanel.right
        anchors.right: parent.right
        anchors.top: top.bottom
        anchors.bottom: parent.bottom
        anchors.leftMargin: 2 * root.cw
        anchors.rightMargin: 2 * root.cw
        anchors.bottomMargin: 2 * root.cw
        Well {
            id: screen
            anchors.fill: parent
            anchors.margins: 8
            FakePage {
                anchors.fill: parent
                anchors.margins: 2
                colors: root.colors
                title: root.proto ? root.proto.activeTabInfo.title : ""
            }
        }
        Repeater {
            model: root.proto && root.proto.mode === "HINT" ? 6 : 0
            Raised {
                x: 110 + (index * 137) % 500
                y: 150 + index * 62
                width: 3 * root.cw
                height: root.ch - 4
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

    // ------------------------------------------------------------ overlays
    // Picker: a drawer sliding out of the bezel's top edge.
    Raised {
        visible: proto && proto.pickerOpen
        anchors.left: bezel.left
        anchors.right: bezel.right
        anchors.top: bezel.top
        anchors.leftMargin: 6 * root.cw
        anchors.rightMargin: 6 * root.cw
        height: 19 * root.ch
        Column {
            anchors.fill: parent
            anchors.margins: 10
            Well {
                width: parent.width
                height: root.ch * 1.4
                Mono {
                    anchors.left: parent.left
                    anchors.leftMargin: root.cw
                    anchors.verticalCenter: parent.verticalCenter
                    text: "> " + (root.proto ? root.proto.query.toUpperCase() : "") + "_"
                    color: root.spaceColor
                    font.bold: true
                }
                Label {
                    anchors.right: parent.right
                    anchors.rightMargin: root.cw
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.proto ? root.proto.filteredTabs().length + " tabs · all spaces" : ""
                }
            }
            Item {
                width: 1
                height: 8
            }
            Row {
                width: parent.width
                height: parent.height - root.ch * 1.4 - 8
                spacing: 8
                Well {
                    id: list
                    width: parent.width * 0.58
                    height: parent.height
                    Column {
                        anchors.fill: parent
                        anchors.margins: 4
                        Repeater {
                            model: root.proto ? root.proto.filteredTabs() : []
                            delegate: Item {
                                required property var modelData
                                required property int index
                                readonly property bool cur: index === root.proto.cursor
                                width: parent.width
                                height: root.ch
                                Rectangle {
                                    anchors.fill: parent
                                    color: root.colors.surface
                                    visible: parent.cur
                                }
                                Row {
                                    anchors.verticalCenter: parent.verticalCenter
                                    x: root.cw
                                    spacing: root.cw
                                    Led {
                                        anchors.verticalCenter: parent.verticalCenter
                                        on: true
                                        tint: root.proto.spaces[modelData.space].color
                                        width: 6
                                        height: 6
                                        radius: 3
                                    }
                                    Mono {
                                        text: root.proto.spaces[modelData.space].name
                                        color: root.colors.mutedText
                                        width: 5 * root.cw
                                        font.pixelSize: Style.font.caption
                                        font.letterSpacing: 1
                                    }
                                    Mono {
                                        text: (modelData.pinned ? "◆ " : "") + modelData.title
                                        color: parent.parent.cur ? root.spaceColor :
                                                                   root.colors.text
                                        font.bold: parent.parent.cur
                                    }
                                    Mono {
                                        text: modelData.origin
                                        color: root.colors.mutedText
                                    }
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: root.proto.cursor = index
                                }
                            }
                        }
                    }
                }
                Well {
                    width: parent.width - list.width - 8
                    height: parent.height
                    readonly property var row: root.proto ? root.proto.filteredTabs(
                                                                )[root.proto.cursor] : null
                    readonly property bool onShow: row && row.index === root.proto.activeTab
                    FakePage {
                        visible: parent.row && !parent.onShow
                        anchors.fill: parent
                        anchors.margins: 4
                        colors: root.colors
                        title: parent.row ? parent.row.title : ""
                        miniature: true
                    }
                    Label {
                        visible: parent.row && !parent.onShow
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.margins: root.cw
                        text: parent.row ? "as of " + parent.row.age : ""
                        color: root.spaceColor
                    }
                    Column {
                        visible: parent.onShow
                        x: root.cw * 1.5
                        y: root.ch * 0.5
                        spacing: 2
                        Label {
                            text: "on show"
                        }
                        Mono {
                            text: parent.parent.row ? parent.parent.row.title : ""
                            color: root.spaceColor
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
                        Label {
                            text: "blocked  " + root.proto.blocked
                        }
                        Label {
                            text: "visited  just now"
                        }
                    }
                }
            }
        }
    }

    // which-key: a card slid out of the bezel's bottom edge.
    Raised {
        visible: proto && proto.whichKeyOpen
        anchors.left: bezel.left
        anchors.right: bezel.right
        anchors.bottom: bezel.bottom
        anchors.leftMargin: 4 * root.cw
        anchors.rightMargin: 4 * root.cw
        height: 6.5 * root.ch
        Label {
            x: 2 * root.cw
            y: root.ch * 0.6
            text: "keys after space"
        }
        Grid {
            x: 2 * root.cw
            y: 2 * root.ch
            columns: 6
            columnSpacing: 2 * root.cw
            rowSpacing: 4
            Repeater {
                model: root.proto ? root.proto.leaderBindings : []
                Row {
                    required property var modelData
                    width: 22 * root.cw
                    height: root.ch
                    spacing: root.cw
                    Well {
                        width: Math.max(2.5 * root.cw, k.implicitWidth + root.cw)
                        height: root.ch - 6
                        anchors.verticalCenter: parent.verticalCenter
                        Mono {
                            id: k
                            anchors.centerIn: parent
                            text: modelData.key
                            color: root.spaceColor
                            font.bold: true
                            font.pixelSize: Style.font.caption
                        }
                    }
                    Mono {
                        text: modelData.label
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }
        }
    }

    // Sidebar tree: a panel over the left of the screen.
    Raised {
        visible: proto && proto.sidebarOpen
        anchors.left: bezel.left
        anchors.top: bezel.top
        anchors.bottom: bezel.bottom
        width: 30 * root.cw
        Column {
            x: 2 * root.cw
            y: root.ch
            Repeater {
                model: root.proto ? root.proto.spaces : []
                delegate: Column {
                    required property var modelData
                    required property int index
                    Row {
                        spacing: root.cw
                        height: root.ch * 1.3
                        Led {
                            anchors.verticalCenter: parent.verticalCenter
                            on: index === root.proto.activeSpace
                            tint: modelData.color
                        }
                        Label {
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData.name
                            color: modelData.color
                        }
                    }
                    Repeater {
                        model: root.proto.tabs.filter(t => t.space === index)
                        Mono {
                            required property var modelData
                            text: (modelData.pinned ? "◆ " : "  ") + modelData.title
                            width: 26 * root.cw
                            height: root.ch
                            verticalAlignment: Text.AlignVCenter
                            color: root.proto.tabs.indexOf(modelData) === root.proto.activeTab
                                   ? root.spaceColor : root.colors.mutedText
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

    // Toasts: a strip printed under the readout.
    Column {
        anchors.top: bezel.top
        anchors.right: bezel.right
        anchors.margins: 2 * root.cw
        spacing: 6
        Repeater {
            model: proto ? proto.toasts : []
            Raised {
                required property var modelData
                width: t.implicitWidth + 3 * root.cw
                height: root.ch * 1.5
                Led {
                    anchors.left: parent.left
                    anchors.leftMargin: root.cw
                    anchors.verticalCenter: parent.verticalCenter
                    tint: root.colors.accent
                    width: 6
                    height: 6
                    radius: 3
                }
                Mono {
                    id: t
                    anchors.centerIn: parent
                    anchors.horizontalCenterOffset: root.cw * 0.6
                    text: modelData.text
                }
            }
        }
    }
}
