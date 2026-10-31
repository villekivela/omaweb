import QtQuick
import Quickshell
import Quickshell.Hyprland
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
    // The browser's window, raised once the overlay is gone: Hyprland keeps
    // the focus on a surface that holds the keyboard as the overlay does.
    property var raising: null

    // Hyprland moves to the window's workspace for a focus by its address,
    // which it does not for a toplevel's own request to be raised.
    function focus(window) {
        const hyprland = Hyprland.toplevels.values.find(each => each.wayland === window);
        const address = hyprland ? String(hyprland.address) : "";
        if (address === "") {
            window.activate();
            return;
        }
        const at = "address:" + (address.startsWith("0x") ? address : "0x" + address);
        Quickshell.execDetached(Hyprland.usingLua ? ["hyprctl", "dispatch",
                                                     "hl.dsp.focus({ window = \"" + at + "\" })"] :
                                                    ["hyprctl", "dispatch", "focuswindow", at]);
    }

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

    Timer {
        id: raise
        interval: 50
        onTriggered: {
            if (root.raising && !root.opened)
                root.focus(root.raising);
            root.raising = null;
        }
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
            onTaken: function (window) {
                root.raising = window;
                root.dismiss();
                raise.restart();
            }
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
