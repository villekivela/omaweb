import QtQuick
import Omaweb
import qs.Commons
import qs.Ui as Omarchy
import "DevicePixels.mjs" as DevicePixels

Item {
    id: root
    objectName: (pinned ? "pinned-" : "tab-") + tabId
    // The display's pixels per logical pixel, for resting on whole ones.
    readonly property real pixelRatio: Window.window ? Window.window.devicePixelRatio : 1

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
    // Whether the reader marked this Pinned tab Keep active. A pin has no
    // title to carry the state and the tab menu is the only other place the
    // setting appears, so the row draws it: a standing decision the reader
    // cannot see is one they cannot tell they have made, or undo.
    required property bool tabKeepActive
    // The split this tab is in, as the other tab of it, and whether this tab
    // is the one on show beside the active tab. A split's two rows are one
    // row of two: each is half the width, the active half is marked as the
    // active tab is, and the tab beside is marked as on show at half the
    // strength.
    required property string splitPartnerId
    required property bool tabBeside
    readonly property bool inSplit: splitPartnerId.length > 0
    // Whether this row is a split's right half. The list sets it, since it
    // knows where it laid the row out.
    property bool trailingHalf: false
    // How far an ordinary row's marks reach past it on each side, which the
    // list sets: out to the sidebar's edges, as the Omnibar's rows reach its
    // panel's, and for a split's half, half way across the gap to the other
    // half. The row's content stays where it is, so the site chip stays lined
    // up with the address field's icon. A pin is a tile with no title for a
    // bar to lead, and its marks stay inside it.
    property real reachLeft: 0
    property real reachRight: 0
    property var colors
    property string iconFontFamily
    property bool useFavicons: true
    property bool tintFavicons: false
    // The Agent attached to this tab, as the core reports it: the
    // connection's `name` and `busy` while one of its commands is in flight.
    // Null for a tab no Agent drives. A Pinned tab is never an Agent's.
    property var agent: null
    readonly property bool showsAgent: agent !== null && !pinned
    readonly property string agentNote: qsTr(
                                            "%1 is driving this tab. It stays rendered while attached.").arg(
                                            agent && String(agent.name || "").length > 0 ? String(
                                                                                               agent.name) :
                                                                                           qsTr("An Agent"))
    // The key that selects this tab, as the keymap displays it, and whether
    // Primary is being held for the labels.
    property string keyLabel: ""
    property bool keyLabelShown: false
    // Whether the row was last reached by a press rather than by the keyboard.
    // A hand on the mouse is not steering the Sidebar cursor.
    property bool reachedByPointer: false
    onActiveFocusChanged: if (!activeFocus)
                              reachedByPointer = false
    readonly property bool cursorShown: activeFocus && !reachedByPointer

    // The Omnibar's bar, on the sidebar's left edge of the row on show. A
    // split is one row on show, so it has one bar, on its left half whichever
    // half is active: a bar on each half would draw a second line down the
    // middle of the sidebar.
    readonly property bool showsBar: !pinned && !trailingHalf && (active || tabBeside)

    // Moves the keyboard to this row as the Sidebar cursor.
    function steer() {
        root.reachedByPointer = false;
        root.forceActiveFocus();
    }

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

    // Whether the site's own colour may reach this row at all. A pin is a
    // favicon with no title, so its colour is the only thing telling the sites
    // apart — but taking a colour off the site's artwork is what "Tint
    // favicons" answers, and a reader who has said no to that has said no
    // here too. Without it the pin is chrome: the theme's muted border, and
    // the accent for the active one, as an ordinary row's.
    readonly property bool siteColored: pinned && tintFavicons

    // A tab that is making sound says so, and a muted one keeps saying it:
    // the speaker is the only place the sound can be given back, so it stays
    // on the row for as long as the reader's decision does.
    readonly property bool showsAudio: tabAudible || tabMuted
    readonly property bool silenced: tabMuted || tabSoundSuppressed

    // Keep active is a pin's setting alone, and the core gives it up when a tab
    // is unpinned, so an ordinary row has nothing to say here.
    readonly property bool showsKeepActive: pinned && tabKeepActive

    // The slot an ordinary row gives its site chip, and where it starts. The
    // stand-in the chip draws is two characters of the theme's smallest type,
    // so the box is derived from that size rather than fixed: a theme with a
    // larger font would otherwise clip its own letters. The speaker takes the
    // same box, so the two never sit in different places and the title beside
    // them never moves.
    readonly property int chipSize: Math.round(Style.font.caption * 1.6)
    readonly property int chipInset: Style.space(8)
    // Where the close button and the Agent mark end: 12 px inside the edge the
    // row's wash reaches, which for an ordinary row is the sidebar's. Out in
    // the margin, so the glyph does not stand far in from a hover that runs to
    // the sidebar's edge, but not so far that it crowds it.
    readonly property int endGap: Style.space(12)

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
    transform: Translate {
        x: root.carry.x
        y: root.carry.y
    }

    // A row settles into an opened place rather than jumping into it. The one
    // in the hand is not eased: it is already following the pointer. Under
    // reduced motion every row steps into its place.
    Behavior on carry {
        enabled: !root.lifted && !SystemMotion.reduced
        PropertyAnimation {
            duration: 110
            easing.type: Easing.OutCubic
        }
    }

    height: Style.space(pinned ? 44 : 36)
    activeFocusOnTab: true
    Accessible.role: Accessible.PageTab
    Accessible.name: (pinned ? qsTr("Pinned: %1").arg(tabTitle) : tabTitle) + (tabBeside ? " "
                                                                                           + qsTr("(beside)") :
                                                                                           "") + (tabMuted
                                                                                                  ? " " + qsTr(
                                                                                                        "(muted)") :
                                                                                                    (tabSoundSuppressed
                                                                                                     && tabAudible
                                                                                                     ? " " + qsTr(
                                                                                                           "(playing silently)") :
                                                                                                       (tabAudible
                                                                                                        ? " " + qsTr(
                                                                                                              "(playing audio)") :
                                                                                                          ""))) + (showsKeepActive
                                                                                                                   ? " " + qsTr(
                                                                                                                         "(kept active)") :
                                                                                                                     "") + (showsAgent
                                                                                                                            ? " " + qsTr(
                                                                                                                                  "(Agent tab)") :
                                                                                                                              "")
    Accessible.description: showsAgent ? agentNote : ""
    Accessible.onPressAction: root.activated(root.tabId)

    // Return climbs to the outline, which opens the row as l does and hands
    // the keyboard to the page. Space opens it where it stands.
    Keys.onPressed: function (event) {
        if (event.key === Qt.Key_Space) {
            root.activated(root.tabId);
            event.accepted = true;
        }
    }

    // The keyboard reaches this menu through the `tab-menu` command rather than
    // through a key handled here: `Shift+F10` already belongs to the page's own
    // menu, and a window shortcut takes a key before a focused row sees it.

    function openMenu(x, y) {
        const point = root.mapToItem(null, x, y);
        root.menuRequested(root.tabId, point.x, point.y);
    }

    // The tab on show is marked as the Omnibar marks its selected row: the
    // accent at 14%, edge to edge with square corners. The tab beside a split's
    // active half is on show too, at half the strength. A pin keeps the mark
    // inside its tile, in the site's colour where it has one.
    //
    // The kit paints hover as a veil over the fill, so the wash sits on a plate
    // beneath the row rather than in a background: hovering the active row
    // then deepens the wash instead of replacing it.
    Rectangle {
        objectName: "tabWash-" + root.tabId
        anchors.fill: parent
        anchors.leftMargin: -root.reachLeft
        anchors.rightMargin: -root.reachRight
        visible: root.active || (root.tabBeside && !root.pinned)
        radius: root.pinned ? Style.cornerRadius : 0
        color: root.siteColored ? Qt.alpha(root.siteColor, 0.18) : Qt.alpha(root.colors.accent,
                                                                            root.active ? 0.14 :
                                                                                          0.07)
    }

    // The Omnibar's hover on an ordinary row, edge to edge. The Sidebar cursor
    // brings its own, so a row under both is not washed twice.
    Rectangle {
        objectName: "tabHover-" + root.tabId
        anchors.fill: parent
        anchors.leftMargin: -root.reachLeft
        anchors.rightMargin: -root.reachRight
        visible: !root.pinned && hoverArea.containsMouse && !root.cursorShown
        color: Qt.alpha(root.colors.accent, 0.07)
    }

    // A pin is a bordered tile at rest, as the reader's own address field is,
    // and carries the kit's control fill and hover. The active pin's wash is on
    // the plate beneath, so the tile takes its border in the same colour rather
    // than the kit's selected fill. An ordinary row is a line of text, and its
    // marks are the plates around it.
    Omarchy.Button {
        id: tabButton
        objectName: "tabButton-" + root.tabId
        anchors.fill: parent
        visible: root.pinned
        hasCursor: root.activeFocus || hoverArea.containsMouse
        bordered: true
        foreground: !root.active ? root.colors.mutedText : (root.siteColored ? root.siteColor :
                                                                               root.colors.accent)
        background: Style.normalFillFor(root.colors.text, root.colors.accent)
        accent: root.siteColored && root.active ? root.siteColor : root.colors.accent
        horizontalPadding: 0
        verticalPadding: 0
    }

    // The bar, on the sidebar's edge. Square, as the wash it leads is.
    Rectangle {
        objectName: "tabBar-" + root.tabId
        anchors.left: parent.left
        anchors.leftMargin: -root.reachLeft
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 2
        visible: root.showsBar
        color: root.colors.accent
    }

    // The Sidebar cursor: the row holding the keyboard. On an ordinary row it
    // is the pointer's 7% with an accent border, edge to edge: the border is
    // what tells the keyboard from the pointer, and from the tab beside, which
    // is 7% too. On the row with the bar the border has no left side, since a
    // second line of the accent on that edge would merge with the bar and hide
    // it. On a pin it is the border alone, inside the tile.
    //
    // A row holds the keyboard only while the sidebar does, so nothing is lit
    // while the reader is on the page. A press on a row focuses it too, and a
    // row reached that way is not lit.
    Omarchy.BorderSurface {
        objectName: "sidebarCursor-" + root.tabId
        anchors.fill: parent
        anchors.leftMargin: -root.reachLeft
        anchors.rightMargin: -root.reachRight
        visible: root.cursorShown
        radius: root.pinned ? Style.cornerRadius : 0
        color: root.pinned ? "transparent" : Qt.alpha(root.colors.accent, 0.07)
        borderSpec: Border.flat(root.colors.accent, root.showsBar ? "1 1 1 0" : 1)
    }

    SiteTile {
        id: tile
        objectName: "siteTile-" + root.tabId
        implicitWidth: root.chipSize
        implicitHeight: root.chipSize
        // The speaker stands in the chip's place rather than beside it: a row
        // that widened for it would shove its own title sideways every time a
        // page started and stopped playing. The chip is what the row can spare
        // — the title beside it already names the site.
        visible: !root.showsAudio || root.pinned
        anchors.left: parent.left
        anchors.leftMargin: root.pinned ? DevicePixels.snap((parent.width - width) / 2, root.pixelRatio) :
                                          root.chipInset
        anchors.verticalCenter: parent.verticalCenter
        colors: root.colors
        siteUrl: root.tabUrl
        iconUrl: root.tabIconUrl
        highlighted: root.active
        useArtwork: root.useFavicons
        tintArtwork: root.tintFavicons
        siteColoredMark: root.siteColored
    }

    // The pin's own corner, opposite the speaker, so a page being heard and a
    // page kept running can both be read off one tile. Ringed in the sidebar's
    // ground rather than left bare: the mark lies over site artwork, which is
    // any colour at all, and a dot the artwork swallows says nothing.
    Rectangle {
        objectName: "keepActive-" + root.tabId
        visible: root.showsKeepActive
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: 4
        anchors.bottomMargin: 4
        width: 8
        height: 8
        radius: width / 2
        color: root.siteColored && root.active ? root.siteColor : root.colors.accent
        border.width: 1
        border.color: root.colors.windowOpaque
        Accessible.ignored: true
    }

    // The title names the page; its address is already in the address button
    // whenever the tab is the active one, and reading it twice crowds the row.
    Text {
        visible: !root.pinned
        anchors.left: tile.right
        anchors.leftMargin: Style.space(9)
        anchors.right: parent.right
        anchors.rightMargin: root.endGap - root.reachRight + closeButton.width + 6
        anchors.verticalCenter: parent.verticalCenter
        text: root.tabTitle.length > 0 ? root.tabTitle : tile.host
        color: root.active || root.tabBeside ? root.colors.text : root.colors.mutedText
        elide: Text.ElideRight
        font.family: Style.font.family
        font.pixelSize: Style.font.body
    }

    // An Agent's tab says so at the end of its row, in the place the close
    // button takes on hover.
    Item {
        id: agentSpot
        objectName: "agentSpot-" + root.tabId
        anchors.right: parent.right
        anchors.rightMargin: root.endGap - root.reachRight
        anchors.verticalCenter: parent.verticalCenter
        width: 28
        height: 28
        visible: root.showsAgent

        AgentMark {
            objectName: "agentMark-" + root.tabId
            anchors.centerIn: parent
            visible: !hoverArea.containsMouse
            busy: root.agent !== null && root.agent.busy === true
            color: root.colors.agentAccent
            font.family: root.iconFontFamily
        }

        // The mark and the close button share one place, so the note is the
        // row's: the whole row answers the pointer for it.
        Omarchy.PanelToolTip {
            objectName: "agentNote-" + root.tabId
            visible: root.showsAgent && hoverArea.containsMouse && !root.lifted
            text: root.agentNote
            fontFamily: Style.font.family
        }
    }

    // Over the chip rather than at the end of the row, where the speaker and
    // the close button already take turns.
    KeyLabel {
        objectName: "keyLabel-tab-" + root.tabId
        iconFontFamily: root.iconFontFamily
        anchors.horizontalCenter: tile.horizontalCenter
        anchors.verticalCenter: tile.verticalCenter
        keys: root.keyLabel
        shown: root.keyLabelShown
        colors: root.colors
    }

    MouseArea {
        id: hoverArea
        objectName: "tabPointer-" + root.tabId
        // The row answers the pointer as far as its wash reaches, so hover
        // begins at the sidebar's edge rather than at the list's margin. The
        // area's coordinates are therefore not the row's: a point the row
        // reports is mapped out of them first.
        anchors.fill: parent
        anchors.leftMargin: -root.reachLeft
        anchors.rightMargin: -root.reachRight
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
        // Whether the press being released carried the row. A release ends
        // the drag before the click arrives, so the click asks this rather
        // than whether the row is still lifted.
        property bool carried: false

        function report(mouse) {
            const scene = hoverArea.mapToItem(null, mouse.x, mouse.y);
            root.dragMoved(root.tabId, scene.x, scene.y);
        }

        onPressed: function (mouse) {
            hoverArea.carried = false;
            hoverArea.grabbedAt = hoverArea.mapToItem(root, mouse.x, mouse.y);
            hoverArea.pressedAt = hoverArea.mapToItem(null, mouse.x, mouse.y);
        }

        onPositionChanged: function (mouse) {
            // The area's own record of what is held, not the event's: a
            // synthesized move carries no buttons, and a right-press opening
            // the menu must not drag the row on the way.
            if (!(hoverArea.pressedButtons & Qt.LeftButton))
                return;
            const scene = hoverArea.mapToItem(null, mouse.x, mouse.y);
            if (!root.lifted) {
                // A split's half stays with its row: the core refuses to move
                // it, so the hand is not offered a drag it cannot finish.
                if (root.inSplit)
                    return;
                const travelled = Math.max(Math.abs(scene.x - hoverArea.pressedAt.x), Math.abs(
                                               scene.y - hoverArea.pressedAt.y));
                if (travelled < hoverArea.liftThreshold)
                    return;
                root.dragStarted(root.tabId);
            }
            hoverArea.report(mouse);
        }

        onReleased: function (mouse) {
            if (!root.lifted)
                return;
            hoverArea.carried = true;
            root.dragEnded(root.tabId);
        }

        // A gesture the window took away — the pointer leaving the window, or
        // something above claiming it — leaves the row where the list has
        // already put it rather than holding it in the air.
        onCanceled: if (root.lifted)
                        root.dragEnded(root.tabId)

        onClicked: function (mouse) {
            if (mouse.button === Qt.RightButton) {
                root.reachedByPointer = true;
                root.forceActiveFocus();
                const at = hoverArea.mapToItem(root, mouse.x, mouse.y);
                root.openMenu(at.x, at.y);
                return;
            }
            // A row that has just been carried into place was not clicked.
            if (root.lifted || hoverArea.carried)
                return;
            root.reachedByPointer = true;
            root.forceActiveFocus();
            const overClose = !root.pinned && closeButton.covers(mouse.x);
            if (audioButton.covers(mouse.x, mouse.y)) {
                root.muteToggled(root.tabId);
            } else if (overClose) {
                root.closeRequested(root.tabId);
            } else {
                root.activated(root.tabId);
            }
        }

        // A pin has only its chip to say which site it is, so its speaker goes
        // in the corner over it rather than in its place.
        Omarchy.BorderSurface {
            id: audioButton
            objectName: "audio-" + root.tabId
            function covers(x, y) {
                return audioButton.visible && x >= audioButton.x && x < audioButton.x
                        + audioButton.width && y >= audioButton.y && y < audioButton.y
                        + audioButton.height;
            }
            readonly property bool hot: hoverArea.containsMouse && covers(hoverArea.mouseX,
                                                                          hoverArea.mouseY)
            readonly property color foreground: root.siteColored && root.active ? root.siteColor : (
                                                                                      root.silenced
                                                                                      || !root.active
                                                                                      ? root.colors.mutedText :
                                                                                        root.colors.text)
            anchors.left: root.pinned ? undefined : parent.left
            anchors.leftMargin: root.reachLeft + root.chipInset
            anchors.verticalCenter: root.pinned ? undefined : parent.verticalCenter
            anchors.right: root.pinned ? parent.right : undefined
            anchors.rightMargin: 2
            anchors.top: root.pinned ? parent.top : undefined
            anchors.topMargin: 2
            width: root.chipSize
            height: root.chipSize
            visible: root.showsAudio
            radius: Style.cornerRadius
            color: hot ? Style.hoverFillFor(audioButton.foreground, root.colors.accent) :
                         "transparent"
            borderSpec: hot ? Border.controlSpec("hover-cursor", audioButton.foreground,
                                                 root.colors.accent) : Border.none()
            Accessible.role: Accessible.Button
            Accessible.name: (root.tabMuted ? qsTr("Unmute %1") : (root.tabSoundSuppressed ? qsTr(
                                                                                                 "Allow sound from %1") :
                                                                                             qsTr("Mute %1"))).arg(
                                 root.tabTitle)
            Accessible.onPressAction: root.muteToggled(root.tabId)

            Text {
                anchors.centerIn: parent
                text: root.silenced ? "volume_off" : "volume_up"
                color: audioButton.foreground
                font.family: root.iconFontFamily
                font.pixelSize: Style.font.iconLarge
            }
        }

        Omarchy.BorderSurface {
            id: closeButton
            objectName: "close-" + root.tabId
            property string accessibleName: qsTr("Close %1").arg(root.tabTitle)
            // The row reaches past the button to the sidebar's edge, and a press
            // out there opens the row rather than closing it.
            function covers(x) {
                return x >= closeButton.x && x < closeButton.x + closeButton.width;
            }
            readonly property bool hot: hoverArea.containsMouse && covers(hoverArea.mouseX)
            property color foreground: hoverArea.containsMouse ? root.colors.mutedText :
                                                                 "transparent"
            anchors.right: parent.right
            anchors.rightMargin: root.endGap
            anchors.verticalCenter: parent.verticalCenter
            width: 28
            height: 28
            visible: !root.pinned
            radius: Style.cornerRadius
            color: hot ? Style.hoverFillFor(root.colors.mutedText, root.colors.accent) :
                         "transparent"

            borderSpec: hot ? Border.controlSpec("hover-cursor", root.colors.mutedText,
                                                 root.colors.accent) : Border.none()
            Accessible.role: Accessible.Button
            Accessible.name: accessibleName
            Accessible.onPressAction: root.closeRequested(root.tabId)

            Text {
                anchors.centerIn: parent
                text: "close"
                color: closeButton.foreground
                font.family: root.iconFontFamily
                font.pixelSize: Style.font.iconLarge
            }
        }
    }
}
