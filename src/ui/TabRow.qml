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
    required property bool active
    required property bool loading
    required property bool tabAudible
    required property bool tabMuted
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

    // The 18px slot an ordinary row gives its site chip, and where it starts.
    // The speaker takes the same box, so the two never sit in different
    // places and the title beside them never moves.
    readonly property int chipSize: 18
    readonly property int chipInset: 8

    signal activated(string tabId)
    signal closeRequested(string tabId)
    signal muteToggled(string tabId)
    // A drag or a keypress asking for this row to change places, by whole rows
    // within its own section. The row says how far, not where: which section it
    // is in and where that ends is the core's to know.
    signal moveRequested(string tabId, int offset)
    // The row's own menu, opened by pointer or by keyboard. Scene coordinates,
    // because the menu hangs in the window rather than inside the row.
    signal menuRequested(string tabId, real anchorX, real anchorY)

    height: pinned ? 44 : 36
    activeFocusOnTab: true
    Accessible.role: Accessible.PageTab
    Accessible.name: (pinned ? "Pinned: " + tabTitle : tabTitle)
        + (tabMuted ? " (muted)" : (tabAudible ? " (playing audio)" : ""))
    Accessible.onPressAction: root.activated(root.tabId)

    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
            root.activated(root.tabId)
            event.accepted = true
        } else if (event.key === Qt.Key_F10 && (event.modifiers & Qt.ShiftModifier)) {
            root.openMenu(0, root.height)
            event.accepted = true
        }
    }

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
        // Where the drag started, in scene coordinates. The row itself moves
        // out from under the pointer as the drag lands, so its own frame is no
        // use for measuring how far the hand has travelled.
        property point dragOrigin: Qt.point(0, 0)
        property bool dragged: false
        // Ordinary rows are stacked, pins are laid out along the row, so a
        // whole place is a row's height in one and its width in the other.
        readonly property real dragStep: root.pinned ? root.width : root.height

        onPressed: function(mouse) {
            hoverArea.dragOrigin = root.mapToItem(null, mouse.x, mouse.y)
            hoverArea.dragged = false
        }

        onPositionChanged: function(mouse) {
            if (!hoverArea.pressed || !(mouse.buttons & Qt.LeftButton)) return
            if (hoverArea.dragStep <= 0) return
            const point = root.mapToItem(null, mouse.x, mouse.y)
            const delta = root.pinned
                ? point.x - hoverArea.dragOrigin.x
                : point.y - hoverArea.dragOrigin.y
            if (Math.abs(delta) < hoverArea.dragStep) return
            const step = delta > 0 ? 1 : -1
            hoverArea.dragOrigin = root.pinned
                ? Qt.point(hoverArea.dragOrigin.x + step * hoverArea.dragStep,
                    hoverArea.dragOrigin.y)
                : Qt.point(hoverArea.dragOrigin.x,
                    hoverArea.dragOrigin.y + step * hoverArea.dragStep)
            hoverArea.dragged = true
            root.moveRequested(root.tabId, step)
        }

        onClicked: function(mouse) {
            if (mouse.button === Qt.RightButton) {
                root.forceActiveFocus()
                root.openMenu(mouse.x, mouse.y)
                return
            }
            // A row that has just been dragged into place was not clicked.
            if (hoverArea.dragged) {
                hoverArea.dragged = false
                return
            }
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
                : (root.tabMuted || !root.active ? root.colors.mutedText : root.colors.text)
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
            Accessible.name: (root.tabMuted ? "Unmute " : "Mute ") + root.tabTitle
            Accessible.onPressAction: root.muteToggled(root.tabId)

            Text {
                anchors.centerIn: parent
                text: root.tabMuted ? "volume_off" : "volume_up"
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
