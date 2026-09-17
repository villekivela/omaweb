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

    // A `ScrollView` insets its content and can be laid out right to left; a
    // webpage's view is a plain `Item` and carries none of that. Read through
    // rather than required, so one bar serves a scrolling region of the chrome
    // and a page the engine draws without the call sites dividing into two
    // kinds.
    readonly property real viewTopInset: root.view && root.view.topPadding !== undefined
                                         ? root.view.topPadding : 0
    readonly property real viewLeftInset: root.view && root.view.leftPadding !== undefined
                                          ? root.view.leftPadding : 0
    readonly property real viewLengthDown: {
        if (!root.view)
            return 0;
        return root.view.availableHeight !== undefined ? root.view.availableHeight :
                                                         root.view.height;
    }
    readonly property real viewLengthAcross: {
        if (!root.view)
            return 0;
        return root.view.availableWidth !== undefined ? root.view.availableWidth : root.view.width;
    }
    readonly property bool viewMirrored: root.view && root.view.mirrored !== undefined
                                         ? root.view.mirrored : false

    // The padding the view keeps along the edge the bar runs down. A bar the
    // width of the gutter laid inside `availableWidth` covers the last of every
    // row while the region's own padding stands empty beside it, so where the
    // view has padding at least as wide as the gutter the bar runs in the
    // padding and the content keeps its full width. A view without padding — a
    // webpage's — gets the gutter over its edge as before.
    readonly property real viewTrailingInset: {
        if (!root.view)
            return 0;
        if (root.orientation === Qt.Horizontal)
            return root.view.bottomPadding !== undefined ? root.view.bottomPadding : 0;
        const inset = root.viewMirrored ? root.view.leftPadding : root.view.rightPadding;
        return inset !== undefined ? inset : 0;
    }
    readonly property real lane: Math.max(root.viewTrailingInset, root.gutter)
    readonly property int thumbInset: Math.floor((root.lane - root.hoveredThumb) / 2)

    parent: root.view
    x: {
        if (!root.view)
            return 0;
        if (root.orientation === Qt.Horizontal)
            return root.viewLeftInset;
        return root.viewMirrored ? 0 : root.view.width - root.width;
    }
    y: {
        if (!root.view)
            return 0;
        return root.orientation === Qt.Horizontal ? root.view.height - root.height :
                                                    root.viewTopInset;
    }
    width: root.orientation === Qt.Horizontal && root.view ? root.viewLengthAcross : root.lane

    height: root.orientation === Qt.Horizontal || !root.view ? root.lane : root.viewLengthDown

    // Content that fits its view has nothing for a bar to say. Qt's own style
    // hides the bar for an `AsNeeded` policy inside the painting this file
    // replaces, so the rule is restated here: without it a pointer resting at
    // the edge of a short list draws a thumb the length of the whole track.
    visible: root.policy === ScrollBar.AlwaysOn || (root.policy !== ScrollBar.AlwaysOff
                                                    && root.size < 1.0)

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
            // Centred across the lane at its widened width, so a lane that is
            // the region's padding carries the thumb in the middle of the
            // padding rather than against the window's edge.
            anchors.rightMargin: root.orientation === Qt.Horizontal ? 0 : root.thumbInset
            anchors.bottomMargin: root.orientation === Qt.Horizontal ? root.thumbInset : 0
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
