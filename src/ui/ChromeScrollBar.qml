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

    // The scrolling region this bar belongs to, and the reason it is named
    // rather than inferred: Qt's `ScrollView` does not lay its scrollbars out.
    // The geometry lives inside the default `ScrollBar` its style file
    // declares, so replacing `ScrollBar.vertical` drops the parent, the
    // position and the length along with the painting, and an unlaid bar sits
    // at the view's top corner at its implicit size. Every call site hands the
    // view over; the bindings below are the ones the style file would have
    // carried.
    property Item view: null

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

    parent: root.view
    x: {
        if (!root.view)
            return 0;
        if (root.orientation === Qt.Horizontal)
            return root.view.leftPadding;
        return root.view.mirrored ? 0 : root.view.width - root.width;
    }
    y: {
        if (!root.view)
            return 0;
        return root.orientation === Qt.Horizontal ? root.view.height - root.height :
                                                    root.view.topPadding;
    }
    width: root.orientation === Qt.Horizontal && root.view ? root.view.availableWidth :
                                                             root.implicitWidth
    height: root.orientation === Qt.Horizontal || !root.view ? root.implicitHeight :
                                                               root.view.availableHeight

    // Proportional sizing alone leaves a long list with a few pixels of thumb,
    // which is visible but not catchable. Capped as well as floored: the length
    // this is measured against is zero before the view has laid the bar out,
    // and an uncapped ratio would ask for a thumb longer than its own track.
    minimumSize: {
        const along = root.orientation === Qt.Horizontal ? root.width : root.height;
        return Math.min(0.5, 32 / Math.max(along, 64));
    }

    // No track, in any state. The thumb alone says where the reader stands in
    // the list, and a lane drawn behind it is a second shape laid over the
    // content to say the same thing again. Named as null rather than left out:
    // the style this derives from paints a track of its own under a
    // high-contrast desktop, which would be the one place the lane came back.
    background: null

    contentItem: Item {
        implicitWidth: root.gutter
        implicitHeight: root.gutter

        Rectangle {
            id: thumb

            // The thumb hugs the window's edge and grows inward, so widening
            // does not shift the shape the reader is already aiming at.
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

    // Bound rather than `readonly`: a Behavior animates by writing the property
    // it is attached to, so a readonly one fails to load rather than simply
    // refusing to move.
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
