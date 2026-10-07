import QtQuick
import qs.Commons
import qs.Ui as Omarchy

// A value the reader slides within a range, in whole steps: the kit's
// `PanelSlider`, the value beside it, and a reset while the value is the
// reader's rather than the default's. Drawn by the kit (third_party/omarchy-shell),
// which is left as it is; this file is the adapter that adds what the kit's
// slider does not carry: the keyboard, a focus ring as the kit draws it on its
// buttons, and what a screen reader is told.
//
// The kit's slider is a pointer control, so the arrow keys and the accessible
// actions step the value here, by `step`, and stop at either end. The value
// and the default it stands over are spoken together, as `SettingStepper` does.
//
// Reset keeps its place in the row while it is hidden. A track that moved when
// reset appeared would move under the pointer on the first step of a drag.
Row {
    id: root

    property var colors
    // What is stored, in `unit`s, whole numbers, so that a step is never a
    // fraction of one. The call site binds it.
    property int current: 0
    property int minimum: 0
    property int maximum: 100
    property int step: 1
    property bool overridden: false
    property string unit: "%"
    property string accessibleName: ""
    // Where reset takes the value, named so that reset is a return and not a
    // jump: "the theme's".
    property string defaultName: qsTr("the default")

    // Qt hands an assistive tool a slider's value, range and step by these
    // names. A tool that sets `value` is a reader moving it, so the write goes
    // the way a drag's does and `value` is bound to `current` again.
    property int value: root.current
    readonly property int minimumValue: root.minimum
    readonly property int maximumValue: root.maximum
    readonly property int stepSize: root.step

    onValueChanged: {
        if (root.value === root.current)
            return;
        const next = root.snapped(root.value);
        root.value = Qt.binding(function () {
            return root.current;
        });
        if (next !== root.current)
            root.moved(next);
    }

    // The value the reader asked for, already on a step and inside the range.
    signal moved(int value)
    signal reset

    readonly property string spokenValue: root.overridden ? qsTr("%1 percent").arg(root.current) :
                                                            qsTr("%1 percent, %2").arg(
                                                                root.current).arg(root.defaultName)

    function snapped(candidate) {
        const steps = Math.round((candidate - root.minimum) / root.step);
        return Math.min(root.maximum, Math.max(root.minimum, root.minimum + steps * root.step));
    }

    function stepBy(direction) {
        const next = root.snapped(root.current + direction * root.step);
        if (next !== root.current)
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

    Item {
        id: control
        width: controlRow.width + Style.spacing.md * 2
        height: Math.max(slider.implicitHeight, valueLabel.implicitHeight) + Style.spacing.md

        Omarchy.BorderSurface {
            objectName: "focusRing"
            anchors.fill: parent
            visible: root.activeFocus
            radius: Style.cornerRadius
            color: Style.focusFillFor(root.colors.text, root.colors.text)
            borderSpec: Border.controlSpec("focus", root.colors.text, root.colors.text)
        }

        Row {
            id: controlRow
            anchors.centerIn: parent
            spacing: Style.spacing.controlGap

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
                value: root.current / 100
                onMoved: function (fraction) {
                    const next = root.snapped(Math.round(fraction * 100));
                    if (next !== root.current)
                        root.moved(next);
                }
            }

            Text {
                id: valueLabel
                objectName: "value"
                anchors.verticalCenter: parent.verticalCenter
                width: Math.ceil(valueMetrics.advanceWidth(root.maximum + root.unit))
                horizontalAlignment: Text.AlignHCenter
                text: root.current + root.unit
                color: root.colors.text
                font.family: Style.font.family
                font.pixelSize: Style.font.body
                Accessible.ignored: true
            }
        }
    }

    FontMetrics {
        id: valueMetrics
        font.family: valueLabel.font.family
        font.pixelSize: valueLabel.font.pixelSize
    }

    ActionButton {
        objectName: "reset"
        anchors.verticalCenter: parent.verticalCenter
        colors: root.colors
        label: qsTr("reset")
        opacity: root.overridden ? 1.0 : 0.0
        enabled: root.overridden
        focusable: root.overridden
        accessibleName: qsTr("Reset %1 to %2").arg(root.accessibleName).arg(root.defaultName)
        Accessible.ignored: !root.overridden
        onClicked: root.reset()
    }
}
