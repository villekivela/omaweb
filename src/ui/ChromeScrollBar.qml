import QtQuick
import QtQuick.Controls.Basic

// The scrollbar for the chrome's own scrolling regions — the sidebar's tab
// list, the Settings pane. An overlay in the macOS manner: it holds a gutter at
// the edge of the region, paints nothing there while the reader is elsewhere,
// and shows a thumb when the list moves or the pointer arrives.
//
// Qt's Basic scrollbar is replaced rather than tinted because its colours come
// from `palette.mid` and `palette.dark` — Qt's own `QPalette`, which Omaweb
// never sets. The theme palette travels through QML as `colors` and never
// reaches it, so a Basic bar is a fixed neutral grey under every theme. Reading
// `colors` here is also what tints the bar inside a Private window: that map
// already carries the private grounds, so nothing about privacy is said twice.
ScrollBar {
    id: root

    property var colors

    // The gutter is wider than the thumb it rests: the reader aims at the edge
    // of the list, not at a six-pixel line, and the widened form has to have
    // somewhere to widen into. It is never painted, so the extra width costs
    // the page nothing.
    readonly property int gutter: 14
    readonly property int restingThumb: 6
    readonly property int hoveredThumb: 9

    // The pointer holds the widened form, and so does a drag that has left the
    // gutter behind — a thumb that narrowed mid-drag would slide out from under
    // the pointer that is carrying it.
    readonly property bool expanded: root.hovered || root.pressed

    implicitWidth: root.gutter
    implicitHeight: root.gutter
    padding: 0
    // Proportional sizing alone leaves a long list with a few pixels of thumb,
    // which is visible but not catchable.
    minimumSize: root.orientation === Qt.Horizontal ? 32 / Math.max(root.width, 1) : 32 / Math.max(
                                                          root.height, 1)

    // The track stands only under the pointer. While the reader is merely
    // scrolling, a bare thumb says the same thing without drawing a second
    // shape over the list.
    background: Rectangle {
        visible: root.trackOpacity > 0
        opacity: root.trackOpacity
        color: root.colors ? root.colors.surface : "transparent"

        // The hairline divides the track from the list it overlays, so it goes
        // on the inner edge — the left of a vertical bar, the top of a
        // horizontal one — and never along the window's own edge.
        Rectangle {
            width: root.orientation === Qt.Horizontal ? parent.width : 1
            height: root.orientation === Qt.Horizontal ? 1 : parent.height
            color: root.colors ? root.colors.separator : "transparent"
        }
    }

    // Bound rather than `readonly`, here and for the thumb's width: a Behavior
    // animates by writing the property it is attached to, so a readonly one
    // fails to load rather than simply refusing to move.
    property real trackOpacity: root.expanded ? 1.0 : 0.0

    Behavior on trackOpacity {
        NumberAnimation {
            duration: 120
            easing.type: Easing.OutCubic
        }
    }

    contentItem: Item {
        implicitWidth: root.gutter
        implicitHeight: root.gutter

        Rectangle {
            id: thumb

            // The thumb hugs the window's edge and grows inward, so the shape
            // the reader is already tracking does not jump sideways when the
            // track arrives under it.
            anchors.right: root.orientation === Qt.Horizontal ? undefined : parent.right
            anchors.bottom: root.orientation === Qt.Horizontal ? parent.bottom : undefined
            anchors.rightMargin: root.orientation === Qt.Horizontal ? 0 : 2
            anchors.bottomMargin: root.orientation === Qt.Horizontal ? 2 : 0
            anchors.verticalCenter: root.orientation === Qt.Horizontal ? undefined :
                                                                         parent.verticalCenter
            anchors.horizontalCenter: root.orientation === Qt.Horizontal ? parent.horizontalCenter :
                                                                           undefined

            width: root.orientation === Qt.Horizontal ? parent.width : root.thumbThickness
            height: root.orientation === Qt.Horizontal ? root.thumbThickness : parent.height
            radius: root.thumbThickness / 2
            color: {
                if (!root.colors)
                    return "transparent";
                if (root.pressed)
                    return root.colors.accent;
                if (root.hovered)
                    return root.colors.mutedText;
                return root.colors.border;
            }

            Behavior on color {
                ColorAnimation {
                    duration: 120
                }
            }
        }
    }

    property real thumbThickness: root.expanded ? root.hoveredThumb : root.restingThumb

    Behavior on thumbThickness {
        NumberAnimation {
            duration: 120
            easing.type: Easing.OutCubic
        }
    }

    // Nothing is drawn until the list moves or the pointer arrives, and the
    // pointer holds it: only an untouched bar is allowed to fade.
    opacity: 0.0

    states: State {
        name: "shown"
        when: root.policy === ScrollBar.AlwaysOn || root.expanded || (root.active && root.size
                                                                      < 1.0)

        PropertyChanges {
            root.opacity: 1.0
        }
    }

    transitions: Transition {
        from: "shown"

        SequentialAnimation {
            PauseAnimation {
                duration: 900
            }

            NumberAnimation {
                property: "opacity"
                to: 0.0
                duration: 260
                easing.type: Easing.OutCubic
            }
        }
    }
}
