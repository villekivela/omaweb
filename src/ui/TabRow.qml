import QtQuick
import qs.Commons
import qs.Ui as Omarchy

Item {
    id: root
    objectName: (pinned ? "pinned-" : "tab-") + tabId

    required property string tabId
    required property string tabTitle
    required property url tabUrl
    required property url tabIconUrl
    required property bool pinned
    // Which place this row holds inside its own section. The list sets it, and
    // a drag is answered in the same counting.
    property int placeInSection: -1
    required property bool active
    required property bool loading
    required property bool tabAudible
    required property bool tabMuted
    // The tab's sound is being held back until the reader has dealt with its
    // origin. The row draws it the way it draws muting, because that is what it
    // is from where the reader sits — silence, with the sound one press away.
    required property bool tabSoundSuppressed
    property var colors
    property string iconFontFamily
    property bool useFavicons: true
    property bool tintFavicons: true

    // A pinned tab is a square with no title, so its colour is what tells the
    // sites apart, and the mark is drawn in it on every pin. The colour leaves
    // the mark only on the active pin — a wash behind the button and a border
    // to match. The rest keep the theme's own muted border, so one pin is the
    // coloured one and the row around it stays monochrome.
    //
    // Until the favicon says what colour the site is, the pin stays the
    // theme's: a hue hashed out of the host name tells chips apart, but it is
    // not the site's own colour and no button is painted in it.
    readonly property bool hasSiteColor: tile.hasSiteColor
    readonly property color siteColor: hasSiteColor ? tile.siteTint : colors.accent

    // A tab that is making sound says so, and a muted one keeps saying it:
    // the speaker is the only place the sound can be given back, so it stays
    // on the row for as long as the reader's decision does.
    readonly property bool showsAudio: tabAudible || tabMuted
    readonly property bool silenced: tabMuted || tabSoundSuppressed

    // The 18px slot an ordinary row gives its site chip, and where it starts.
    // The speaker takes the same box, so the two never sit in different
    // places and the title beside them never moves.
    readonly property int chipSize: 18
    readonly property int chipInset: 8

    signal activated(string tabId)
    signal closeRequested(string tabId)
    signal muteToggled(string tabId)
    // A drag of this row, reported in scene coordinates. Where the pointer is
    // and where that lands belong to the list: a row knows how tall it is and
    // nothing about the rows around it. The list answers by placing this row —
    // `lifted` while it is held, `carry` for how far it has been carried from
    // where the list put it — so the row stays under the hand that took it
    // while the rows it passes open the place it will land in.
    signal dragStarted(string tabId)
    signal dragMoved(string tabId, real sceneX, real sceneY)
    signal dragEnded(string tabId)
    // The row's own menu, opened by pointer or by keyboard. Scene coordinates,
    // because the menu hangs in the window rather than inside the row.
    signal menuRequested(string tabId, real anchorX, real anchorY)

    // A held row is drawn where the hand has carried it rather than where the
    // list put it, and over the rows it is passing. The rows it passes are
    // carried too, by the list, into the places the arrangement would give
    // them — so the gap the row would drop into is open before it is dropped.
    //
    // Carried by a transform rather than by `x` and `y`: the row's place is
    // the positioner's to set, and a row that fought it for its own coordinates
    // would be put back the moment anything else in the list changed.
    property bool lifted: false
    property point carry: Qt.point(0, 0)
    readonly property point grabbedAt: hoverArea.grabbedAt
    z: lifted ? 20 : 0
    opacity: lifted ? 0.92 : 1.0
    transform: Translate { x: root.carry.x; y: root.carry.y }

    // A row settles into an opened place rather than jumping into it. The one
    // in the hand is not eased: it is already following the pointer.
    Behavior on carry {
        enabled: !root.lifted
        PropertyAnimation { duration: 110; easing.type: Easing.OutCubic }
    }

    height: pinned ? 44 : 36
    activeFocusOnTab: true
    Accessible.role: Accessible.PageTab
    Accessible.name: (pinned ? "Pinned: " + tabTitle : tabTitle)
        + (tabMuted ? " (muted)"
            : (tabSoundSuppressed && tabAudible ? " (playing silently)"
                : (tabAudible ? " (playing audio)" : "")))
    Accessible.onPressAction: root.activated(root.tabId)

    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
            root.activated(root.tabId)
            event.accepted = true
        }
    }

    // The keyboard reaches this menu through the `tab-menu` command rather than
    // through a key handled here: `Shift+F10` already belongs to the page's own
    // menu, and a window shortcut takes a key before a focused row sees it.

    function openMenu(x, y) {
        const point = root.mapToItem(null, x, y)
        root.menuRequested(root.tabId, point.x, point.y)
    }

    // The kit paints hover and focus as veils over the fill, so the wash sits
    // on a plate beneath the button rather than in its background: hovering
    // the active pin then deepens the wash instead of replacing it.
    Rectangle {
        anchors.fill: parent
        visible: root.pinned && root.active
        color: Qt.rgba(root.siteColor.r, root.siteColor.g, root.siteColor.b, 0.18)
        radius: Style.cornerRadius
    }

    Omarchy.Button {
        id: tabButton
        anchors.fill: parent
        // The active pin has a wash and a border of its own, so it never takes
        // the kit's selected fill; an unpinned row has only that fill to say it.
        active: root.active && !root.pinned
        hasCursor: root.activeFocus || hoverArea.containsMouse
        bordered: root.pinned
        foreground: root.pinned && root.active ? root.siteColor
            : (root.pinned ? root.colors.mutedText : root.colors.text)
        background: "transparent"
        accent: root.pinned && root.active ? root.siteColor : root.colors.accent
        horizontalPadding: 0
        verticalPadding: 0
    }

    SiteTile {
        id: tile
        objectName: "siteTile-" + root.tabId
        implicitWidth: root.pinned ? 22 : root.chipSize
        implicitHeight: root.pinned ? 22 : root.chipSize
        // The speaker stands in the chip's place rather than beside it: a row
        // that widened for it would shove its own title sideways every time a
        // page started and stopped playing. The chip is what the row can spare
        // — the title beside it already names the site.
        visible: !root.showsAudio || root.pinned
        anchors.left: parent.left
        anchors.leftMargin: root.pinned ? (parent.width - width) / 2 : root.chipInset
        anchors.verticalCenter: parent.verticalCenter
        colors: root.colors
        siteUrl: root.tabUrl
        iconUrl: root.tabIconUrl
        highlighted: root.active
        useArtwork: root.useFavicons
        tintArtwork: root.tintFavicons
        siteColoredMark: root.pinned
    }

    // The title names the page; its address is already in the address button
    // whenever the tab is the active one, and reading it twice crowds the row.
    Text {
        visible: !root.pinned
        anchors.left: tile.right
        anchors.leftMargin: 9
        anchors.right: parent.right
        anchors.rightMargin: closeButton.width + 10
        anchors.verticalCenter: parent.verticalCenter
        text: root.tabTitle.length > 0 ? root.tabTitle : tile.host
        color: root.active ? root.colors.text : root.colors.mutedText
        elide: Text.ElideRight
        font.family: Style.font.family
        font.pixelSize: Style.font.body
    }

    MouseArea {
        id: hoverArea
        objectName: "tabPointer-" + root.tabId
        anchors.fill: parent
        z: 10
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        // A press on a row and a move is a reorder, so the row keeps the
        // gesture until the button comes up rather than letting the list it
        // scrolls in take it half way through. The list still scrolls by wheel
        // and by its own bar; what it no longer does is scroll by dragging the
        // rows the reader is trying to rearrange.
        preventStealing: true
        // Where in the row the hand took hold, and where it was when it did.
        // A press is a click until it has travelled far enough to be a drag,
        // so that a row is not lifted by the tremor in a click.
        property point grabbedAt: Qt.point(0, 0)
        property point pressedAt: Qt.point(0, 0)
        readonly property real liftThreshold: 4

        function report(mouse) {
            const scene = root.mapToItem(null, mouse.x, mouse.y)
            root.dragMoved(root.tabId, scene.x, scene.y)
        }

        onPressed: function(mouse) {
            hoverArea.grabbedAt = Qt.point(mouse.x, mouse.y)
            hoverArea.pressedAt = root.mapToItem(null, mouse.x, mouse.y)
        }

        onPositionChanged: function(mouse) {
            // The area's own record of what is held, not the event's: a
            // synthesized move carries no buttons, and a right-press opening
            // the menu must not drag the row on the way.
            if (!(hoverArea.pressedButtons & Qt.LeftButton)) return
            const scene = root.mapToItem(null, mouse.x, mouse.y)
            if (!root.lifted) {
                const travelled = Math.max(Math.abs(scene.x - hoverArea.pressedAt.x),
                    Math.abs(scene.y - hoverArea.pressedAt.y))
                if (travelled < hoverArea.liftThreshold) return
                root.dragStarted(root.tabId)
            }
            hoverArea.report(mouse)
        }

        onReleased: function(mouse) {
            if (!root.lifted) return
            root.dragEnded(root.tabId)
        }

        // A gesture the window took away — the pointer leaving the window, or
        // something above claiming it — leaves the row where the list has
        // already put it rather than holding it in the air.
        onCanceled: if (root.lifted) root.dragEnded(root.tabId)

        onClicked: function(mouse) {
            if (mouse.button === Qt.RightButton) {
                root.forceActiveFocus()
                root.openMenu(mouse.x, mouse.y)
                return
            }
            // A row that has just been carried into place was not clicked.
            if (root.lifted) return
            root.forceActiveFocus()
            const overClose = !root.pinned
                && mouse.x >= root.width - closeButton.width - closeButton.anchors.rightMargin
            if (audioButton.covers(mouse.x, mouse.y)) {
                root.muteToggled(root.tabId)
            } else if (overClose) {
                root.closeRequested(root.tabId)
            } else {
                root.activated(root.tabId)
            }
        }

        // A pin has only its chip to say which site it is, so its speaker goes
        // in the corner over it rather than in its place.
        Omarchy.BorderSurface {
            id: audioButton
            objectName: "audio-" + root.tabId
            function covers(x, y) {
                return audioButton.visible
                    && x >= audioButton.x && x < audioButton.x + audioButton.width
                    && y >= audioButton.y && y < audioButton.y + audioButton.height
            }
            readonly property bool hot: hoverArea.containsMouse
                && covers(hoverArea.mouseX, hoverArea.mouseY)
            readonly property color foreground: root.pinned && root.active
                ? root.siteColor
                : (root.silenced || !root.active ? root.colors.mutedText : root.colors.text)
            anchors.left: root.pinned ? undefined : parent.left
            anchors.leftMargin: root.chipInset
            anchors.verticalCenter: root.pinned ? undefined : parent.verticalCenter
            anchors.right: root.pinned ? parent.right : undefined
            anchors.rightMargin: 2
            anchors.top: root.pinned ? parent.top : undefined
            anchors.topMargin: 2
            width: root.chipSize
            height: root.chipSize
            visible: root.showsAudio
            radius: Style.cornerRadius
            color: hot ? Style.hoverFillFor(audioButton.foreground, root.colors.accent)
                : "transparent"
            borderSpec: hot
                ? Border.controlSpec("hover-cursor", audioButton.foreground, root.colors.accent)
                : Border.none()
            Accessible.role: Accessible.Button
            Accessible.name: (root.tabMuted ? "Unmute "
                : (root.tabSoundSuppressed ? "Allow sound from " : "Mute ")) + root.tabTitle
            Accessible.onPressAction: root.muteToggled(root.tabId)

            Text {
                anchors.centerIn: parent
                text: root.silenced ? "volume_off" : "volume_up"
                color: audioButton.foreground
                font.family: root.iconFontFamily
                font.pixelSize: Style.font.icon
            }
        }

        Omarchy.BorderSurface {
            id: closeButton
            objectName: "close-" + root.tabId
            property string accessibleName: "Close " + root.tabTitle
            readonly property bool hot: hoverArea.containsMouse
                && hoverArea.mouseX >= root.width - width - anchors.rightMargin
            property color foreground: hoverArea.containsMouse
                ? root.colors.mutedText : "transparent"
            anchors.right: parent.right
            anchors.rightMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            width: 28
            height: 28
            visible: !root.pinned
            radius: Style.cornerRadius
            color: hot ? Style.hoverFillFor(root.colors.mutedText, root.colors.accent)
                : "transparent"
            borderSpec: hot
                ? Border.controlSpec("hover-cursor", root.colors.mutedText, root.colors.accent)
                : Border.none()
            Accessible.role: Accessible.Button
            Accessible.name: accessibleName
            Accessible.onPressAction: root.closeRequested(root.tabId)

            Text {
                anchors.centerIn: parent
                text: "close"
                color: closeButton.foreground
                font.family: root.iconFontFamily
                font.pixelSize: Style.font.icon
            }
        }
    }

}
