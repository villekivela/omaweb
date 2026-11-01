import QtQuick
import Quickshell
import Quickshell.Wayland
import qs.Commons
import qs.Commons as Commons
import qs.Ui
import "BarModel.mjs" as BarModel

// Omaweb in the Omarchy bar (#906), placed first in the center when the
// reader sets its tabs to show here: the Space on show, which opens the tab
// switcher, then the page's back, forward and reload, the Location, and the
// Status island's marks while they apply.
//
// Everything it shows comes from the browser over the bar socket, one state a
// line, and everything it does goes back as one action a line; it reaches no
// page. It takes no room while no browser answers there.
BarWidget {
    id: root
    moduleName: "omaweb.bar"

    readonly property bool spaceLetters: setting("spaceNames", "full") === "letter"
    readonly property int locationWidth: Number(setting("locationWidth", 360))

    readonly property var browser: link.browser
    readonly property bool live: link.live
    readonly property var toplevel: ToplevelManager.activeToplevel
    // A Private window shares the main window's app id, so the main window
    // is known by its title too.
    readonly property bool focused: {
        if (toplevel === null || !live)
            return false;
        if (toplevel.appId === link.tabWindowAppId)
            return true;
        return toplevel.appId === link.appId && toplevel.title === browser.window;
    }
    readonly property var space: live ? browser.spaces.find(s => s.id === browser.activeSpace)
                                        || null : null
    readonly property var downloads: live && browser.status ? browser.status.downloads : null

    readonly property color fg: bar ? bar.barForeground : Commons.Color.foreground
    readonly property color barGround: bar ? bar.background : Commons.Color.background
    readonly property color popupFg: Commons.Color.popups.text
    readonly property string font: bar ? bar.fontFamily : Style.font.family
    readonly property int gap: Style.space(10)

    // The status popup, open over the mark that opened it.
    property bool statusOpen: false
    property Item statusAnchor: root

    // The switcher is this plugin's overlay, which the shell opens and closes.
    function toggleSwitcher() {
        const command = "omarchy-shell shell toggle omaweb.bar";
        if (root.bar && typeof root.bar.run === "function")
            root.bar.run(command);
        else
            Quickshell.execDetached(["sh", "-c", command]);
    }

    // The card calls this when a click lands outside it.
    function close() {
        statusOpen = false;
    }

    function openStatus(anchor) {
        statusOpen = !(statusOpen && statusAnchor === anchor);
        statusAnchor = anchor;
    }

    visible: live
    implicitWidth: !live ? 0 : (vertical ? barSize : grid.implicitWidth + gap * 2)
    implicitHeight: !live ? 0 : (vertical ? grid.implicitHeight + gap * 2 : barSize)

    BrowserLink {
        id: link
        socketPath: root.setting("socket", (Quickshell.env("XDG_RUNTIME_DIR") || "/tmp")
                                 + "/omaweb/bar.sock")
        appId: root.setting("appId", "omaweb")
        tabWindowAppId: root.setting("tabWindowAppId", "omaweb-tab")
        onLiveChanged: if (!live)
                           root.statusOpen = false
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

        // The Space on show, filled with its colour.
        Item {
            id: pill
            visible: !!root.space
            implicitWidth: root.vertical ? root.barSize - 8 : pillRow.implicitWidth + Style.space(
                                               12)

            implicitHeight: root.vertical ? 18 : root.barSize - 10

            Rectangle {
                anchors.fill: parent
                radius: 4
                color: root.space ? root.space.colour : "transparent"
                opacity: pillMouse.containsMouse ? 0.85 : 1
            }
            Row {
                id: pillRow
                anchors.centerIn: parent
                spacing: Style.space(5)
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 7
                    height: 7
                    radius: 2
                    color: root.barGround
                }
                Text {
                    visible: !root.vertical
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.space ? BarModel.spaceLabel(root.space.name, root.spaceLetters) : ""
                    textFormat: Text.PlainText
                    color: root.barGround
                    font.family: root.font
                    font.pixelSize: Style.font.body
                }
            }
            MouseArea {
                id: pillMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.toggleSwitcher()
                onEntered: if (root.bar && root.space)
                               root.bar.showTooltip(pill, qsTr("%1: every Space and tab").arg(
                                                        root.space.name))
                onExited: if (root.bar)
                              root.bar.hideTooltip(pill)
            }
        }

        BarText {
            text: "󰁍"
            dim: !(root.live && root.browser.location.canBack)
            tooltip: qsTr("Back")
            onClicked: link.send({
                                     "do": "back"
                                 })
        }
        BarText {
            text: "󰁔"
            dim: !(root.live && root.browser.location.canForward)
            tooltip: qsTr("Forward")
            onClicked: link.send({
                                     "do": "forward"
                                 })
        }
        BarText {
            text: "󰑐"
            tooltip: qsTr("Reload")
            onClicked: link.send({
                                     "do": "reload"
                                 })
        }

        // The Location, at a fixed width so a tab switch moves nothing.
        Item {
            id: location
            implicitWidth: root.vertical ? root.barSize - 4 : root.locationWidth
            implicitHeight: root.vertical ? 22 : root.barSize - 8

            Rectangle {
                anchors.fill: parent
                radius: 4
                color: Qt.alpha(root.fg, locationMouse.containsMouse ? 0.1 : 0)
            }
            // From the left, so the lock sits by back, forward and reload.
            Row {
                visible: !root.vertical
                anchors.left: parent.left
                anchors.leftMargin: 6
                anchors.verticalCenter: parent.verticalCenter
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
                    link.send({
                                  "do": "omnibar"
                              });
                    link.raise(null);
                }
                onEntered: if (root.bar && root.live)
                               root.bar.showTooltip(location, root.browser.location.title)
                onExited: if (root.bar)
                              root.bar.hideTooltip(location)
            }
        }

        // The Status island's marks, each only while it applies: a Problem
        // recording, the downloads running and a newer release.
        BarText {
            id: recordingMark
            visible: root.live && !!root.browser.status.recording
            text: "󰑋"
            colour: Commons.Color.urgent
            tooltip: qsTr("Recording a problem")
            onClicked: root.openStatus(recordingMark)
        }
        BarText {
            id: statusMark
            visible: !!root.downloads && root.downloads.running > 0
            text: root.vertical || !root.downloads ? "󰇚" : "󰇚 " + root.downloads.progress + "%"
            tooltip: root.downloads ? root.downloads.files.map(d => d.name + " " + d.progress
                                                                    + "%").join("\n") : ""
            onClicked: root.openStatus(statusMark)
        }
        BarText {
            id: releaseMark
            visible: root.live && !!root.browser.status.release
            text: root.vertical || !visible ? "󰚰" : "󰚰 " + root.browser.status.release.version
            tooltip: visible ? root.browser.status.release.instruction : ""
            onClicked: root.openStatus(releaseMark)
        }
    }

    // ----- pieces

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

    component StatusRow: Item {
        id: row
        property string glyph
        property string label
        property string detail
        width: parent ? parent.width : 300
        implicitHeight: detail ? 40 : 28

        Text {
            x: 8
            anchors.verticalCenter: parent.verticalCenter
            text: row.glyph
            color: root.popupFg
            font.family: root.font
            font.pixelSize: Style.font.body
        }
        Column {
            anchors.left: parent.left
            anchors.leftMargin: 32
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
                font.pixelSize: Style.font.bodySmall
            }
        }
    }

    PopupCard {
        id: card
        anchorItem: root.statusAnchor
        bar: root.bar
        owner: root
        open: root.statusOpen
        contentWidth: card.fittedContentWidth(Style.space(360))
        contentHeight: card.fittedContentHeight(status.implicitHeight)

        Column {
            id: status
            width: parent.width
            StatusRow {
                visible: root.live && !!root.browser.status.recording
                glyph: "󰑋"
                label: qsTr("Recording a problem")
                detail: qsTr("Stop it from the window's Status island")
            }
            Repeater {
                model: root.downloads ? root.downloads.files : []
                StatusRow {
                    required property var modelData
                    glyph: "󰇚"
                    label: modelData.name
                    detail: qsTr("%1% downloaded").arg(modelData.progress)
                }
            }
            StatusRow {
                visible: root.live && !!root.browser.status.release
                glyph: "󰚰"
                label: visible ? qsTr("Omaweb %1 is out").arg(root.browser.status.release.version) :
                                 ""
                detail: visible ? root.browser.status.release.instruction : ""
            }
        }
    }
}
