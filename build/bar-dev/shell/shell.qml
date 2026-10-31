import QtQuick
import Quickshell
import Quickshell.Io
import qs.Commons
import qs.Commons as Commons
import "bar"

// Scratch harness for #906: a stand-in screen with the worktree's widgets in
// a bar strip along its top and the tab switcher open over it, reloaded by
// Quickshell on every save. BAR_SOCKET picks the feed: the installed
// browser's own socket by default, or fake-feed.py's.
ShellRoot {
    // The real overlay, as the shell would open it:
    // qs ipc -p <this file> call omaweb-harness toggle
    OmawebTabs {
        id: overlay
    }
    IpcHandler {
        target: "omaweb-harness"
        function toggle(): void { overlay.toggle() }
        function variant(which: int): void { screen.core.variant = which }
    }
    FloatingWindow {
        id: harness
        implicitWidth: 1400
        implicitHeight: 860
        color: "#2a2f3a"

        function feed(extra) {
            const socket = Quickshell.env("BAR_SOCKET");
            return Object.assign(socket ? { socket: socket } : {}, extra);
        }

        QtObject {
            id: fakeBar
            property color barForeground: Commons.Color.bar.text
            property color background: Commons.Color.bar.background
            property string fontFamily: Style.font.family
            property bool vertical: false
            property int barSize: Style.bar.sizeHorizontal
            property string position: "top"
            function showTooltip(target, text) { tip.text = text }
            function hideTooltip(target) { tip.text = "" }
        }

        Item {
            id: screen
            anchors.fill: parent
            property alias core: core

            Rectangle {
                width: parent.width
                height: fakeBar.barSize
                color: fakeBar.background
                Row {
                    anchors.verticalCenter: parent.verticalCenter
                    x: 8
                    spacing: 6
                    Text { text: "  1 2 3 4"; color: fakeBar.barForeground; font.family: fakeBar.fontFamily; anchors.verticalCenter: parent.verticalCenter }
                    OmawebBar { bar: fakeBar; settings: harness.feed({ role: "tabs", appId: Quickshell.env("BAR_FOCUSED_APP") || "org.quickshell", spaceNames: Quickshell.env("BAR_SPACE_NAMES") || "full" }) }
                }
                OmawebBar {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.verticalCenter: parent.verticalCenter
                    bar: fakeBar
                    settings: harness.feed({ role: "location", appId: Quickshell.env("BAR_FOCUSED_APP") || "org.quickshell" })
                }
            }

            // The switcher, open over the stand-in screen as the overlay
            // would be.
            Rectangle {
                anchors.fill: parent
                color: Commons.Color.menu.scrim
            }
            BrowserLink {
                id: cardLink
                socketPath: Quickshell.env("BAR_SOCKET") || ((Quickshell.env("XDG_RUNTIME_DIR") || "/tmp") + "/omaweb/bar.sock")
                onLiveChanged: if (live) core.reset()
            }
            SwitcherCore {
                id: core
                anchors.fill: parent
                link: cardLink
                onDismissed: reset()
            }
            PrototypeSwitcherBar {
                core: core
            }
        }

        Text { id: tip; anchors.bottom: parent.bottom; x: 8; color: "white"; font.family: fakeBar.fontFamily }
    }
}
