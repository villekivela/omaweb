import QtQuick
import qs.Commons
import qs.Ui as Omarchy

// A value the reader slides within a range, in whole steps: the kit's
// `PanelSlider`, the value beside it, and a reset while the value is the
// reader's rather than the default's. Drawn by the kit (third_party/omarchy-shell),
// which is left as it is; this file is the adapter that adds what the kit's
// slider does not carry, which is the keyboard and what a screen reader is told.
//
// The kit's slider is a pointer control, so the arrow keys and the accessible
// actions step the value here, by `step`, and stop at either end. The value
// and the default it stands over are spoken together, as `SettingStepper` does.
Row {
    id: root

    property var colors
    // In `unit`s, whole numbers, so that a step is never a fraction of one.
    property int value: 0
    property int minimum: 0
    property int maximum: 100
    property int step: 1
    property bool overridden: false
    property string unit: "%"
    property string accessibleName: ""
    // Where reset takes the value, named so that reset is a return and not a
    // jump: "the theme's".
    property string defaultName: qsTr("the default")

    // The value the reader asked for, already on a step and inside the range.
    signal moved(int value)
    signal reset

    readonly property string spokenValue: root.overridden ? qsTr("%1 percent").arg(root.value) :
                                                            qsTr("%1 percent, %2").arg(
                                                                root.value).arg(root.defaultName)

    function snapped(candidate) {
        const steps = Math.round((candidate - root.minimum) / root.step);
        return Math.min(root.maximum, Math.max(root.minimum, root.minimum + steps * root.step));
    }

    function stepBy(direction) {
        const next = root.snapped(root.value + direction * root.step);
        if (next !== root.value)
            root.moved(next);
    }

    spacing: Style.spacing.controlGap
    activeFocusOnTab: true

    Keys.onLeftPressed: root.stepBy(-1)
    Keys.onDownPressed: root.stepBy(-1)
    Keys.onRightPressed: root.stepBy(1)
    Keys.onUpPressed: root.stepBy(1)

    Accessible.role: Accessible.Slider
    Accessible.name: root.accessibleName
    Accessible.description: root.spokenValue
    Accessible.focusable: true
    Accessible.onIncreaseAction: root.stepBy(1)
    Accessible.onDecreaseAction: root.stepBy(-1)

    Omarchy.PanelSlider {
        id: slider
        objectName: "slider"
        anchors.verticalCenter: parent.verticalCenter
        width: Style.space(160)
        bar: QtObject {
            readonly property color foreground: root.colors.text
            readonly property color background: root.colors.sidebarOpaque
        }
        minimum: root.minimum / 100
        maximum: root.maximum / 100
        step: root.step / 100
        value: root.value / 100
        onMoved: function (fraction) {
            const next = root.snapped(Math.round(fraction * 100));
            if (next !== root.value)
                root.moved(next);
        }
    }

    Text {
        id: valueLabel
        objectName: "value"
        anchors.verticalCenter: parent.verticalCenter
        width: Math.ceil(valueMetrics.advanceWidth(root.maximum + root.unit))
        horizontalAlignment: Text.AlignHCenter
        text: root.value + root.unit
        color: root.colors.text
        font.family: Style.font.family
        font.pixelSize: Style.font.body
        Accessible.ignored: true
    }

    FontMetrics {
        id: valueMetrics
        font.family: valueLabel.font.family
        font.pixelSize: valueLabel.font.pixelSize
    }

    ActionButton {
        objectName: "reset"
        colors: root.colors
        label: qsTr("reset")
        visible: root.overridden
        accessibleName: qsTr("Reset %1 to %2").arg(root.accessibleName).arg(root.defaultName)
        onClicked: root.reset()
    }
}
