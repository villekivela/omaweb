import QtQuick
import Quickshell
import Quickshell.Io
import Quickshell.Wayland
import qs.Commons
import qs.Commons as Commons
import qs.Ui
import "BarModel.mjs" as BarModel

// Omaweb's Spaces and tabs in the Omarchy bar (#906). The browser places it
// twice when the reader sets its tabs to show here: role "tabs" after the
// workspaces, and role "location" in the center.
//
// Everything it shows comes from the browser over the bar socket, one state a
// line, and everything it does goes back as one action a line; it reaches no
// page. It takes no room while no browser answers there.
BarWidget {
    id: root
    moduleName: "omaweb.bar"

    readonly property string role: setting("role", "tabs")
    readonly property bool spaceLetters: setting("spaceNames", "full") === "letter"
    readonly property real maxShare: Number(setting("maxWidthPercent", 40)) / 100
    readonly property int locationWidth: Number(setting("locationWidth", 360))
    readonly property string socketPath: setting("socket", (Quickshell.env("XDG_RUNTIME_DIR")
                                                            || "/tmp") + "/omaweb/bar.sock")
    // The main window and the private ones are `omaweb`, a Tab window is
    // `omaweb-tab`.
    readonly property string appId: setting("appId", "omaweb")
    readonly property string tabWindowAppId: setting("tabWindowAppId", "omaweb-tab")

    property var browser: null
    readonly property bool live: socket.connected && browser !== null
    readonly property var toplevel: ToplevelManager.activeToplevel
    readonly property bool focused: toplevel !== null && (toplevel.appId === appId || toplevel.appId
                                                          === tabWindowAppId)
    readonly property var space: live ? browser.spaces.find(s => s.id === browser.activeSpace)
                                        || null : null
    readonly property string activeId: space ? space.active : ""
    readonly property var activeTab: space ? space.pins.concat(space.tabs).find(t => t.id
                                                                                     === activeId)
                                             || null : null
    readonly property var downloads: live && browser.status ? browser.status.downloads : null

    readonly property color fg: bar ? bar.barForeground : Commons.Color.foreground
    readonly property color barGround: bar ? bar.background : Commons.Color.background
    readonly property color popupFg: Commons.Color.popups.text
    readonly property string font: bar ? bar.fontFamily : Style.font.family
    readonly property int gap: Style.space(10)
    readonly property real iconLength: 14

    // Which popup is open ("menu", "spaces", "status" or ""), and over what.
    property string popup: ""
    property Item popupAnchor: root
    property var menuTab: null
    property real wheelAccumulator: 0

    function send(message) {
        if (!socket.connected)
            return;
        socket.write(JSON.stringify(message) + "\n");
        socket.flush();
    }

    // The bar is the shell's, so it is the shell that can hand Omaweb the
    // keyboard: the browser cannot take focus for a click it never saw.
    // A window is told by its title, which the browser sends for its main
    // window. Where the title matches no window or more than one, nothing is
    // raised rather than the wrong one.
    function raise(tab) {
        const windows = ToplevelManager.toplevels.values;
        const only = found => found.length === 1 ? found[0] : null;
        let target = null;
        if (tab && tab.poppedOut)
            target = only(windows.filter(w => w.appId === root.tabWindowAppId && w.title
                                              === tab.title));
        if (!target && root.live)
            target = only(windows.filter(w => w.appId === root.appId && w.title
                                              === root.browser.window));
        if (target)
            target.activate();
    }

    function activate(tab, spaceId) {
        send({
                 "do": "activate",
                 "tab": tab.id,
                 "space": spaceId || root.browser.activeSpace
             });
        raise(tab);
    }

    function openPopup(kind, anchor, tab) {
        if (popup === kind && popupAnchor === anchor) {
            popup = "";
            return;
        }
        popupAnchor = anchor;
        menuTab = tab || null;
        popup = kind;
    }

    function closePopup() {
        popup = "";
    }

    function hostCode(host) {
        return String(host || "").replace(/^www\./, "").slice(0, 2);
    }

    function along(item) {
        return item && item.visible ? (vertical ? item.implicitHeight : item.implicitWidth) : 0;
    }

    // ----- overflow: everything but the tabs keeps its size, and the tabs get
    // what is left of the cap. Their spacing shrinks first, then the ones
    // furthest from the active tab go into "+N".

    readonly property real barLength: {
        const w = root.QsWindow.window;
        if (!w)
            return 1600;
        return vertical ? w.height : w.width;
    }
    readonly property real fixedLength: !live || role !== "tabs" ? 0 : along(navGroup) + along(
                                                                       spacesRow) + along(pinsRow)
                                                                   + along(statusMark) + along(
                                                                       recordingMark) + along(
                                                                       releaseMark) + gap * 4
                                                                   + grid.spacing * 6
    readonly property var tabLayout: {
        if (!live || role !== "tabs" || !space)
            return {
                "pad": 7,
                "tabs": [],
                "hidden": 0
            };
        const room = barLength * maxShare - fixedLength;
        const fitted = BarModel.fitTabs(space.tabs, activeId, room, iconLength);
        return fitted.hidden > 0 ? BarModel.fitTabs(space.tabs, activeId, room - plusN.markLength,
                                                    iconLength) : fitted;
    }

    visible: live
    implicitWidth: !live ? 0 : (vertical ? barSize : grid.implicitWidth + gap * 2)
    implicitHeight: !live ? 0 : (vertical ? grid.implicitHeight + gap * 2 : barSize)

    Socket {
        id: socket
        path: root.socketPath
        parser: SplitParser {
            onRead: data => {
                try {
                    const message = JSON.parse(data);
                    if (message.type === "state")
                        root.browser = message;
                } catch (e) {}
            }
        }
        onConnectionStateChanged: if (!connected) {
                                      root.browser = null;
                                      root.popup = "";
                                  }
    }

    // The socket is there only while the reader has the tabs in the bar, and
    // the browser may start after the shell.
    Timer {
        interval: 1000
        repeat: true
        running: !socket.connected
        triggeredOnStart: true
        onTriggered: socket.connected = true
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.NoButton
        enabled: root.role === "tabs"
        onWheel: function (wheel) {
            root.wheelAccumulator += wheel.angleDelta.y || wheel.angleDelta.x;
            if (Math.abs(root.wheelAccumulator) < 120)
                return;
            root.send({
                          "do": "cycle",
                          "step": root.wheelAccumulator > 0 ? -1 : 1
                      });
            root.wheelAccumulator = 0;
        }
    }

    Grid {
        id: grid
        anchors.centerIn: parent
        columns: root.vertical ? 1 : 100
        spacing: Style.space(6)
        verticalItemAlignment: Grid.AlignVCenter
        horizontalItemAlignment: Grid.AlignHCenter
        opacity: root.focused ? 1 : 0.65
        Behavior on opacity {
            NumberAnimation {
                duration: 160
            }
        }

        // ----- role: tabs

        Grid {
            id: navGroup
            visible: root.role === "tabs"
            columns: root.vertical ? 1 : 100
            verticalItemAlignment: Grid.AlignVCenter
            horizontalItemAlignment: Grid.AlignHCenter
            BarText {
                text: "󰁍"
                dim: !(root.live && root.browser.location.canBack)
                tooltip: qsTr("Back")
                onClicked: root.send({
                                         "do": "back"
                                     })
            }
            BarText {
                text: "󰁔"
                dim: !(root.live && root.browser.location.canForward)
                tooltip: qsTr("Forward")
                onClicked: root.send({
                                         "do": "forward"
                                     })
            }
            BarText {
                text: "󰑐"
                tooltip: qsTr("Reload")
                onClicked: root.send({
                                         "do": "reload"
                                     })
            }
        }

        Divider {
            visible: root.role === "tabs"
        }

        Grid {
            id: spacesRow
            visible: root.role === "tabs"
            columns: root.vertical ? 1 : 100
            spacing: 2
            verticalItemAlignment: Grid.AlignVCenter
            horizontalItemAlignment: Grid.AlignHCenter
            Repeater {
                model: root.live ? root.browser.spaces : []
                SpaceSwitch {
                    required property var modelData
                    space: modelData
                }
            }
        }

        Divider {
            visible: root.role === "tabs" && !!root.space && root.space.pins.length > 0
        }

        Grid {
            id: pinsRow
            visible: root.role === "tabs" && !!root.space && root.space.pins.length > 0
            columns: root.vertical ? 1 : 100
            verticalItemAlignment: Grid.AlignVCenter
            horizontalItemAlignment: Grid.AlignHCenter
            Repeater {
                model: root.space ? root.space.pins : []
                TabCell {
                    required property var modelData
                    tab: modelData
                    pad: 5
                    pinned: true
                }
            }
        }

        Divider {
            visible: root.role === "tabs" && !!root.space && root.space.tabs.length > 0
        }

        Grid {
            visible: root.role === "tabs" && root.tabLayout.tabs.length > 0
            columns: root.vertical ? 1 : 100
            verticalItemAlignment: Grid.AlignVCenter
            horizontalItemAlignment: Grid.AlignHCenter
            Repeater {
                model: root.tabLayout.tabs
                TabCell {
                    required property var modelData
                    tab: modelData
                    pad: root.tabLayout.pad
                }
            }
        }

        BarText {
            id: plusN
            readonly property real markLength: root.vertical ? implicitHeight : implicitWidth
            visible: root.role === "tabs" && root.tabLayout.hidden > 0
            text: "+" + root.tabLayout.hidden
            tooltip: qsTr("%1 more tabs").arg(root.tabLayout.hidden)
            onClicked: root.openPopup("spaces", plusN)
        }

        // The Status island's marks, each only while it applies: a Problem
        // recording, the downloads running and a newer release.
        BarText {
            id: recordingMark
            visible: root.role === "tabs" && root.live && !!root.browser.status.recording
            text: "󰑋"
            colour: Commons.Color.urgent
            tooltip: qsTr("Recording a problem")
            onClicked: root.openPopup("status", recordingMark)
        }
        BarText {
            id: statusMark
            visible: root.role === "tabs" && !!root.downloads && root.downloads.running > 0
            text: root.vertical || !root.downloads ? "󰇚" : "󰇚 " + root.downloads.progress + "%"
            tooltip: root.downloads ? root.downloads.files.map(d => d.name + " " + d.progress
                                                                    + "%").join("\n") : ""
            onClicked: root.openPopup("status", statusMark)
        }
        BarText {
            id: releaseMark
            visible: root.role === "tabs" && root.live && !!root.browser.status.release
            text: root.vertical || !visible ? "󰚰" : "󰚰 " + root.browser.status.release.version
            tooltip: visible ? root.browser.status.release.instruction : ""
            onClicked: root.openPopup("status", releaseMark)
        }

        // ----- role: location, at a fixed width so a tab switch moves nothing

        Item {
            id: location
            visible: root.role === "location"
            implicitWidth: root.vertical ? root.barSize - 4 : root.locationWidth
            implicitHeight: root.vertical ? 22 : root.barSize - 8

            Rectangle {
                anchors.fill: parent
                radius: 4
                color: Qt.alpha(root.fg, locationMouse.containsMouse ? 0.1 : 0)
            }
            Row {
                visible: !root.vertical
                anchors.centerIn: parent
                width: Math.min(implicitWidth, parent.width - 12)
                spacing: Style.space(6)
                Text {
                    id: lock
                    text: root.live && root.browser.location.secure ? "󰌾" : "󰿆"
                    color: root.fg
                    opacity: 0.6
                    font.family: root.font
                    font.pixelSize: Style.font.body
                }
                Text {
                    id: hostText
                    text: root.live ? root.browser.location.host : ""
                    textFormat: Text.PlainText
                    color: root.fg
                    font.family: root.font
                    font.pixelSize: Style.font.body
                }
                // What the fixed width leaves after the lock and the host.
                Text {
                    width: Math.max(0, Math.min(implicitWidth, location.width - 12 - lock.implicitWidth
                                                - hostText.implicitWidth - Style.space(12)))
                    text: root.live ? root.browser.location.title : ""
                    textFormat: Text.PlainText
                    elide: Text.ElideRight
                    color: root.fg
                    opacity: 0.5
                    font.family: root.font
                    font.pixelSize: Style.font.body
                }
            }
            Text {
                visible: root.vertical
                anchors.centerIn: parent
                text: root.live && root.browser.location.secure ? "󰌾" : "󰿆"
                color: root.fg
                font.family: root.font
                font.pixelSize: Style.font.body
            }
            MouseArea {
                id: locationMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    root.send({
                                  "do": "omnibar"
                              });
                    root.raise(null);
                }
                onEntered: if (root.bar && root.live)
                               root.bar.showTooltip(location, root.browser.location.title)
                onExited: if (root.bar)
                              root.bar.hideTooltip(location)
            }
        }
    }

    // ----- pieces

    component Divider: Rectangle {
        implicitWidth: root.vertical ? 12 : 1
        implicitHeight: root.vertical ? 1 : 14
        color: root.fg
        opacity: 0.25
    }

    component SiteIcon: Item {
        id: icon
        property var tab
        implicitWidth: root.iconLength
        implicitHeight: root.iconLength

        // The icon the page reported, as it reported it: a page that offers
        // one for dark backgrounds is shown that one under a dark theme.
        Image {
            id: image
            anchors.fill: parent
            source: icon.tab && icon.tab.icon ? "file://" + icon.tab.icon : ""
            sourceSize: Qt.size(32, 32)
            smooth: true
            asynchronous: true
            visible: status === Image.Ready
        }
        Rectangle {
            anchors.fill: parent
            radius: 4
            visible: image.status !== Image.Ready
            // A page with no icon: its host's first letters, in the bar's own
            // colours, since only Space colours mark anything here.
            color: Qt.alpha(root.fg, 0.16)
            Text {
                anchors.centerIn: parent
                text: icon.tab ? root.hostCode(icon.tab.host) : ""
                color: root.fg
                font.family: root.font
                font.pixelSize: 8
                font.bold: true
            }
        }
        // An Agent tab: a dot at the top left, opposite the sound mark.
        Rectangle {
            visible: !!icon.tab && icon.tab.agent
            x: -3
            y: -3
            width: 6
            height: 6
            radius: 3
            color: Commons.Color.accent
            border.width: 1
            border.color: root.barGround
        }
        // A Sounding tab, or one in a Tab window, at the top right.
        Text {
            visible: !!icon.tab && (icon.tab.sound !== "" || icon.tab.poppedOut)
            text: !icon.tab ? "" : icon.tab.sound === "playing" ? "󰕾" : icon.tab.sound === "muted"
                                                                  ? "󰖁" : "󰁜"
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.rightMargin: -6
            anchors.topMargin: -5
            color: root.fg
            font.family: root.font
            font.pixelSize: 8
        }
    }

    component TabCell: Item {
        id: cell
        property var tab
        property real pad: 3
        property bool pinned: false
        readonly property bool isActive: !!tab && root.activeId === tab.id

        implicitWidth: root.vertical ? root.barSize - 8 : site.implicitWidth + pad * 2
        implicitHeight: root.vertical ? site.implicitHeight + pad * 2 + 4 : root.barSize - 12

        Rectangle {
            anchors.fill: parent
            radius: 4
            color: Qt.alpha(root.fg, cellMouse.containsMouse ? 0.08 : 0)
        }
        // The active tab is underlined in its Space's colour.
        Rectangle {
            visible: cell.isActive
            x: root.vertical ? site.x - 5 : site.x
            y: root.vertical ? site.y : site.y + site.height + 2
            width: root.vertical ? 2 : root.iconLength
            height: root.vertical ? root.iconLength : 2
            radius: 1
            color: root.space ? root.space.colour : root.fg
        }
        // Every tab but the active one sits back a little, so a row of
        // favicons reads as a row rather than a wall of colour.
        SiteIcon {
            id: site
            tab: cell.tab
            opacity: cell.isActive || cellMouse.containsMouse ? 1 : 0.75
            x: (cell.width - width) / 2
            anchors.verticalCenter: parent.verticalCenter
        }
        MouseArea {
            id: cellMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton
            onClicked: function (mouse) {
                if (mouse.button === Qt.MiddleButton)
                    root.send({
                                  "do": "close",
                                  "tab": cell.tab.id
                              });
                else if (mouse.button === Qt.RightButton)
                    root.openPopup("menu", cell, cell.tab);
                else
                    root.activate(cell.tab);
            }
            onEntered: if (root.bar)
                           root.bar.showTooltip(cell, cell.tab.title + (cell.tab.agent ? "\n" + qsTr(
                                                                                             "Agent: %1").arg(
                                                                                             cell.tab.agentAct) :
                                                                                         ""))
            onExited: if (root.bar)
                          root.bar.hideTooltip(cell)
        }
    }

    // One Space. The active one is filled with its colour, and every one keeps
    // its width whichever is active, so a switch moves nothing. Clicking the
    // active one opens every Space; clicking another switches to it.
    component SpaceSwitch: Item {
        id: sw
        property var space
        readonly property bool current: root.live && space.id === root.browser.activeSpace
        implicitWidth: root.vertical ? root.barSize - 8 : swRow.implicitWidth + Style.space(12)
        implicitHeight: root.vertical ? 18 : root.barSize - 10

        Rectangle {
            anchors.fill: parent
            radius: 4
            color: sw.current ? sw.space.colour : Qt.alpha(root.fg, swMouse.containsMouse ? 0.1 : 0)
        }
        Row {
            id: swRow
            anchors.centerIn: parent
            spacing: Style.space(5)
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: 7
                height: 7
                radius: 2
                color: sw.current ? root.barGround : sw.space.colour
            }
            Text {
                visible: !root.vertical
                anchors.verticalCenter: parent.verticalCenter
                text: BarModel.spaceLabel(sw.space.name, root.spaceLetters)
                textFormat: Text.PlainText
                color: sw.current ? root.barGround : root.fg
                opacity: sw.current ? 1 : 0.75
                font.family: root.font
                font.pixelSize: Style.font.body
            }
            Text {
                visible: !root.vertical
                anchors.verticalCenter: parent.verticalCenter
                text: sw.space.count
                color: sw.current ? root.barGround : root.fg
                opacity: sw.current ? 0.8 : 0.45
                font.family: root.font
                font.pixelSize: Style.font.body
            }
        }
        MouseArea {
            id: swMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                if (sw.current) {
                    root.openPopup("spaces", sw);
                    return;
                }
                root.send({
                              "do": "space",
                              "space": sw.space.id
                          });
                root.raise(null);
            }
            onEntered: if (root.bar)
                           root.bar.showTooltip(sw, sw.current ? qsTr("%1: every Space and tab").arg(
                                                                     sw.space.name) : qsTr(
                                                                     "Switch to %1").arg(
                                                                     sw.space.name))
            onExited: if (root.bar)
                          root.bar.hideTooltip(sw)
        }
    }

    component BarText: Item {
        id: label
        property string text
        property string tooltip
        property color colour: root.fg
        property bool dim: false
        signal clicked
        implicitWidth: root.vertical ? root.barSize - 8 : glyph.implicitWidth + Style.space(10)
        implicitHeight: root.vertical ? glyph.implicitHeight + Style.space(8) : root.barSize - 8
        opacity: dim ? 0.35 : 1

        Rectangle {
            anchors.fill: parent
            radius: 4
            color: Qt.alpha(root.fg, labelMouse.containsMouse ? 0.1 : 0)
        }
        Text {
            id: glyph
            anchors.centerIn: parent
            text: label.text
            color: label.colour
            font.family: root.font
            font.pixelSize: Style.font.body
        }
        MouseArea {
            id: labelMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: label.clicked()
            onEntered: if (root.bar && label.tooltip)
                           root.bar.showTooltip(label, label.tooltip)
            onExited: if (root.bar)
                          root.bar.hideTooltip(label)
        }
    }

    component PopupRow: Item {
        id: row
        property var tab
        property string glyph
        property color glyphColour: root.popupFg
        property string label
        property string detail
        property bool current: false
        property bool indent: false
        property bool agent: false
        signal picked
        width: parent ? parent.width : 300
        implicitHeight: detail ? 40 : 28

        Rectangle {
            anchors.fill: parent
            radius: 4
            color: Qt.alpha(root.popupFg, rowMouse.containsMouse ? 0.1 : row.current ? 0.06 : 0)
        }
        SiteIcon {
            visible: !!row.tab
            tab: row.tab
            x: row.indent ? 14 : 6
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            visible: !row.tab && row.glyph !== ""
            x: 8
            anchors.verticalCenter: parent.verticalCenter
            text: row.glyph
            color: row.glyphColour
            font.family: root.font
            font.pixelSize: Style.font.body
        }
        Column {
            anchors.left: parent.left
            anchors.leftMargin: (row.indent ? 14 : 6) + 26
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            Text {
                width: parent.width
                text: row.label
                textFormat: Text.PlainText
                elide: Text.ElideRight
                color: root.popupFg
                font.family: root.font
                font.pixelSize: Style.font.body
                font.bold: row.current
            }
            Text {
                visible: row.detail !== ""
                width: parent.width
                text: row.detail
                textFormat: Text.PlainText
                elide: Text.ElideRight
                color: row.agent ? Commons.Color.accent : root.popupFg
                opacity: row.agent ? 1 : 0.6
                font.family: root.font
                font.pixelSize: Style.font.small || Style.font.body - 2
            }
        }
        MouseArea {
            id: rowMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: row.picked()
        }
    }

    // A tab in the popup of every Space. In the Space on show it can be
    // dragged to another place among its own section's rows.
    component DraggableTab: PopupRow {
        id: dragged
        property var spaceId
        property bool movable: false
        required property int index
        property int sectionCount: 1
        property real restY: 0
        indent: true
        label: tab ? tab.title : ""
        agent: !!tab && tab.agent
        detail: !tab ? "" : tab.agent ? qsTr("Agent: %1").arg(tab.agentAct) : tab.poppedOut ? qsTr(
                                                                                                  "In a Tab window") :
                                                                                              ""
        onPicked: {
            root.activate(tab, spaceId);
            root.closePopup();
        }

        DragHandler {
            enabled: dragged.movable
            yAxis.enabled: true
            xAxis.enabled: false
            onActiveChanged: {
                if (active) {
                    dragged.restY = dragged.y;
                    return;
                }
                const steps = Math.round((dragged.y - dragged.restY) / dragged.height);
                const destination = Math.max(0, Math.min(dragged.sectionCount - 1, dragged.index
                                                         + steps));
                dragged.y = dragged.restY;
                if (destination !== dragged.index)
                    root.send({
                                  "do": "move",
                                  "tab": dragged.tab.id,
                                  "index": destination
                              });
            }
        }
    }

    PopupCard {
        id: card
        anchorItem: root.popupAnchor
        bar: root.bar
        owner: root
        open: root.popup !== ""
        contentWidth: card.fittedContentWidth(root.popup === "menu" ? Style.space(200) : Style.space(
                                                                          360))
        contentHeight: card.fittedContentHeight(body.item ? body.item.implicitHeight : 0)

        Flickable {
            anchors.fill: parent
            contentHeight: body.item ? body.item.implicitHeight : 0
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            interactive: root.popup !== "spaces"

            Loader {
                id: body
                width: parent.width
                sourceComponent: root.popup === "menu" ? menuBody : root.popup === "spaces"
                                                         ? spacesBody : root.popup === "status"
                                                           ? statusBody : null
            }
        }
    }

    Component {
        id: menuBody
        Column {
            width: parent ? parent.width : 200
            readonly property var tab: root.menuTab
            readonly property bool pinned: !!(tab && root.space && root.space.pins.some(p => p.id
                                                                                             === tab.id))
            PopupRow {
                glyph: "󰐃"
                label: parent.pinned ? qsTr("Unpin") : qsTr("Pin")
                onPicked: {
                    root.send({
                                  "do": parent.pinned ? "unpin" : "pin",
                                  "tab": parent.tab.id
                              });
                    root.closePopup();
                }
            }
            PopupRow {
                glyph: "󰆏"
                label: qsTr("Duplicate")
                onPicked: {
                    root.send({
                                  "do": "duplicate",
                                  "tab": parent.tab.id
                              });
                    root.closePopup();
                }
            }
            PopupRow {
                glyph: "󰁜"
                label: parent.tab && parent.tab.poppedOut ? qsTr("Put back") : qsTr("Pop out")
                onPicked: {
                    root.send({
                                  "do": "popout",
                                  "tab": parent.tab.id
                              });
                    root.closePopup();
                }
            }
            PopupRow {
                glyph: "󰅖"
                label: qsTr("Close")
                onPicked: {
                    root.send({
                                  "do": "close",
                                  "tab": parent.tab.id
                              });
                    root.closePopup();
                }
            }
        }
    }

    Component {
        id: spacesBody
        Column {
            width: parent ? parent.width : 360
            spacing: Style.space(6)
            Repeater {
                model: root.live ? root.browser.spaces : []
                Column {
                    id: spaceBlock
                    required property var modelData
                    readonly property bool onShow: modelData.id === root.browser.activeSpace
                    width: parent.width
                    spacing: 1
                    PopupRow {
                        glyph: "■"
                        glyphColour: spaceBlock.modelData.colour
                        label: spaceBlock.modelData.name + "  ·  " + spaceBlock.modelData.count
                        current: spaceBlock.onShow
                        onPicked: {
                            root.send({
                                          "do": "space",
                                          "space": spaceBlock.modelData.id
                                      });
                            root.raise(null);
                            root.closePopup();
                        }
                    }
                    Repeater {
                        model: spaceBlock.modelData.pins
                        DraggableTab {
                            required property var modelData
                            tab: modelData
                            spaceId: spaceBlock.modelData.id
                            movable: spaceBlock.onShow
                            sectionCount: spaceBlock.modelData.pins.length
                            current: modelData.id === spaceBlock.modelData.active
                        }
                    }
                    Repeater {
                        model: spaceBlock.modelData.tabs
                        DraggableTab {
                            required property var modelData
                            tab: modelData
                            spaceId: spaceBlock.modelData.id
                            movable: spaceBlock.onShow
                            sectionCount: spaceBlock.modelData.tabs.length
                            current: modelData.id === spaceBlock.modelData.active
                        }
                    }
                }
            }
        }
    }

    Component {
        id: statusBody
        Column {
            width: parent ? parent.width : 360
            PopupRow {
                visible: root.live && !!root.browser.status.recording
                glyph: "󰑋"
                label: qsTr("Recording a problem")
                detail: qsTr("Stop it from the window's Status island")
            }
            Repeater {
                model: root.downloads ? root.downloads.files : []
                PopupRow {
                    required property var modelData
                    glyph: "󰇚"
                    label: modelData.name
                    detail: qsTr("%1% downloaded").arg(modelData.progress)
                }
            }
            PopupRow {
                visible: root.live && !!root.browser.status.release
                glyph: "󰚰"
                label: visible ? qsTr("Omaweb %1 is out").arg(root.browser.status.release.version) :
                                 ""
                detail: visible ? root.browser.status.release.instruction : ""
            }
        }
    }
}
