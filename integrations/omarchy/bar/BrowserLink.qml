import QtQuick
import Quickshell
import Quickshell.Io
import Quickshell.Wayland

// One connection to Omaweb's bar socket: the browser's latest state, the
// actions sent back, and raising its window, which only the shell can do for
// a click or key the browser never saw.
Item {
    id: link

    property string socketPath: (Quickshell.env("XDG_RUNTIME_DIR") || "/tmp") + "/omaweb/bar.sock"
    // The main window and the private ones are `omaweb`, a Tab window is
    // `omaweb-tab`.
    property string appId: "omaweb"
    property string tabWindowAppId: "omaweb-tab"
    // Connected only while this is true.
    property bool active: true

    property var browser: null
    readonly property bool live: socket.connected && browser !== null

    function send(message) {
        if (!socket.connected)
            return;
        socket.write(JSON.stringify(message) + "\n");
        socket.flush();
    }

    // The window showing `tab`, told by its title, which the browser sends
    // for its main window. Where the title matches no window or more than
    // one, it is none rather than the wrong one.
    function windowOf(tab) {
        const windows = ToplevelManager.toplevels.values;
        const only = found => found.length === 1 ? found[0] : null;
        let target = null;
        if (tab && tab.poppedOut)
            target = only(windows.filter(w => w.appId === link.tabWindowAppId && w.title
                                              === tab.title));
        if (!target && link.live)
            target = only(windows.filter(w => w.appId === link.appId && w.title
                                              === link.browser.window));
        return target;
    }

    function raise(tab) {
        const target = windowOf(tab);
        if (target)
            target.activate();
    }

    function activate(tab, spaceId) {
        send({
                 "do": "activate",
                 "tab": tab.id,
                 "space": spaceId || browser.activeSpace
             });
    }

    Socket {
        id: socket
        path: link.socketPath
        parser: SplitParser {
            onRead: data => {
                try {
                    const message = JSON.parse(data);
                    if (message.type === "state")
                        link.browser = message;
                } catch (e) {}
            }
        }
        onConnectionStateChanged: if (!connected)
                                      link.browser = null
    }

    onActiveChanged: if (!active)
                         socket.connected = false

    // The socket is there only while the reader has the tabs in the bar, and
    // the browser may start after the shell.
    Timer {
        interval: 1000
        repeat: true
        running: link.active && !socket.connected
        triggeredOnStart: true
        onTriggered: socket.connected = true
    }
}
