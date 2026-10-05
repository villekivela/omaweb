import QtQuick
import qs.Commons
import qs.Ui as Omarchy

// The list Omaweb draws under a focused page field to offer what could go in
// it. The field keeps the keyboard, so nothing here takes focus: the page
// hands over the keys that walk the list, and the owner moves `highlighted`
// and decides what accepting a row does. Form history owns the first list;
// it is one surface so that what else is offered under a field is drawn the
// same way.
//
// It is drawn on the kit's popup surface, at least as wide as the field, under
// it, or above it where the page has no room below.
Omarchy.BorderSurface {
    id: root

    // Each row is `{value, typed}`: the value offered and the part of it the
    // reader has already typed, which is drawn regular with the rest bold, as
    // an Engine suggestion is in the Omnibar. A row may instead carry a
    // `detail`, drawn muted on a second line under a value drawn plain, or be
    // a `note`, drawn muted, which says something rather than offers it. Rows
    // come in `group`s, and a hairline divides one group from the next.
    property var rows: []
    property int highlighted: -1
    property bool open: false
    // The field and the page it is in, in the parent's coordinates. The list
    // stays within the page.
    property rect anchorRect: Qt.rect(0, 0, 0, 0)
    property rect boundsRect: Qt.rect(0, 0, 0, 0)

    readonly property int count: rows.length
    readonly property bool shown: open && count > 0
    readonly property real edgeMargin: 8
    readonly property real maxWidth: 480
    readonly property real rowHeight: Style.spacing.popupRowHeight
    readonly property var popupBorderSpec: Border.surfaceSpec("popups", "border",
                                                              Color.popups.border,
                                                              Style.normalBorderWidth)

    signal accepted(int index)

    function escaped(text) {
        return text.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
    }

    function markup(row) {
        if (row.typed === undefined)
            return escaped(row.value);
        const kept = row.value.toLowerCase().startsWith(row.typed.toLowerCase()) ? row.typed.length :
                                                                                   0;
        return escaped(row.value.substring(0, kept)) + "<b>" + escaped(row.value.substring(kept))
                + "</b>";
    }

    readonly property real roomBelow: boundsRect.y + boundsRect.height - anchorRect.y
                                      - anchorRect.height - edgeMargin
    readonly property bool above: implicitHeight > roomBelow && anchorRect.y - boundsRect.y
                                  - edgeMargin > roomBelow

    visible: shown
    readonly property real boundsRight: boundsRect.x + boundsRect.width
    width: Math.max(0, Math.min(Math.max(anchorRect.width, Math.min(rowsColumn.implicitWidth + contentLeftInset
                                                                    + contentRightInset, maxWidth)),
                                boundsRect.width - 2 * edgeMargin))
    x: Math.max(boundsRect.x + edgeMargin, Math.min(anchorRect.x, boundsRight - width - edgeMargin))
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
                // A pointer over a row shows it and no more: highlighting it
                // would hand Enter to the list while the reader is typing.
                readonly property bool hot: pointer.containsMouse && !current && !note
                readonly property bool note: modelData.note === true

                readonly property string detail: modelData.detail || ""
                readonly property bool divided: index > 0 && root.rows[index - 1].group
                                                !== modelData.group
                readonly property real dividerSpace: divided ? Style.spacing.sm * 2
                                                               + Style.spacing.hairline : 0

                objectName: "suggestionRow" + index
                width: rowsColumn.width
                implicitWidth: Math.max(label.implicitWidth, detailLabel.implicitWidth) + 2
                               * Style.spacing.controlPaddingX
                height: dividerSpace + (detail.length > 0 ? label.implicitHeight
                                                            + detailLabel.implicitHeight + 2
                                                            * Style.spacing.controlPaddingY :
                                                            root.rowHeight)
                color: "transparent"

                Accessible.role: Accessible.ListItem
                Accessible.name: detail.length > 0 ? value + ", " + detail : value
                Accessible.selected: current

                Rectangle {
                    objectName: "suggestionDivider"
                    visible: row.divided
                    x: Style.spacing.controlPaddingX
                    y: Style.spacing.sm
                    width: parent.width - 2 * Style.spacing.controlPaddingX
                    height: Style.spacing.hairline
                    color: Color.popups.border
                }

                Rectangle {
                    id: body
                    y: row.dividerSpace
                    width: parent.width
                    height: parent.height - row.dividerSpace
                    color: (row.current && !row.note) || row.hot ? Style.hoverFillFor(
                                                                       Color.popups.text,
                                                                       Color.accent) : "transparent"
                }

                Text {
                    id: label
                    objectName: "suggestionText"
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: Style.spacing.controlPaddingX
                    anchors.rightMargin: Style.spacing.controlPaddingX
                    y: row.detail.length > 0 ? body.y + Style.spacing.controlPaddingY : body.y + (
                                                   body.height - height) / 2
                    textFormat: Text.StyledText
                    text: root.markup(row.modelData)
                    color: row.note ? Color.muted : row.current ? Style.hoverStateColor(Color.popups.text,
                                                                                        Color.accent) :
                                                                  Color.popups.text
                    font.family: Style.font.family
                    font.pixelSize: Style.font.body
                    elide: Text.ElideRight
                }

                Text {
                    id: detailLabel
                    objectName: "suggestionDetail"
                    visible: row.detail.length > 0
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: Style.spacing.controlPaddingX
                    anchors.rightMargin: Style.spacing.controlPaddingX
                    anchors.top: label.bottom
                    textFormat: Text.PlainText
                    text: row.detail
                    color: row.current ? Style.hoverStateColor(Color.muted, Color.accent) :
                                         Color.muted
                    font.family: Style.font.family
                    font.pixelSize: Style.font.caption
                    elide: Text.ElideRight
                }

                // Accepted on the press: a release would come after the page
                // had heard of a press outside its field.
                MouseArea {
                    id: pointer
                    anchors.fill: body
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onPressed: root.accepted(row.index)
                }
            }
        }
    }
}
