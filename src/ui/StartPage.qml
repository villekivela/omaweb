import QtQuick
import Omaweb
import qs.Commons

// What Omaweb shows where a page would be when there is none to show: a Space
// at rest, `about:blank`, or a new-tab request that has not been given a
// destination yet. It is the Omnibar at rest over the night road. The Omnibar
// itself is the window's own, drawn resting on this page's horizon; the page
// is the road under it and the one line that names the Shortcut sheet.
//
// It costs no engine. The road moves only while the page is on show and the
// window is the reader's. It runs under the whole window, the sidebar standing
// over it, unless the page stands in one pane of a split. A reader who turns
// the road off gets the sidebar's fill instead, or, over a page a new tab was
// asked from, that page blurred under the sheet tint, as the Shortcut sheet
// shows it.
Item {
    id: root
    objectName: "startPage"

    property var colors
    property bool privateWindow: false
    property bool open: false
    // The Settings interface section's road. Off, the backdrop below takes its
    // place.
    property bool roadEnabled: true
    // The Settings interface section's CRT glass over the road.
    property bool glassEnabled: true
    // The reader asked for less motion: the road holds one still frame, with
    // no clock, no drive and no band or flicker on its glass.
    property bool reducedMotion: false
    // The window is on screen and holds the keyboard.
    property bool windowActive: false
    // A destination was committed from the Omnibar and its page has not
    // painted yet.
    property bool driving: false
    // How far below the horizon the Omnibar's field ends, so the hint sits
    // under it rather than behind it.
    property real fieldBelowHorizon: 32
    // The page the Start page was summoned over, blurred under the sheet tint
    // when the road is off. Must not be an ancestor of this item.
    property Item pageSource: null
    // Whether the page fades as it gives way to a page.
    property bool ease: true
    // Set by a drive and kept while the Start page fades out after it, so the
    // fade is the slower one and the road, easing out of its drive, is still
    // moving and lit as the page takes over.
    property bool drove: false
    onDrivingChanged: if (driving)
                          drove = true
    // The widest the page area can be, which is the width the road is drawn
    // at. The page area narrows and widens with the sidebar, and a road drawn
    // at the window's width only moves when it does, where one drawn at the
    // page area's would draw itself again for every width the seam passes.
    property real roadWidth: width
    // How much of the window lies left of the page, which the road runs under
    // to the window's edge. None in a pane of a split, where the road stays in
    // the pane.
    property real roadReach: 0

    // Where the Omnibar's field rests.
    readonly property real horizonY: road.sceneItem ? road.sceneItem.horizonY : height / 2
    readonly property int roadFrames: road.frames
    readonly property bool roadRunning: road.drawing
    // The road, while it is drawn: the Scene host whose picture the
    // Omnibar's glass blurs at rest and whose light falls on its rim.
    readonly property Item scene: root.roadEnabled ? road : null

    // It fades in under the Omnibar arriving, and out as the page it gave way
    // to takes over. While it fades out a click is that page's.
    opacity: open ? 1 : 0
    visible: opacity > 0

    // Once it has gone, the next time it comes back it starts at rest.
    onVisibleChanged: if (!visible)
                          drove = false

    // The fade after a drive is the road reporting the page's arrival, as the
    // loading indicator reports a load, so it plays after a key's Return as
    // after a click, and only reduced motion stills it.
    Behavior on opacity {
        enabled: root.ease || root.drove && !SystemMotion.reduced

        // Leaving after a drive is slower: the page is already there under
        // it, so the longer fade costs no waiting.
        NumberAnimation {
            duration: root.drove && !root.open ? 420 : 180
            easing.type: root.drove && !root.open ? Easing.InOutQuad : Easing.OutCubic
        }
    }

    // Over a page the Start page stands in for it, so the page beneath hears
    // nothing until the Start page has gone.
    MouseArea {
        anchors.fill: parent
        enabled: root.open
        hoverEnabled: true
        acceptedButtons: Qt.AllButtons
        onWheel: function (wheel) {
            wheel.accepted = true;
        }
    }

    PageBackdrop {
        objectName: "startPageBackdrop"
        anchors.fill: parent
        visible: !root.roadEnabled
        source: root.pageSource
        tint: root.pageSource ? root.colors.sheet : root.colors.sidebar
    }

    // A road in a pane stays in it; one under the window runs past the page's
    // left edge, under the sidebar.
    clip: root.roadReach <= 0

    SceneHost {
        id: road
        objectName: "startPageScene"
        x: root.roadReach > 0 ? -root.roadReach : Math.round((root.width - width) / 2)
        width: root.roadReach > 0 ? root.roadWidth : Math.max(root.width, root.roadWidth)
        height: root.height
        visible: root.roadEnabled
        colors: root.colors
        unlit: root.privateWindow
        glass: root.glassEnabled
        reducedMotion: root.reducedMotion
        // What the reader sees of it: under the window, from the window's left
        // edge to the page's right one, and otherwise the page it is clipped
        // to.
        frame: root.roadReach > 0 ? Qt.rect(0, 0, root.roadReach + root.width, height) : Qt.rect(-x,
                                                                                                 0, root.width,
                                                                                                 height)
        running: root.visible && root.roadEnabled && root.windowActive
        navigating: root.driving ? 1 : 0
        scene: Component {
            NightRoad {}
        }
    }

    // The Shortcut sheet is summoned, not shown, so the page names the key:
    // a keycap in the accent and the word beside it, on a plate of the road's
    // dark so it reads over the lit horizon as well as the ground.
    Rectangle {
        id: hint
        objectName: "startPageHint"

        readonly property bool onRoad: root.roadEnabled && !!road.sceneItem
        readonly property color keyColor: onRoad ? road.sceneItem.roles.glow : root.colors.accent
        readonly property color wordColor: onRoad ? road.sceneItem.roles.light : root.colors.text

        anchors.horizontalCenter: parent.horizontalCenter
        y: root.horizonY + root.fieldBelowHorizon + Style.spacing.xl
        visible: !root.driving
        width: hintRow.implicitWidth + Style.spacing.lg * 2
        height: hintRow.implicitHeight + Style.spacing.sm * 2
        radius: 3
        color: Qt.rgba(0, 0, 0, root.roadEnabled ? 0.45 : 0)
        Accessible.role: Accessible.StaticText
        Accessible.name: qsTr("Question mark shows the keyboard shortcuts")

        Row {
            id: hintRow
            anchors.centerIn: parent
            spacing: Style.spacing.md

            Rectangle {
                width: Math.max(height, keyText.implicitWidth + Style.spacing.md)
                height: keyText.implicitHeight + Style.spacing.xs * 2
                anchors.verticalCenter: parent.verticalCenter
                radius: 3
                color: "transparent"
                border.width: 1
                border.color: hint.keyColor

                Text {
                    id: keyText
                    anchors.centerIn: parent
                    text: "?"
                    color: hint.keyColor
                    font.family: Style.font.family
                    font.pixelSize: Style.font.body
                    font.bold: true
                    Accessible.ignored: true
                }
            }

            Text {
                objectName: "startPageHintWord"
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("shortcuts")
                color: hint.wordColor
                opacity: 0.85
                font.family: Style.font.family
                font.pixelSize: Style.font.body
                Accessible.ignored: true
            }
        }
    }
}
