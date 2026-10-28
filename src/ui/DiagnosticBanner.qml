import QtQuick
import qs.Commons

// The strip across the top of every window of a run started with
// `omaweb --diagnose` (#753), naming its Diagnostic overrides. It has no close:
// the overrides last the run, and so does the reminder that sites are being
// shown something other than the engine's own. A run without overrides draws
// none and takes no height.
Rectangle {
    id: root
    objectName: "diagnosticBanner"

    property var colors
    property string iconFontFamily
    // `<name>=<value>` each, as the run was started with.
    property var overrides: []

    readonly property bool shown: !!root.overrides && root.overrides.length > 0 && !!root.colors
    readonly property string text: root.shown ? qsTr("Diagnostic overrides for this run: %1").arg(
                                                    root.overrides.join(", ")) : ""

    height: root.shown ? 24 : 0
    visible: root.shown
    color: root.shown ? Qt.tint(root.colors.windowOpaque, Qt.rgba(root.colors.urgent.r,
                                                                  root.colors.urgent.g,
                                                                  root.colors.urgent.b, 0.14)) :
                        "transparent"
    Accessible.role: Accessible.StaticText
    Accessible.name: root.text

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: root.shown ? root.colors.urgent : "transparent"
    }

    Text {
        id: mark
        anchors.left: parent.left
        anchors.leftMargin: Style.space(8)
        anchors.verticalCenter: parent.verticalCenter
        text: "experiment"
        color: root.shown ? root.colors.urgent : "transparent"
        font.family: root.iconFontFamily
        font.pixelSize: Style.font.iconSmall
        Accessible.ignored: true
    }

    Text {
        anchors.left: mark.right
        anchors.leftMargin: Style.space(8)
        anchors.right: parent.right
        anchors.rightMargin: Style.space(8)
        anchors.verticalCenter: parent.verticalCenter
        text: root.text
        color: root.shown ? root.colors.text : "transparent"
        font.family: Style.font.family
        font.pixelSize: Style.font.caption
        elide: Text.ElideRight
        Accessible.ignored: true
    }
}
