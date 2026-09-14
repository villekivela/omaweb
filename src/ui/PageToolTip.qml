import QtQuick
import qs.Commons
import qs.Ui as Omarchy

// A page's own `title` text, drawn where the chrome's tooltips are drawn. The
// kit's `PanelToolTip` is the tooltip this window already has a style for, so
// it is used rather than restyled; what this adds is the part the kit's
// contract does not cover. A kit tooltip is declared inside the item it
// describes and follows that item's hover state, and a page has no such item
// here: the engine reports a point in its own coordinates and says only that
// the page raised a tooltip or took it back.
//
// Timing belongs to the engine. Chromium has already waited out its own hover
// delay before it says Show, and says Hide when the pointer leaves, so a delay
// here would be the second one the reader waits through and a timeout here
// would take the text away while they are still under it.
Item {
    id: root

    // Where the page wants the tooltip, in this item's coordinates.
    property real anchorX: 0
    property real anchorY: 0
    property string text: ""
    property bool open: false

    // Clear of the pointer, which the engine's point is usually under.
    readonly property real pointerGap: 18
    readonly property real edgeMargin: 8
    // A measure to read at, rather than however wide the title happens to be.
    // A tooltip let out to the window's width is one long line the eye has to
    // track back across; this is around fifty characters at the kit's face.
    readonly property real maxWidth: 360

    Omarchy.PanelToolTip {
        id: tip
        parent: root
        // The engine does not wait: it reports the ask as soon as the hovered
        // node changes, which is why Qt's own delegate carried a delay of its
        // own. The wait is the kit's, so a page's tooltip arrives on the same
        // timing as the chrome's rather than on a second one invented here.
        //
        // Nothing expires it, unlike the delegate's 1500 ms. The reader is
        // reading, and the page says when the tooltip is over.
        timeout: -1
        visible: root.open && root.text.length > 0
        text: root.text
        // A short title keeps its own width; a long one is held to the measure
        // and wraps. The window is the last bound rather than the first, so a
        // narrow window still gets a tooltip that fits inside it.
        width: Math.min(implicitWidth, root.maxWidth, root.width - 2 * root.edgeMargin)

        // The kit's tooltip carries chrome text, which is Omaweb's own and
        // short, so it never had reason to wrap. A page's title is neither,
        // and against the bound above it would be cut off mid-word instead.
        // The kit's colour, face, and border insets are the ones it sets
        // itself: this diverges from PanelToolTip in wrapping and in nothing
        // else.
        contentItem: Text {
            text: tip.text
            textFormat: Text.PlainText
            color: tip.panelForeground
            font.family: tip.fontFamily
            font.pixelSize: tip.fontSize
            wrapMode: Text.WordWrap
            // A title long enough to need a seventh line is not being read off
            // a tooltip anyway.
            maximumLineCount: 6
            elide: Text.ElideRight
            leftPadding: Border.left(tip.panelBorderSpec) + Style.spacing.controlPaddingX
            rightPadding: Border.right(tip.panelBorderSpec) + Style.spacing.controlPaddingX
            topPadding: Border.top(tip.panelBorderSpec) + Style.spacing.controlPaddingY
            bottomPadding: Border.bottom(tip.panelBorderSpec) + Style.spacing.controlPaddingY
        }

        // Below the pointer where there is room for it, above where there is
        // not, so a tooltip raised at the foot of the window is not clipped
        // against it.
        readonly property real below: root.anchorY + root.pointerGap
        readonly property real above: Math.max(root.edgeMargin, root.anchorY - root.pointerGap
                                               - height)

        readonly property bool fitsBelow: below + height + root.edgeMargin < root.height

        x: Math.max(root.edgeMargin, Math.min(root.anchorX, root.width - width - root.edgeMargin))
        y: fitsBelow ? below : above
    }
}
