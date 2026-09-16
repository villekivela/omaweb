import QtQuick
import qs.Commons

// A size the reader steps rather than types: the value between a step down
// and a step up, and a reset beside them while the value is the reader's
// rather than the default's. A step past either end of the range is not a
// step, and the button for it says so by being disabled.
//
// The value and the default it stands over are what a screen reader is told,
// on the spin box the row amounts to and again on each button, so that
// "increase" is never read out without the size it increases.
Row {
    id: root

    property var colors
    property int value: 0
    property int minimum: 0
    property int maximum: 100
    property bool overridden: false
    property string unit: "px"
    // What the value reads as at zero, where zero means none rather than a
    // size, as a minimum font size does.
    property string zeroLabel: ""
    property string accessibleName: ""
    // Where reset takes the value, named so that reset is a return and not a
    // jump: "the theme's", "the engine's".
    property string defaultName: "the default"

    signal increased
    signal decreased
    signal reset

    readonly property bool atNone: root.value === 0 && root.zeroLabel.length > 0
    readonly property string valueText: root.atNone ? root.zeroLabel : root.value + root.unit
    readonly property string spokenValue: root.spokenSize() + root.spokenDefault()

    function spokenSize() {
        return root.atNone ? root.zeroLabel : root.value + " pixels";
    }

    function spokenDefault() {
        return root.overridden ? "" : ", " + root.defaultName;
    }

    // The widest value the range allows, so the buttons do not shuffle as
    // the number changes width.
    function widestValue() {
        const widestNumber = valueMetrics.advanceWidth(root.maximum + root.unit);
        const widestWord = valueMetrics.advanceWidth(root.zeroLabel);
        return Math.ceil(Math.max(widestNumber, widestWord));
    }

    spacing: Style.spacing.controlGap

    Accessible.role: Accessible.SpinBox
    Accessible.name: root.accessibleName
    Accessible.description: root.spokenValue
    Accessible.onIncreaseAction: if (root.value < root.maximum)
                                     root.increased()
    Accessible.onDecreaseAction: if (root.value > root.minimum)
                                     root.decreased()

    ActionButton {
        objectName: "decrease"
        colors: root.colors
        label: "−"
        enabled: root.value > root.minimum
        accessibleName: "Decrease " + root.accessibleName + ", now " + root.spokenValue
        onClicked: root.decreased()
    }

    Text {
        id: valueLabel
        objectName: "value"
        anchors.verticalCenter: parent.verticalCenter
        width: root.widestValue()
        horizontalAlignment: Text.AlignHCenter
        text: root.valueText
        color: root.colors.text
        font.family: Style.font.family
        font.pixelSize: Style.font.body
        Accessible.role: Accessible.StaticText
        Accessible.name: root.accessibleName + ": " + root.spokenValue
    }

    FontMetrics {
        id: valueMetrics
        font.family: valueLabel.font.family
        font.pixelSize: valueLabel.font.pixelSize
    }

    ActionButton {
        objectName: "increase"
        colors: root.colors
        label: "+"
        enabled: root.value < root.maximum
        accessibleName: "Increase " + root.accessibleName + ", now " + root.spokenValue
        onClicked: root.increased()
    }

    ActionButton {
        objectName: "reset"
        colors: root.colors
        label: "reset"
        visible: root.overridden
        accessibleName: "Reset " + root.accessibleName + " to " + root.defaultName
        onClicked: root.reset()
    }
}
