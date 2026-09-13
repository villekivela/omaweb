// PROTOTYPE — Variant F, "Horizon". Synthwave: the page floats over a
// perspective grid that runs to a horizon at the bottom in the Space's
// colour, the Space's name is set in display type, tabs are chamfered
// tiles along the top edge, and a gradient from the accent to nothing
// underlines whatever is live. Every colour is the theme's.
import QtQuick
import QtQuick.Shapes
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
    readonly property real horizonY: height - 3.4 * root.ch

    component Mono: Text {
        font.family: Style.font.family
        font.pixelSize: Style.font.body
        color: root.colors.text
        elide: Text.ElideRight
    }
    component Display: Text {
        font.family: Style.font.family
        font.pixelSize: Style.font.displayLarge
        font.bold: true
        font.letterSpacing: 4
        font.capitalization: Font.AllUppercase
        color: root.spaceColor
    }
    // A rectangle with two cut corners, top-left and bottom-right.
    component Chamfer: Shape {
        id: chamfer
        property color fill: root.colors.overlayOpaque
        property color stroke: "transparent"
        property real cut: 10
        ShapePath {
            fillColor: chamfer.fill
            strokeColor: chamfer.stroke
            strokeWidth: 1
            startX: chamfer.cut
            startY: 0
            PathLine {
                x: chamfer.width
                y: 0
            }
            PathLine {
                x: chamfer.width
                y: chamfer.height - chamfer.cut
            }
            PathLine {
                x: chamfer.width - chamfer.cut
                y: chamfer.height
            }
            PathLine {
                x: 0
                y: chamfer.height
            }
            PathLine {
                x: 0
                y: chamfer.cut
            }
            PathLine {
                x: chamfer.cut
                y: 0
            }
        }
    }

    // ------------------------------------------------------------ sky and horizon
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop {
                position: 0
                color: root.colors.windowOpaque
            }
            GradientStop {
                position: 0.75
                color: root.colors.windowOpaque
            }
            GradientStop {
                position: 1
                color: root.colors.sidebarOpaque
            }
        }
    }
    // The grid: verticals converge on a vanishing point above the horizon;
    // horizontals bunch towards it.
    Item {
        id: grid
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        opacity: 0.55
        Canvas {
            id: gridCanvas
            anchors.fill: parent
            onPaint: {
                const c = getContext("2d");
                c.clearRect(0, 0, width, height);
                c.strokeStyle = root.spaceColor;
                c.lineWidth = 1;
                for (let i = 0; i < 21; ++i) {
                    c.beginPath();
                    c.moveTo(width / 2 + (i - 10) * width * 0.09, height);
                    c.lineTo(width / 2 + (i - 10) * width * 0.02, root.horizonY);
                    c.stroke();
                }
                for (let i = 0; i < 7; ++i) {
                    const y = root.horizonY + Math.pow((i + 1) / 7, 2.2) * (height - root.horizonY);
                    c.beginPath();
                    c.moveTo(0, y);
                    c.lineTo(width, y);
                    c.stroke();
                }
            }
            Connections {
                target: root
                function onSpaceColorChanged() {
                    gridCanvas.requestPaint();
                }
            }
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
        }
        // Fade the grid out towards the top so it reads as ground, not wallpaper.
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop {
                    position: 0
                    color: root.colors.windowOpaque
                }
                GradientStop {
                    position: 0.55
                    color: root.colors.windowOpaque
                }
                GradientStop {
                    position: 1
                    color: "transparent"
                }
            }
        }
    }
    // The horizon line and its glow.
    Rectangle {
        y: root.horizonY - 6
        width: parent.width
        height: 12
        gradient: Gradient {
            GradientStop {
                position: 0
                color: "transparent"
            }
            GradientStop {
                position: 0.5
                color: Qt.alpha(root.spaceColor, 0.35)
            }
            GradientStop {
                position: 1
                color: "transparent"
            }
        }
    }
    Rectangle {
        y: root.horizonY
        width: parent.width
        height: 1
        color: root.spaceColor
    }

    // ------------------------------------------------------------ tab tiles along the top
    Item {
        id: tabStrip
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 2.6 * root.ch
        Row {
            x: 2 * root.cw
            y: root.ch * 0.5
            spacing: 6
            Repeater {
                model: root.proto ? root.proto.currentTabs : []
                Item {
                    required property var modelData
                    required property int index
                    readonly property bool current: root.proto.tabs.indexOf(modelData)
                                                    === root.proto.activeTab
                    width: Math.min(24 * root.cw, tileText.implicitWidth + 3 * root.cw)
                    height: root.ch * 1.6
                    Chamfer {
                        anchors.fill: parent
                        fill: parent.current ? Qt.alpha(root.spaceColor, 0.18) :
                                               root.colors.sidebarOpaque

                        stroke: parent.current ? root.spaceColor : root.colors.separator
                        cut: 8
                    }
                    Mono {
                        id: tileText
                        anchors.centerIn: parent
                        width: parent.width - 2 * root.cw
                        text: (modelData.pinned ? "◆ " : (index + 1) + " ") + modelData.title
                        color: parent.current ? root.colors.text : root.colors.mutedText
                        horizontalAlignment: Text.AlignHCenter
                    }
                    Rectangle {
                        visible: parent.current
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: -3
                        x: 8
                        width: parent.width - 16
                        height: 2
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop {
                                position: 0
                                color: "transparent"
                            }
                            GradientStop {
                                position: 0.5
                                color: root.spaceColor
                            }
                            GradientStop {
                                position: 1
                                color: "transparent"
                            }
                        }
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            root.proto.activeTab = root.proto.tabs.indexOf(modelData);
                        }
                    }
                }
            }
        }
        // Right end: the readouts.
        Row {
            anchors.right: parent.right
            anchors.rightMargin: 2 * root.cw
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2 * root.cw
            Mono {
                text: root.proto ? "⊘ " + root.proto.blocked : ""
                color: root.colors.mutedText
            }
            Mono {
                visible: root.proto && root.proto.downloads > 0
                text: root.proto ? "↓ " + Math.round(root.proto.downloadProgress * 100) + "%" : ""
                color: root.colors.accent
            }
            Mono {
                text: root.proto && root.proto.pendingKeys.length ? root.proto.pendingKeys : ""
                color: root.colors.accent
                font.bold: true
            }
        }
    }

    // ------------------------------------------------------------ the page, floating with chamfered corners
    Item {
        id: pageFrame
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: tabStrip.bottom
        anchors.bottom: parent.bottom
        anchors.leftMargin: 3 * root.cw
        anchors.rightMargin: 3 * root.cw
        anchors.bottomMargin: 4.6 * root.ch
        Chamfer {
            anchors.fill: parent
            anchors.margins: -2
            fill: "transparent"
            stroke: root.spaceColor
            cut: 18
            opacity: 0.8
        }
        Chamfer {
            anchors.fill: parent
            fill: root.colors.windowOpaque
            cut: 16
        }
        FakePage {
            anchors.fill: parent
            anchors.margins: 3
            colors: root.colors
            title: root.proto ? root.proto.activeTabInfo.title : ""
            opacity: root.proto && (root.proto.pickerOpen || root.proto.whichKeyOpen) ? 0.35 : 1
            Behavior on opacity {
                NumberAnimation {
                    duration: 120
                }
            }
        }
        Repeater {
            model: root.proto && root.proto.mode === "HINT" ? 6 : 0
            Item {
                x: 100 + (index * 137) % 500
                y: 140 + index * 62
                width: 3 * root.cw
                height: root.ch - 6
                Chamfer {
                    anchors.fill: parent
                    fill: root.colors.accent
                    cut: 5
                }
                Mono {
                    anchors.centerIn: parent
                    text: ["as", "df", "gh", "jk", "ls", "qw"][index]
                    color: root.colors.windowOpaque
                    font.bold: true
                }
            }
        }
        // Loading: a light sweeping the top edge.
        Rectangle {
            visible: root.proto && root.proto.loading
            y: -2
            x: 16
            width: (parent.width - 32) * (root.proto ? root.proto.loadProgress : 0)
            height: 2
            color: root.colors.accent
        }
    }

    // ------------------------------------------------------------ below the horizon: the Space in display type, the mode, the title
    Item {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: pageFrame.bottom
        anchors.bottom: parent.bottom
        anchors.leftMargin: 3 * root.cw
        anchors.rightMargin: 3 * root.cw
        Row {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: root.ch * 0.3
            spacing: 2 * root.cw
            Display {
                text: root.proto ? root.proto.spaces[root.proto.activeSpace].name : ""
                MouseArea {
                    anchors.fill: parent
                    onClicked: root.proto.leader("s")
                }
            }
            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2
                Row {
                    spacing: root.cw
                    Repeater {
                        model: root.proto ? root.proto.spaces : []
                        Rectangle {
                            required property var modelData
                            required property int index
                            width: index === root.proto.activeSpace ? 4 * root.cw : root.cw
                            height: 4
                            color: modelData.color
                            opacity: index === root.proto.activeSpace ? 1 : 0.4
                            Behavior on width {
                                NumberAnimation {
                                    duration: 150
                                }
                            }
                            MouseArea {
                                anchors.fill: parent
                                anchors.margins: -6
                                onClicked: {
                                    root.proto.activeSpace = index;
                                    root.proto.activeTab = root.proto.tabs.indexOf(
                                                root.proto.tabs.filter(t => t.space === index)[0]);
                                }
                            }
                        }
                    }
                }
                Mono {
                    text: root.proto ? root.proto.mode + "  ·  " + root.proto.activeTabIndexInSpace
                                       + "/" + root.proto.currentTabs.length : ""
                    color: root.proto && root.proto.mode === "HINT" ? root.colors.urgent :
                                                                      root.colors.mutedText
                    font.letterSpacing: 2
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            root.proto.whichKeyOpen = !root.proto.whichKeyOpen;
                            root.proto.pendingKeys = root.proto.whichKeyOpen ? "SPC" : "";
                        }
                    }
                }
            }
        }
        Column {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: root.ch * 0.3
            spacing: 2
            Mono {
                anchors.right: parent.right
                text: root.proto ? (root.proto.promptOpen
                                    ? "Clear browsing data for work, last hour?   [y] yes   [n] no" :
                                      root.proto.findOpen ? "/screenshot   3 of 7" :
                                                            root.proto.activeTabInfo.title) : ""
                font.pixelSize: Style.font.title
            }
            Mono {
                anchors.right: parent.right
                text: root.proto ? root.proto.activeTabInfo.origin : ""
                color: root.colors.mutedText
                font.letterSpacing: 2
            }
        }
    }

    // ------------------------------------------------------------ overlays over the page
    // Picker: a chamfered panel dropping from the tab strip.
    Item {
        visible: proto && proto.pickerOpen
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: tabStrip.bottom
        anchors.topMargin: root.ch
        width: Math.min(parent.width - 12 * root.cw, 110 * root.cw)
        height: 19 * root.ch
        Chamfer {
            anchors.fill: parent
            anchors.margins: -2
            fill: "transparent"
            stroke: root.colors.accent
            cut: 16
        }
        Chamfer {
            anchors.fill: parent
            fill: root.colors.overlayOpaque
            cut: 14
        }
        Column {
            anchors.fill: parent
            anchors.margins: root.ch * 0.8
            anchors.leftMargin: 2 * root.cw
            anchors.rightMargin: 2 * root.cw
            Item {
                width: parent.width
                height: root.ch * 1.4
                Mono {
                    anchors.verticalCenter: parent.verticalCenter
                    text: (root.proto ? root.proto.query : "") + "▏"
                    font.pixelSize: Style.font.title
                    color: root.colors.accent
                }
                Mono {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.proto ? root.proto.filteredTabs().length + " across every Space" : ""
                    color: root.colors.mutedText
                    font.letterSpacing: 2
                }
                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop {
                            position: 0
                            color: root.colors.accent
                        }
                        GradientStop {
                            position: 1
                            color: "transparent"
                        }
                    }
                }
            }
            Row {
                width: parent.width
                height: parent.height - root.ch * 1.4
                Column {
                    id: list
                    width: parent.width * 0.58
                    Repeater {
                        model: root.proto ? root.proto.filteredTabs() : []
                        delegate: Item {
                            required property var modelData
                            required property int index
                            readonly property bool cur: index === root.proto.cursor
                            width: list.width
                            height: root.ch
                            Rectangle {
                                anchors.fill: parent
                                visible: parent.cur
                                gradient: Gradient {
                                    orientation: Gradient.Horizontal
                                    GradientStop {
                                        position: 0
                                        color: Qt.alpha(root.colors.accent, 0.35)
                                    }
                                    GradientStop {
                                        position: 1
                                        color: "transparent"
                                    }
                                }
                            }
                            Row {
                                anchors.verticalCenter: parent.verticalCenter
                                x: root.cw
                                spacing: root.cw
                                Rectangle {
                                    width: 3
                                    height: root.ch - 8
                                    y: 4
                                    color: root.proto.spaces[modelData.space].color
                                }
                                Mono {
                                    text: root.proto.spaces[modelData.space].name
                                    color: root.proto.spaces[modelData.space].color
                                    width: 5 * root.cw
                                    font.letterSpacing: 1
                                }
                                Mono {
                                    text: (modelData.pinned ? "◆ " : "") + modelData.title
                                    color: parent.parent.cur ? root.colors.text :
                                                               root.colors.mutedText
                                    font.bold: parent.parent.cur
                                }
                                Mono {
                                    text: modelData.origin
                                    color: root.colors.mutedText
                                    opacity: 0.7
                                }
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: root.proto.cursor = index
                            }
                        }
                    }
                }
                Item {
                    width: parent.width - list.width
                    height: parent.height
                    readonly property var row: root.proto ? root.proto.filteredTabs(
                                                                )[root.proto.cursor] : null
                    readonly property bool onShow: row && row.index === root.proto.activeTab
                    Item {
                        visible: parent.row && !parent.onShow
                        anchors.fill: parent
                        anchors.leftMargin: 2 * root.cw
                        Chamfer {
                            anchors.fill: parent
                            anchors.margins: -1
                            fill: "transparent"
                            stroke: root.colors.border
                            cut: 10
                        }
                        FakePage {
                            anchors.fill: parent
                            anchors.margins: 2
                            colors: root.colors
                            title: parent.parent.row ? parent.parent.row.title : ""
                            miniature: true
                        }
                        Mono {
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.margins: root.cw
                            text: parent.parent.row ? "as of " + parent.parent.row.age : ""
                            color: root.colors.mutedText
                            font.letterSpacing: 2
                        }
                    }
                    Column {
                        visible: parent.onShow
                        anchors.left: parent.left
                        anchors.leftMargin: 3 * root.cw
                        y: root.ch * 0.5
                        spacing: 4
                        Display {
                            text: "on show"
                            font.pixelSize: Style.font.heading
                        }
                        Mono {
                            text: parent.parent.row ? parent.parent.row.title : ""
                            font.pixelSize: Style.font.title
                        }
                        Mono {
                            text: parent.parent.row ? parent.parent.row.origin + "  ·  "
                                                      + root.proto.blocked
                                                      + " blocked  ·  visited just now" : ""
                            color: root.colors.mutedText
                        }
                    }
                }
            }
        }
    }

    // which-key: rises from the horizon.
    Item {
        visible: proto && proto.whichKeyOpen
        anchors.left: pageFrame.left
        anchors.right: pageFrame.right
        anchors.bottom: pageFrame.bottom
        height: 6.5 * root.ch
        Chamfer {
            anchors.fill: parent
            fill: root.colors.overlayOpaque
            cut: 14
        }
        Rectangle {
            anchors.top: parent.top
            x: 14
            width: parent.width - 14
            height: 1
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop {
                    position: 0
                    color: root.spaceColor
                }
                GradientStop {
                    position: 1
                    color: "transparent"
                }
            }
        }
        Grid {
            x: 3 * root.cw
            y: root.ch
            columns: 6
            columnSpacing: 2 * root.cw
            Repeater {
                model: root.proto ? root.proto.leaderBindings : []
                Row {
                    required property var modelData
                    width: 22 * root.cw
                    height: root.ch
                    spacing: root.cw
                    Mono {
                        text: modelData.key
                        color: root.spaceColor
                        font.bold: true
                        width: 4 * root.cw
                        font.letterSpacing: 1
                    }
                    Mono {
                        text: modelData.label
                        color: root.colors.text
                    }
                }
            }
        }
    }

    // Sidebar: the tree, as a chamfered panel over the left of the page.
    Item {
        visible: proto && proto.sidebarOpen
        anchors.left: pageFrame.left
        anchors.top: pageFrame.top
        anchors.bottom: pageFrame.bottom
        width: 30 * root.cw
        Chamfer {
            anchors.fill: parent
            fill: root.colors.overlayOpaque
            cut: 14
            opacity: 0.96
        }
        Column {
            x: 2 * root.cw
            y: root.ch
            Repeater {
                model: root.proto ? root.proto.spaces : []
                delegate: Column {
                    required property var modelData
                    required property int index
                    Display {
                        text: modelData.name
                        color: modelData.color
                        font.pixelSize: Style.font.heading
                        height: root.ch * 1.4
                        verticalAlignment: Text.AlignVCenter
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
                                   ? root.colors.text : root.colors.mutedText
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

    // Toasts: chamfered, top-right, under the strip.
    Column {
        anchors.top: tabStrip.bottom
        anchors.right: parent.right
        anchors.rightMargin: 3 * root.cw
        spacing: 6
        Repeater {
            model: proto ? proto.toasts : []
            Item {
                required property var modelData
                width: t.implicitWidth + 3 * root.cw
                height: root.ch * 1.4
                Chamfer {
                    anchors.fill: parent
                    fill: root.colors.overlayOpaque
                    stroke: root.colors.accent
                    cut: 8
                }
                Mono {
                    id: t
                    anchors.centerIn: parent
                    text: modelData.text
                }
            }
        }
    }
}
