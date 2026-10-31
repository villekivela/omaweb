import QtQuick
import Quickshell
import Quickshell.Wayland
import qs.Commons
import qs.Commons as Commons

// The tab switcher as an Omarchy overlay (#906), which a key opens with
// `omarchy-shell shell toggle omaweb.bar`. It talks to the browser only while
// open.
Item {
    id: root

    property string omarchyPath: Quickshell.env("OMARCHY_PATH")
    property var shell: null
    property var manifest: null
    property bool opened: false

    function open(payloadJson) {
        root.opened = true;
        if (link.live)
            Qt.callLater(card.reset);
    }

    function close() {
        root.opened = false;
    }

    function dismiss() {
        root.opened = false;
        if (root.shell && typeof root.shell.hide === "function")
            root.shell.hide((root.manifest && root.manifest.id) || "omaweb.bar");
    }

    function toggle() {
        if (root.opened)
            root.dismiss();
        else
            root.open("{}");
    }

    BrowserLink {
        id: link
        active: root.opened
        onLiveChanged: if (live && root.opened)
                           card.reset()
    }

    PanelWindow {
        visible: root.opened
        anchors {
            top: true
            bottom: true
            left: true
            right: true
        }
        color: "transparent"
        WlrLayershell.namespace: "omaweb-tabs"
        WlrLayershell.layer: WlrLayer.Overlay
        WlrLayershell.keyboardFocus: WlrKeyboardFocus.Exclusive
        exclusionMode: ExclusionMode.Ignore

        Rectangle {
            anchors.fill: parent
            color: Commons.Color.menu.scrim
        }
        MouseArea {
            anchors.fill: parent
            onClicked: root.dismiss()
        }

        // Hung from a fixed top rather than centred, so the card grows and
        // shrinks downwards as the reader types.
        TabSwitcher {
            id: card
            anchors.horizontalCenter: parent.horizontalCenter
            y: Math.round(parent.height * 0.2)
            link: link
            onDismissed: root.dismiss()
        }

        Text {
            visible: !link.live
            anchors.centerIn: parent
            text: qsTr("Omaweb is not running with its tabs in the bar")
            color: Commons.Color.menu.text
            font.family: Style.font.menuFamily
            font.pixelSize: Style.font.heading
        }
    }
}
