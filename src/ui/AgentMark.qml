import QtQuick
import Omaweb
import qs.Commons

// An Agent's mark, on the row of a tab an Agent is attached to and in place of
// the letter of a Space it works in. It pulses only while one of the Agent's
// commands is in flight and holds still while the connection is idle, so idle
// chrome draws no frames for it. A desktop that asks for reduced motion gets
// a mark held dim instead of a pulse, which still says a command is in flight.
Text {
    id: root

    property bool busy: false

    text: "smart_toy"
    font.pixelSize: Style.font.iconLarge
    readonly property real restingOpacity: root.busy && SystemMotion.reduced ? 0.6 : 1
    opacity: restingOpacity
    Accessible.ignored: true

    SequentialAnimation on opacity {
        running: root.visible && root.busy && !SystemMotion.reduced
        loops: Animation.Infinite
        onRunningChanged: if (!running)
                              root.opacity = Qt.binding(function () {
                                  return root.restingOpacity;
                              })
        NumberAnimation {
            to: 0.3
            duration: 450
            easing.type: Easing.InOutSine
        }
        NumberAnimation {
            to: 1
            duration: 450
            easing.type: Easing.InOutSine
        }
    }
}
