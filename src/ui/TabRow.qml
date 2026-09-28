import QtQuick
import qs.Commons
import qs.Ui as Omarchy
import "PrototypeFakes.js" as Fakes

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
    // Whether the reader marked this Pinned tab Keep active. A pin has no
    // title to carry the state and the tab menu is the only other place the
    // setting appears, so the row draws it: a standing decision the reader
    // cannot see is one they cannot tell they have made, or undo.
    required property bool tabKeepActive
    // The split this tab is in, as the other tab of it, and whether this tab
    // is the one on show beside the active tab. A split's two rows are one
    // row of two: each is half the width, the active half is marked as the
    // active tab is, and the tab beside is marked as on show, bordered the
    // way the kit borders every option of a row of them.
    required property string splitPartnerId
    required property bool tabBeside
    readonly property bool inSplit: splitPartnerId.length > 0
    property var colors
    property string iconFontFamily
    // PROTOTYPE (ui-language): the variant drawn, the key this row answers to,
    // and whether Ctrl is held. The state is invented; see PrototypeFakes.js.
    property string uiVariant: "0"
    property string protoKey: ""
    property bool protoShowKeys: false
    // Pins keep today's grid in every variant.
    readonly property bool protoLedger: uiVariant === "B" && !pinned
    // The Tiled variant lights the current row in the accent while the
    // sidebar holds the keyboard, and leaves it quiet while the page does.
    property bool protoFocusLit: false
    readonly property bool protoKeys: uiVariant === "C" || uiVariant === "D"
    readonly property var protoState: Fakes.stateFor(tabTitle, pinned, active)
    readonly property string protoWord: Fakes.wordFor(protoState, tabAudible)
    // The Keys variant no longer writes the Agent's act under the title.
    readonly property bool protoAgentLine: false
    // Whether the Agent is in the middle of a command. The lab has no Agent,
    // so it acts in invented bursts: two and a half seconds on, four off.
    property bool protoAgentActing: false
    Timer {
        running: root.protoKeys && root.protoState.agent
        repeat: true
        interval: root.protoAgentActing ? 2500 : 4000
        onTriggered: root.protoAgentActing = !root.protoAgentActing
    }
    property bool useFavicons: true
    property bool tintFavicons: false

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
    // here too. Without it the pin is chrome: the theme's muted border, the
    // kit's own selected fill for the active one.
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
    transform: Translate {
        x: root.carry.x
        y: root.carry.y
    }

    // A row settles into an opened place rather than jumping into it. The one
    // in the hand is not eased: it is already following the pointer.
    Behavior on carry {
        enabled: !root.lifted
        PropertyAnimation {
            duration: 110
            easing.type: Easing.OutCubic
        }
    }

    height: protoLedger ? 28 : (protoAgentLine ? 50 : (pinned ? 44 : 36))
    activeFocusOnTab: true
    Accessible.role: Accessible.PageTab
    Accessible.name: (pinned ? "Pinned: " + tabTitle : tabTitle) + (tabBeside ? " (beside)" : "") + (
                         tabMuted ? " (muted)" : (tabSoundSuppressed && tabAudible
                                                  ? " (playing silently)" : (tabAudible
                                                                             ? " (playing audio)" :
                                                                               ""))) + (showsKeepActive
                                                                                        ? " (kept active)" :
                                                                                          "")
    Accessible.onPressAction: root.activated(root.tabId)

    Keys.onPressed: function (event) {
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key
                === Qt.Key_Space) {
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

    // The kit paints hover and focus as veils over the fill, so the wash sits
    // on a plate beneath the button rather than in its background: hovering
    // the active pin then deepens the wash instead of replacing it.
    Rectangle {
        anchors.fill: parent
        visible: root.siteColored && root.active
        color: Qt.rgba(root.siteColor.r, root.siteColor.g, root.siteColor.b, 0.18)
        radius: Style.cornerRadius
    }

    // PROTOTYPE (ui-language): the Ledger row is a line, not a control. The
    // row on show is marked by a bar at its start, the way a cursor marks a
    // line, and nothing else about it is filled.
    Rectangle {
        anchors.fill: parent
        visible: root.protoLedger && (root.active || root.tabBeside || hoverArea.containsMouse)
        color: Qt.rgba(root.colors.text.r, root.colors.text.g, root.colors.text.b, root.active ? 0.07 :
                                                                                                 0.035)
    }
    Rectangle {
        visible: root.protoLedger && (root.active || root.tabBeside)
        width: 2
        height: parent.height
        color: root.active ? root.colors.accent : root.colors.mutedText
    }
    Omarchy.Button {
        id: tabButton
        visible: !root.protoLedger
        anchors.fill: parent
        // A site-coloured pin has a wash and a border of its own, so it never
        // takes the kit's selected fill. Every other current row does — an
        // unpinned one, and a pin the reader has switched site colour off for.
        active: root.active && !root.siteColored
        hasCursor: root.activeFocus || hoverArea.containsMouse
        // A pin is a bordered tile at rest, as the reader's own address field
        // is; an ordinary row takes its border only when it is the current tab,
        // which is what makes one row in the list read as the page on show.
        // The tab beside is on show too, and takes the border without the
        // fill.
        bordered: root.pinned || root.active || root.tabBeside
        foreground: root.siteColored && root.active ? root.siteColor : (root.pinned
                                                                        ? root.colors.mutedText :
                                                                          root.colors.text)
        // A pin is a control rather than a line of text, so it carries the
        // kit's control fill instead of sitting on the sidebar unfilled.
        background: root.pinned ? Style.normalFillFor(root.colors.text, root.colors.accent) :
                                  "transparent"
        accent: root.siteColored && root.active ? root.siteColor : root.colors.accent
        horizontalPadding: 0
        verticalPadding: 0
    }

    Rectangle {
        objectName: "protoFocusLit"
        anchors.fill: parent
        visible: root.protoFocusLit && root.activeFocus
        radius: Style.cornerRadius
        color: Qt.rgba(root.colors.accent.r, root.colors.accent.g, root.colors.accent.b, 0.16)
        border.width: 1
        border.color: root.colors.accent
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
        visible: !root.showsAudio || root.pinned || root.protoLedger
        anchors.left: parent.left
        anchors.leftMargin: root.pinned && !root.protoLedger ? (parent.width - width) / 2 :
                                                               root.chipInset
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: root.protoAgentLine ? -8 : 0
        opacity: root.protoLedger && root.protoState.frozen ? 0.45 : 1
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
        visible: root.showsKeepActive && !root.protoLedger
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
        id: titleText
        visible: !root.pinned || root.protoLedger
        anchors.left: tile.right
        anchors.leftMargin: 9
        anchors.right: parent.right
        anchors.rightMargin: root.protoLedger ? stateWord.implicitWidth + 16 : closeButton.width
                                                + 10
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: root.protoAgentLine ? -8 : 0
        text: root.tabTitle.length > 0 ? root.tabTitle : tile.host
        color: root.active || root.tabBeside ? root.colors.text : root.colors.mutedText
        opacity: root.protoLedger && root.protoState.frozen ? 0.55 : 1
        elide: Text.ElideRight
        font.family: Style.font.family
        font.pixelSize: Style.font.body
    }

    // PROTOTYPE (ui-language): what the page is doing, in one word at the end
    // of its line. Hover gives the place to the close button.
    Text {
        id: stateWord
        visible: root.protoLedger && root.protoWord.length > 0 && !hoverArea.containsMouse
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        text: root.protoWord
        color: root.protoState.agent ? Fakes.agentColor : (root.protoWord === "playing"
                                                           ? root.colors.accent :
                                                             root.colors.mutedText)
        opacity: root.protoWord === "frozen" ? 0.6 : 1
        font.family: Style.font.family
        font.pixelSize: Style.font.caption
    }

    // PROTOTYPE (ui-language): the Agent's name and its last act, under the
    // title, in the Keys variant.
    Text {
        visible: root.protoAgentLine
        anchors.left: titleText.left
        anchors.right: titleText.right
        anchors.top: titleText.bottom
        anchors.topMargin: 2
        text: root.protoState.agentName + " · " + root.protoState.agentDoing
        color: Fakes.agentColor
        elide: Text.ElideRight
        font.family: Style.font.family
        font.pixelSize: Style.font.caption
    }

    // PROTOTYPE (ui-language): an Agent's tab says so at the end of its row.
    // It holds still while the Agent is attached and idle, and pulses only
    // while a command is in flight, so idle chrome still draws no frames.
    Text {
        id: agentMark
        objectName: "protoAgentMark"
        visible: root.protoKeys && root.protoState.agent && !root.pinned
                 && !hoverArea.containsMouse
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        text: "smart_toy"
        color: Fakes.agentColor
        font.family: root.iconFontFamily
        font.pixelSize: Style.font.iconLarge
        opacity: 1
        SequentialAnimation on opacity {
            running: agentMark.visible && root.protoAgentActing
            loops: Animation.Infinite
            onRunningChanged: if (!running)
                                  agentMark.opacity = 1
            NumberAnimation {
                to: 0.3
                duration: 450
                easing.type: Easing.InOutSine
            }
            NumberAnimation {
                to: 1
                duration: 450
                easing.type: Easing.InOutSine
            }
        }
    }

    ProtoKeyBadge {
        anchors.horizontalCenter: tile.horizontalCenter
        anchors.verticalCenter: tile.verticalCenter
        keys: root.protoKey
        colors: root.colors
        shown: root.protoKeys && root.protoShowKeys
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
            const scene = root.mapToItem(null, mouse.x, mouse.y);
            root.dragMoved(root.tabId, scene.x, scene.y);
        }

        onPressed: function (mouse) {
            hoverArea.grabbedAt = Qt.point(mouse.x, mouse.y);
            hoverArea.pressedAt = root.mapToItem(null, mouse.x, mouse.y);
        }

        onPositionChanged: function (mouse) {
            // The area's own record of what is held, not the event's: a
            // synthesized move carries no buttons, and a right-press opening
            // the menu must not drag the row on the way.
            if (!(hoverArea.pressedButtons & Qt.LeftButton))
                return;
            const scene = root.mapToItem(null, mouse.x, mouse.y);
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
            root.dragEnded(root.tabId);
        }

        // A gesture the window took away — the pointer leaving the window, or
        // something above claiming it — leaves the row where the list has
        // already put it rather than holding it in the air.
        onCanceled: if (root.lifted)
                        root.dragEnded(root.tabId)

        onClicked: function (mouse) {
            if (mouse.button === Qt.RightButton) {
                root.forceActiveFocus();
                root.openMenu(mouse.x, mouse.y);
                return;
            }
            // A row that has just been carried into place was not clicked.
            if (root.lifted)
                return;
            root.forceActiveFocus();
            const overClose = !root.pinned && mouse.x >= root.width - closeButton.width
                  - closeButton.anchors.rightMargin;
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
            anchors.leftMargin: root.chipInset
            anchors.verticalCenter: root.pinned ? undefined : parent.verticalCenter
            anchors.right: root.pinned ? parent.right : undefined
            anchors.rightMargin: 2
            anchors.top: root.pinned ? parent.top : undefined
            anchors.topMargin: 2
            width: root.chipSize
            height: root.chipSize
            visible: root.showsAudio && !root.protoLedger
            radius: Style.cornerRadius
            color: hot ? Style.hoverFillFor(audioButton.foreground, root.colors.accent) :
                         "transparent"
            borderSpec: hot ? Border.controlSpec("hover-cursor", audioButton.foreground,
                                                 root.colors.accent) : Border.none()
            Accessible.role: Accessible.Button
            Accessible.name: (root.tabMuted ? "Unmute " : (root.tabSoundSuppressed
                                                           ? "Allow sound from " : "Mute "))
                             + root.tabTitle
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
            property string accessibleName: "Close " + root.tabTitle
            readonly property bool hot: hoverArea.containsMouse && hoverArea.mouseX >= root.width
                                        - width - anchors.rightMargin
            property color foreground: hoverArea.containsMouse ? root.colors.mutedText :
                                                                 "transparent"
            anchors.right: parent.right
            anchors.rightMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            width: 28
            height: 28
            visible: !root.pinned
            radius: root.protoLedger ? 0 : Style.cornerRadius
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
