import QtQuick
import qs.Commons
import qs.Ui as Omarchy

// The list Omaweb draws under a focused page field to offer what could go in
// it. The field keeps the keyboard, so nothing here takes focus: the page
// hands over the keys that walk the list, and the owner moves `highlighted`
// and decides what accepting a row does. Form history is the first owner;
// addresses and payment cards offer theirs through the same surface.
//
// It is drawn on the kit's popup surface, at least as wide as the field, under
// it, or above it where the window has no room below.
Omarchy.BorderSurface {
    id: root

    // Each row is `{value, typed}`: the value offered and the part of it the
    // reader has already typed, which is drawn regular with the rest bold, as
    // an Engine suggestion is in the Omnibar.
    property var rows: []
    property int highlighted: -1
    property bool open: false
    // The field, in the parent's coordinates.
    property rect anchorRect: Qt.rect(0, 0, 0, 0)

    readonly property int count: rows.length
    readonly property bool shown: open && count > 0
    readonly property real edgeMargin: 8
    readonly property real maxWidth: 480
    readonly property real rowHeight: Style.spacing.popupRowHeight
    readonly property var popupBorderSpec: Border.surfaceSpec("popups", "border",
                                                              Color.popups.border,
                                                              Style.normalBorderWidth)

    signal accepted(int index)
    signal hovered(int index)

    function escaped(text) {
        return text.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
    }

    function markup(row) {
        const kept = row.value.toLowerCase().startsWith(row.typed.toLowerCase()) ? row.typed.length :
                                                                                   0;
        return escaped(row.value.substring(0, kept)) + "<b>" + escaped(row.value.substring(kept))
                + "</b>";
    }

    readonly property real roomBelow: parent ? parent.height - anchorRect.y - anchorRect.height
                                               - edgeMargin : 0
    readonly property bool above: implicitHeight > roomBelow && anchorRect.y - edgeMargin
                                  > roomBelow

    visible: shown
    width: parent ? Math.min(Math.max(anchorRect.width, Math.min(rowsColumn.implicitWidth
                                                                 + contentLeftInset
                                                                 + contentRightInset, maxWidth)),
                             parent.width - 2 * edgeMargin) : 0
    x: parent ? Math.max(edgeMargin, Math.min(anchorRect.x, parent.width - width - edgeMargin)) : 0
    y: above ? anchorRect.y - height : anchorRect.y + anchorRect.height
    implicitHeight: contentTopInset + rowsColumn.implicitHeight + contentBottomInset
    height: implicitHeight
    padding: Style.spacing.hairline
    radius: Style.cornerRadius
    color: Color.popups.background
    borderSpec: root.popupBorderSpec

    Accessible.role: Accessible.List

    Column {
        id: rowsColumn
        x: root.contentLeftInset
        y: root.contentTopInset
        width: root.width - root.contentLeftInset - root.contentRightInset

        Repeater {
            model: root.rows

            Rectangle {
                id: row
                required property var modelData
                required property int index
                readonly property string value: modelData.value
                readonly property bool current: index === root.highlighted

                objectName: "suggestionRow" + index
                width: rowsColumn.width
                implicitWidth: label.implicitWidth + 2 * Style.spacing.controlPaddingX
                height: root.rowHeight
                color: current ? Style.hoverFillFor(Color.popups.text, Color.accent) : "transparent"
                Accessible.role: Accessible.ListItem
                Accessible.name: value
                Accessible.selected: current

                Text {
                    id: label
                    objectName: "suggestionText"
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: Style.spacing.controlPaddingX
                    anchors.rightMargin: Style.spacing.controlPaddingX
                    textFormat: Text.StyledText
                    text: root.markup(row.modelData)
                    color: row.current ? Style.hoverStateColor(Color.popups.text, Color.accent) :
                                         Color.popups.text
                    font.family: Style.font.family
                    font.pixelSize: Style.font.body
                    elide: Text.ElideRight
                }

                // Accepted on the press: a release would come after the page
                // had heard of a press outside its field.
                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onPositionChanged: root.hovered(row.index)
                    onPressed: root.accepted(row.index)
                }
            }
        }
    }
}
