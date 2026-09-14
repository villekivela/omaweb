import QtQuick
import qs.Commons

// A page opened by a link that asked for a new tab, shown over the tab it came
// from instead of as a tab of its own. The reader came for a look, and most
// looks end in closing: a tab for each would leave the outline holding pages
// that were never meant to stay. So the page is drawn here, over the page it
// came from, in the panel grammar every question about the browser arrives in,
// and the outline is not told. Keeping it is one command, which makes it an
// ordinary tab, page and all.
//
// The window owns the engine and its life; this is where it is drawn while it
// is a Glance. The chrome goes on answering for the tab beneath: the address,
// the find bar, back and forward are that tab's, and what the Glance's page
// wants to say it says in its own head.
//
// A `FocusScope`, for the reason `DialogPanel` is one: the page beneath settles
// its own `focus` binding after the Glance opens, and a flat item would lose
// the keyboard to it.
FocusScope {
    id: root
    objectName: "glance"

    property var colors
    property string iconFontFamily
    property bool open: false
    property bool ease: true
    // The page to blur behind the panel, when there is one. Must not be an
    // ancestor of this item.
    property Item pageSource: null
    // The engine drawing the Glance's page, parented to `pageHost` by whoever
    // built it. Null while the Glance is closed.
    property var engine: null

    readonly property string pageTitle: root.engine && root.engine.pageTitle.length > 0
                                        ? root.engine.pageTitle : "Glance"
    readonly property string pageAddress: root.engine ? String(root.engine.currentUrl) : ""
    // Where the engine goes, for the window to parent it into.
    readonly property alias pageHost: pageHost
    readonly property alias panelObjectName: panel.objectName

    signal closed
    signal openAsTabRequested

    SheetLift {
        id: lift
        shown: root.open
        ease: root.ease
    }

    // Drawn for the length of the drop.
    visible: lift.showing
    focus: root.open

    onOpenChanged: if (open)
                       root.focusPage()

    function focusPage() {
        root.forceActiveFocus();
        if (root.engine)
            root.engine.focusPage();
    }

    Keys.onPressed: function (event) {
        if (event.key === Qt.Key_Escape) {
            root.closed();
            event.accepted = true;
        }
    }

    // The scrim stands still while the panel lifts: a ground that moved would
    // show the page's edge above it for the length of the lift.
    PageBackdrop {
        objectName: "glanceBackdrop"
        anchors.fill: parent
        source: root.pageSource
        tint: root.colors.sheet
        opacity: lift.progress

        MouseArea {
            anchors.fill: parent
            onClicked: root.closed()
        }
    }

    Rectangle {
        id: panel
        objectName: "glancePanel"
        anchors.fill: parent
        // Inset enough that the page beneath is seen around it and the panel
        // reads as over the page rather than as the page.
        anchors.margins: 40
        transform: lift
        opacity: lift.progress
        radius: 3
        color: root.colors.overlay
        border.width: 1
        border.color: root.colors.accent
        clip: true

        // A click on the panel's own edge is not a click outside it.
        MouseArea {
            anchors.fill: parent
        }

        Item {
            id: head
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: panel.border.width
            height: 38

            SectionLabel {
                id: title
                objectName: "glanceTitle"
                anchors.left: parent.left
                anchors.leftMargin: 16
                anchors.right: address.left
                anchors.rightMargin: Style.spacing.md
                anchors.verticalCenter: parent.verticalCenter
                colors: root.colors
                text: root.pageTitle
                elide: Text.ElideRight
                topPadding: overshoot
                bottomPadding: overshoot
            }

            // The address beside the name, quiet, because the address trigger
            // in the sidebar goes on reporting the tab beneath and the reader
            // has to be able to see where the Glance has taken them.
            Text {
                id: address
                objectName: "glanceAddress"
                anchors.right: hint.left
                anchors.rightMargin: Style.spacing.lg
                anchors.verticalCenter: parent.verticalCenter
                width: Math.min(implicitWidth, head.width * 0.4)
                text: root.pageAddress
                elide: Text.ElideMiddle
                color: root.colors.mutedText
                font.family: Style.font.family
                font.pixelSize: Style.font.caption
            }

            Text {
                id: hint
                objectName: "glanceHint"
                anchors.right: openAsTab.left
                anchors.rightMargin: Style.spacing.md
                anchors.verticalCenter: parent.verticalCenter
                text: "esc closes"
                color: root.colors.mutedText
                font.family: Style.font.family
                font.pixelSize: Style.font.caption
            }

            ChromeButton {
                id: openAsTab
                objectName: "openGlanceAsTabButton"
                anchors.right: closeButton.left
                anchors.verticalCenter: parent.verticalCenter
                width: 30
                height: 30
                icon: "open_in_new"
                accessibleName: "Open as tab"
                fontFamily: root.iconFontFamily
                foreground: root.colors.mutedText
                accent: root.colors.accent
                onClicked: root.openAsTabRequested()
            }

            ChromeButton {
                id: closeButton
                objectName: "closeGlanceButton"
                anchors.right: parent.right
                anchors.rightMargin: Style.spacing.sm
                anchors.verticalCenter: parent.verticalCenter
                width: 30
                height: 30
                icon: "close"
                accessibleName: "Close Glance"
                fontFamily: root.iconFontFamily
                foreground: root.colors.mutedText
                accent: root.colors.accent
                onClicked: root.closed()
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: root.colors.separator
            }
        }

        // The page, on the opaque ground a webpage needs, inside the border
        // rather than under it.
        Rectangle {
            id: pageHost
            objectName: "glancePageHost"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: head.bottom
            anchors.bottom: parent.bottom
            anchors.margins: panel.border.width
            color: root.colors.windowOpaque
        }
    }
}
