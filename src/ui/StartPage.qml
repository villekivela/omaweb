import QtQuick
import qs.Commons

// What Omaweb shows where a page would be when there is none to show: a Space
// at rest, `about:blank`, or a new-tab request that has not been given a
// destination yet. It is the Omnibar at rest over the night road. The Omnibar
// itself is the window's own, drawn resting on this page's horizon; the page
// is the road under it and the one line that names the Shortcut sheet.
//
// It costs no engine. The road moves only while the page is on show and the
// window is the reader's, and a reader who turns the road off gets the
// sidebar's fill instead.
Item {
    id: root
    objectName: "startPage"

    property var colors
    property bool privateWindow: false
    property bool open: false
    // The Settings interface section's road. Off, the page is the sidebar's
    // fill under the Omnibar.
    property bool roadEnabled: true
    // The window is on screen and holds the keyboard.
    property bool windowActive: false
    // A destination was committed from the Omnibar and its page has not
    // painted yet.
    property bool driving: false
    // How far below the horizon the Omnibar's field ends, so the hint sits
    // under it rather than behind it.
    property real fieldBelowHorizon: 32
    // The page the Start page was summoned over, blurred under the sidebar's
    // fill when the road is off. Must not be an ancestor of this item.
    property Item pageSource: null
    // Whether the page fades as it gives way to a page.
    property bool ease: true
    // Set by a drive and kept while the Start page fades out after it, so the
    // road goes on driving and glowing into the page rather than stopping
    // where the page arrived.
    property bool drove: false
    onDrivingChanged: if (driving)
                          drove = true
    // The widest the page area can be, which is the width the road is drawn
    // at. The page area narrows and widens with the sidebar, and a road drawn
    // at the window's width only moves when it does, where one drawn at the
    // page area's would draw itself again for every width the seam passes.
    property real roadWidth: width

    // Where the Omnibar's field rests.
    readonly property real horizonY: road.horizonY
    readonly property int roadFrames: road.frames
    readonly property bool roadRunning: road.running

    // It fades in under the Omnibar arriving, and out as the page it gave way
    // to takes over. While it fades out a click is that page's.
    opacity: open ? 1 : 0
    visible: opacity > 0

    // Once it has gone, the next time it comes back it starts at rest.
    onVisibleChanged: if (!visible)
                          drove = false

    Behavior on opacity {
        enabled: root.ease

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

    clip: true

    NightRoad {
        id: road
        x: Math.round((root.width - width) / 2)
        width: Math.max(root.width, root.roadWidth)
        height: root.height
        visible: root.roadEnabled
        colors: root.colors
        privateWindow: root.privateWindow
        running: root.visible && root.roadEnabled && root.windowActive
        driving: root.driving || (root.drove && !root.open)
    }

    // The Shortcut sheet is summoned, not shown, so the page names the key:
    // a keycap in the accent and the word beside it, on a plate of the road's
    // dark so it reads over the lit horizon as well as the ground.
    Rectangle {
        id: hint
        objectName: "startPageHint"

        readonly property color keyColour: root.roadEnabled ? road.glow : root.colors.accent
        readonly property color wordColour: root.roadEnabled ? road.light : root.colors.text

        anchors.horizontalCenter: parent.horizontalCenter
        y: root.horizonY + root.fieldBelowHorizon + Style.spacing.xl
        visible: !root.driving
        width: hintRow.implicitWidth + Style.spacing.lg * 2
        height: hintRow.implicitHeight + Style.spacing.sm * 2
        radius: 3
        color: Qt.rgba(0, 0, 0, root.roadEnabled ? 0.45 : 0)
        Accessible.role: Accessible.StaticText
        Accessible.name: "Question mark shows the keyboard shortcuts"

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
                border.color: hint.keyColour

                Text {
                    id: keyText
                    anchors.centerIn: parent
                    text: "?"
                    color: hint.keyColour
                    font.family: Style.font.family
                    font.pixelSize: Style.font.body
                    font.bold: true
                    Accessible.ignored: true
                }
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "shortcuts"
                color: hint.wordColour
                opacity: 0.85
                font.family: Style.font.family
                font.pixelSize: Style.font.body
                Accessible.ignored: true
            }
        }
    }
}
