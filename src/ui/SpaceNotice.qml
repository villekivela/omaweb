import QtQuick
import qs.Commons

// Names the Space a switch arrived in, at the top of the page, where the
// reader is looking: the footer names a Space only by its letter. It comes
// down from the top edge as a page notice does, stays a moment and goes back
// up. The window places it over the middle of where the page settles and
// holds it there, so the page's own arrival does not carry it.
Rectangle {
    id: root

    property var colors
    property string spaceName: ""
    // The reader's chrome ease. Refused, the notice appears and goes where it
    // stands.
    property bool ease: true
    readonly property alias text: name.text
    // How far above its place the notice is, while it comes and goes.
    property real drop: 0

    function show() {
        root.drop = root.ease ? -8 : 0;
        showing.restart();
    }

    visible: opacity > 0
    opacity: 0
    y: 12 + drop
    width: name.implicitWidth + 32
    height: name.implicitHeight + 14
    radius: 2
    color: colors.overlay
    border.width: 1
    border.color: colors.border
    Accessible.role: Accessible.StaticText
    Accessible.name: name.text

    Text {
        id: name
        anchors.centerIn: parent
        text: root.spaceName
        color: root.colors.text
        font.family: Style.font.family
        font.pixelSize: Style.font.body
    }

    SequentialAnimation {
        id: showing
        ParallelAnimation {
            NumberAnimation {
                target: root
                property: "opacity"
                to: 1
                duration: 160
            }
            NumberAnimation {
                target: root
                property: "drop"
                to: 0
                duration: 160
                easing.type: Easing.OutCubic
            }
        }
        PauseAnimation {
            duration: 900
        }
        ParallelAnimation {
            NumberAnimation {
                target: root
                property: "opacity"
                to: 0
                duration: 120
            }
            NumberAnimation {
                target: root
                property: "drop"
                to: root.ease ? -8 : 0
                duration: 120
                easing.type: Easing.InCubic
            }
        }
    }
}
