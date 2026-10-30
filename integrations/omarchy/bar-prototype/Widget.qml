import QtQuick
import Quickshell
import Quickshell.Io
import Quickshell.Wayland
import qs.Commons
import qs.Commons as Commons
import qs.Ui

// PROTOTYPE, throwaway: omaweb's tabs in the Omarchy bar, fed by
// fake-omaweb.py over a Unix socket. Placed twice: role "tabs" shows the
// Space, pins and tabs; role "navigation" shows back, forward, reload and
// the host. It takes no room while nothing answers on the socket.
BarWidget {
    id: root
    moduleName: "omaweb.bar-prototype"

    readonly property string role: setting("role", "tabs")
    readonly property string appId: setting("appId", "dev.omaweb.prototype")
    readonly property int titleWidth: Number(setting("titleWidth", 200))
    readonly property real maxShare: Number(setting("maxWidthPercent", 40)) / 100
    readonly property string socketPath: (Quickshell.env("XDG_RUNTIME_DIR") || "/tmp")
                                         + "/omaweb/bar-prototype.sock"

    property var browser: null
    readonly property bool live: socket.connected && browser !== null
    readonly property var toplevel: ToplevelManager.activeToplevel
    readonly property bool focused: toplevel !== null && toplevel.appId === appId
    readonly property var space: live ? browser.spaces.find(s => s.id === browser.activeSpace) :
                                        null
    readonly property var others: live ? browser.spaces.filter(s => s.id !== browser.activeSpace) :
                                         []
    readonly property var activeId: space ? space.active : null
    readonly property var downloads: live ? browser.status.downloads : []

    readonly property color fg: bar ? bar.barForeground : Commons.Color.foreground
    readonly property color popupFg: Commons.Color.popups.text
    readonly property string font: bar ? bar.fontFamily : Style.font.family

    // Which popup is open ("menu", "spaces", "status" or ""), and what it is for.
    property string popup: ""
    property Item popupAnchor: root
    property var menuTab: null
    property real wheelAccumulator: 0

    function close() {
        popup = "";
    }

    function send(message) {
        if (!socket.connected)
            return;
        socket.write(JSON.stringify(message) + "\n");
        socket.flush();
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

    function hostCode(host) {
        var h = String(host || "").replace(/^www\./, "");
        return h.slice(0, 2);
    }

    function hostTint(host) {
        var hash = 0;
        for (var i = 0; i < host.length; i++)
            hash = (hash * 31 + host.charCodeAt(i)) >>> 0;
        return Qt.hsla((hash % 360) / 360, 0.45, 0.42, 1);
    }

    // --- overflow: pins and the active tab always show; the other tabs shrink
    // their spacing, then the ones furthest from the active tab go to "+N".

    readonly property real barLength: {
        var w = root.QsWindow.window;
        if (!w)
            return 1600;
        return vertical ? w.height : w.width;
    }
    readonly property real maxLength: barLength * maxShare
    readonly property real iconLength: 16
    readonly property real fixedLength: {
        if (!live || role !== "tabs")
            return 0;
        var along = item => item ? (vertical ? item.implicitHeight : item.implicitWidth) : 0;
        var total = along(spaceChip) + along(othersRow) + along(statusMark) + grid.spacing * 6;
        var pins = space.pins.length;
        total += pins * (iconLength + 8) + (pins ? along(divider) : 0);
        if (!vertical)
            total += Math.min(titleWidth, titleMetrics.advanceWidth) + 8;
        return total + iconLength + 8;
    }
    readonly property var tabLayout: {
        if (!live || role !== "tabs")
            return {
                pad: 3,
                tabs: [],
                hidden: 0
            };
        var tabs = space.tabs;
        var rest = tabs.filter(t => t.id !== activeId).length;
        var room = maxLength - fixedLength;
        var pads = [4, 3, 2, 1];
        for (var p = 0; p < pads.length; p++) {
            if (rest * (iconLength + pads[p] * 2) <= room)
                return {
                    pad: pads[p],
                    tabs: tabs,
                    hidden: 0
                };
        }
        var fit = Math.max(0, Math.floor((room - 30) / (iconLength + 2)));
        var at = Math.max(0, tabs.findIndex(t => t.id === activeId));
        var ranked = tabs.map((t, i) => ({
            t: t,
            i: i
        })).filter(x => x.t.id !== activeId).sort((a, b) => Math.abs(a.i - at) - Math.abs(b.i
                                                                                          - at)).slice(
                0, fit);
        var keep = {};
        ranked.forEach(x => keep[x.t.id] = true);
        var shown = tabs.filter(t => t.id === activeId || keep[t.id]);
        return {
            pad: 1,
            tabs: shown,
            hidden: tabs.length - shown.length
        };
    }

    TextMetrics {
        id: titleMetrics
        font.family: root.font
        font.pixelSize: Style.font.body
        text: {
            if (!root.space)
                return "";
            var all = root.space.pins.concat(root.space.tabs);
            var t = all.find(x => x.id === root.activeId);
            return t ? t.title : "";
        }
    }

    visible: live
    implicitWidth: !live ? 0 : (vertical ? barSize : grid.implicitWidth + Style.space(8))
    implicitHeight: !live ? 0 : (vertical ? grid.implicitHeight + Style.space(8) : barSize)

    Socket {
        id: socket
        path: root.socketPath
        parser: SplitParser {
            onRead: data => {
                try {
                    var message = JSON.parse(data);
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

    Timer {
        interval: 1000
        repeat: true
        running: !socket.connected
        triggeredOnStart: true
        onTriggered: socket.connected = true
    }

    // Scrolling anywhere on the tabs moves to the previous or next tab.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.NoButton
        enabled: root.role === "tabs"
        onWheel: function (wheel) {
            root.wheelAccumulator += wheel.angleDelta.y || wheel.angleDelta.x;
            if (Math.abs(root.wheelAccumulator) < 120)
                return;
            root.send({
                          do: "cycle",
                          step: root.wheelAccumulator > 0 ? -1 : 1
                      });
            root.wheelAccumulator = 0;
        }
    }

    Grid {
        id: grid
        anchors.centerIn: parent
        columns: root.vertical ? 1 : 100
        spacing: 2
        verticalItemAlignment: Grid.AlignVCenter
        horizontalItemAlignment: Grid.AlignHCenter
        opacity: root.focused ? 1 : 0.5
        Behavior on opacity {
            NumberAnimation {
                duration: 160
            }
        }

        // ----- role: tabs

        SpaceChip {
            id: spaceChip
            visible: root.role === "tabs"
            space: root.space
            onClicked: root.openPopup("spaces", spaceChip)
        }

        Repeater {
            model: root.role === "tabs" && root.space ? root.space.pins : []
            TabCell {
                required property var modelData
                tab: modelData
                pad: 4
            }
        }

        Rectangle {
            id: divider
            visible: root.role === "tabs" && root.space && root.space.pins.length > 0
                     && root.space.tabs.length > 0
            implicitWidth: root.vertical ? 12 : 1
            implicitHeight: root.vertical ? 1 : 12
            color: root.fg
            opacity: 0.3
        }

        Repeater {
            model: root.role === "tabs" ? root.tabLayout.tabs : []
            TabCell {
                required property var modelData
                tab: modelData
                pad: root.tabLayout.pad
            }
        }

        BarText {
            id: plusN
            visible: root.role === "tabs" && root.tabLayout.hidden > 0
            text: "+" + root.tabLayout.hidden
            tooltip: root.tabLayout.hidden + " more tabs"
            onClicked: root.openPopup("spaces", plusN)
        }

        Row {
            id: othersRow
            visible: root.role === "tabs" && root.others.length > 0
            spacing: Style.space(8)
            leftPadding: root.vertical ? 0 : Style.space(6)
            Repeater {
                model: root.others
                OtherSpace {
                    required property var modelData
                    space: modelData
                }
            }
        }

        BarText {
            id: statusMark
            visible: root.role === "tabs" && root.downloads.length > 0
            text: {
                var d = root.downloads.find(x => x.progress < 100);
                return root.vertical || !d ? "󰇚" : "󰇚 " + d.progress + "%";
            }
            tooltip: root.downloads.map(d => d.name + " " + d.progress + "%").join("\n")
            onClicked: root.openPopup("status", statusMark)
        }

        // ----- role: navigation

        BarText {
            visible: root.role === "navigation"
            text: "󰁍"
            dim: !(root.live && root.browser.nav.canBack)
            tooltip: "Back"
            onClicked: root.send({
                                     do: "back"
                                 })
        }
        BarText {
            visible: root.role === "navigation"
            text: "󰁔"
            dim: !(root.live && root.browser.nav.canForward)
            tooltip: "Forward"
            onClicked: root.send({
                                     do: "forward"
                                 })
        }
        BarText {
            visible: root.role === "navigation"
            text: "󰑐"
            tooltip: "Reload"
            onClicked: root.send({
                                     do: "reload"
                                 })
        }
        BarText {
            visible: root.role === "navigation"
            text: root.vertical || !root.live ? "󰌾" : "󰌾  " + root.browser.nav.host
            tooltip: "Open the Omnibar"
            onClicked: root.send({
                                     do: "omnibar"
                                 })
        }
    }

    // ----- pieces

    component SiteIcon: Item {
        id: icon
        property var tab
        implicitWidth: root.iconLength
        implicitHeight: root.iconLength

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
            color: icon.tab ? root.hostTint(icon.tab.host) : "grey"
            Text {
                anchors.centerIn: parent
                text: icon.tab ? root.hostCode(icon.tab.host) : ""
                color: "white"
                font.family: root.font
                font.pixelSize: 8
                font.bold: true
            }
        }
        // Agent stripe under the icon.
        Rectangle {
            visible: icon.tab && icon.tab.agent
            anchors.top: parent.bottom
            anchors.topMargin: 2
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width
            height: 2
            radius: 1
            color: Commons.Color.accent
        }
        // Sound or popped-out mark at the top right.
        Text {
            visible: icon.tab && (icon.tab.audio !== "" || icon.tab.poppedOut)
            text: !icon.tab ? "" : icon.tab.audio === "playing" ? "󰕾" : icon.tab.audio === "muted"
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
        readonly property bool isActive: tab && root.activeId === tab.id
        readonly property bool showTitle: isActive && !root.vertical

        implicitWidth: root.vertical ? root.barSize : site.implicitWidth + pad * 2 + (showTitle
                                                                                      ? title.width
                                                                                        + Style.space(
                                                                                            6) : 0) + (
                                           isActive ? 4 : 0)
        implicitHeight: root.vertical ? site.implicitHeight + pad * 2 + 4 : root.barSize

        Rectangle {
            anchors.fill: parent
            anchors.margins: root.vertical ? 2 : 0
            anchors.topMargin: root.vertical ? 2 : 4
            anchors.bottomMargin: root.vertical ? 2 : 4
            radius: 4
            color: Qt.alpha(root.fg, 0.12)
            visible: cell.isActive
        }
        SiteIcon {
            id: site
            tab: cell.tab
            x: root.vertical ? (cell.width - width) / 2 : cell.pad + (cell.isActive ? 2 : 0)
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            id: title
            visible: cell.showTitle
            anchors.left: site.right
            anchors.leftMargin: Style.space(6)
            anchors.verticalCenter: parent.verticalCenter
            width: Math.min(root.titleWidth, implicitWidth)
            text: cell.tab ? cell.tab.title : ""
            textFormat: Text.PlainText
            elide: Text.ElideRight
            color: root.fg
            font.family: root.font
            font.pixelSize: Style.font.body
        }
        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton
            onClicked: function (mouse) {
                if (mouse.button === Qt.MiddleButton)
                    root.send({
                                  do: "close",
                                  tab: cell.tab.id
                              });
                else if (mouse.button === Qt.RightButton)
                    root.openPopup("menu", cell, cell.tab);
                else
                    root.send({
                                  do: "activate",
                                  tab: cell.tab.id
                              });
            }
            onEntered: if (root.bar)
                           root.bar.showTooltip(cell, cell.tab.title + (cell.tab.agentAct
                                                                        ? "\nAgent: "
                                                                          + cell.tab.agentAct : ""))
            onExited: if (root.bar)
                          root.bar.hideTooltip(cell)
        }
    }

    component SpaceChip: Item {
        id: chip
        property var space
        signal clicked
        implicitWidth: root.vertical ? root.barSize : chipRow.implicitWidth + Style.space(8)
        implicitHeight: root.vertical ? 20 : root.barSize

        Row {
            id: chipRow
            anchors.centerIn: parent
            spacing: Style.space(6)
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: 9
                height: 9
                radius: 2
                color: chip.space ? chip.space.colour : "grey"
            }
            Text {
                visible: !root.vertical
                anchors.verticalCenter: parent.verticalCenter
                text: chip.space ? chip.space.name : ""
                color: chip.space ? chip.space.colour : root.fg
                font.family: root.font
                font.pixelSize: Style.font.body
                font.bold: true
            }
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: chip.clicked()
        }
    }

    component OtherSpace: Item {
        id: other
        property var space
        implicitWidth: root.vertical ? root.barSize : otherRow.implicitWidth
        implicitHeight: root.vertical ? 14 : root.barSize
        opacity: 0.75

        Row {
            id: otherRow
            anchors.centerIn: parent
            spacing: Style.space(4)
            Rectangle {
                visible: root.vertical
                anchors.verticalCenter: parent.verticalCenter
                width: 7
                height: 7
                radius: 2
                color: other.space.colour
            }
            Text {
                visible: !root.vertical
                text: other.space.name
                color: other.space.colour
                font.family: root.font
                font.pixelSize: Style.font.body
            }
            Text {
                visible: !root.vertical
                text: other.space.count
                color: root.fg
                opacity: 0.6
                font.family: root.font
                font.pixelSize: Style.font.body
            }
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: root.send({
                                     do: "space",
                                     space: other.space.id
                                 })
        }
    }

    component BarText: Item {
        id: label
        property string text
        property string tooltip
        property bool dim: false
        signal clicked
        implicitWidth: root.vertical ? root.barSize : glyph.implicitWidth + Style.space(10)
        implicitHeight: root.vertical ? glyph.implicitHeight + Style.space(8) : root.barSize
        opacity: dim ? 0.35 : 1

        Text {
            id: glyph
            anchors.centerIn: parent
            text: label.text
            color: root.fg
            font.family: root.font
            font.pixelSize: Style.font.body
        }
        MouseArea {
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
        property string label
        property string detail
        property bool current: false
        property bool indent: false
        signal picked
        width: parent ? parent.width : 300
        implicitHeight: detail ? 40 : 28

        Rectangle {
            anchors.fill: parent
            radius: 4
            color: Qt.alpha(root.popupFg, rowMouse.containsMouse ? 0.1 : row.current ? 0.06 : 0)
        }
        SiteIcon {
            id: rowIcon
            visible: !!row.tab
            tab: row.tab
            x: row.indent ? 14 : 6
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            id: rowGlyph
            visible: !row.tab && row.glyph !== ""
            x: 8
            anchors.verticalCenter: parent.verticalCenter
            text: row.glyph
            color: root.popupFg
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
                color: root.popupFg
                opacity: 0.6
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
                label: parent.pinned ? "Unpin" : "Pin"
                onPicked: {
                    root.send({
                                  do: parent.pinned ? "unpin" : "pin",
                                  tab: parent.tab.id
                              });
                    root.close();
                }
            }
            PopupRow {
                glyph: "󰆏"
                label: "Duplicate"
                onPicked: {
                    root.send({
                                  do: "duplicate",
                                  tab: parent.tab.id
                              });
                    root.close();
                }
            }
            PopupRow {
                glyph: "󰁜"
                label: parent.tab && parent.tab.poppedOut ? "Put back" : "Pop out"
                onPicked: {
                    root.send({
                                  do: "popout",
                                  tab: parent.tab.id
                              });
                    root.close();
                }
            }
            PopupRow {
                glyph: "󰅖"
                label: "Close"
                onPicked: {
                    root.send({
                                  do: "close",
                                  tab: parent.tab.id
                              });
                    root.close();
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
                    width: parent.width
                    spacing: 1
                    PopupRow {
                        glyph: "■"
                        label: spaceBlock.modelData.name + "  ·  " + spaceBlock.modelData.count
                        current: spaceBlock.modelData.id === root.browser.activeSpace
                        onPicked: {
                            root.send({
                                          do: "space",
                                          space: spaceBlock.modelData.id
                                      });
                            root.close();
                        }
                    }
                    Repeater {
                        model: spaceBlock.modelData.pins.concat(spaceBlock.modelData.tabs)
                        PopupRow {
                            required property var modelData
                            indent: true
                            tab: modelData
                            label: modelData.title
                            detail: modelData.agent ? "Agent: " + modelData.agentAct :
                                                      modelData.poppedOut ? "In a Tab window" :
                                                                            modelData.audio
                                                                            ? "Sound "
                                                                              + modelData.audio : ""
                            current: modelData.id === spaceBlock.modelData.active
                            onPicked: {
                                root.send({
                                              do: "activate",
                                              tab: modelData.id
                                          });
                                root.close();
                            }
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
            Repeater {
                model: root.downloads
                PopupRow {
                    required property var modelData
                    glyph: modelData.progress < 100 ? "󰇚" : "󰄬"
                    label: modelData.name
                    detail: modelData.progress < 100 ? modelData.progress + "% downloaded" : "Done"
                }
            }
        }
    }
}
