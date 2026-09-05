import QtQuick
import Omaweb

// Notifications a page asked for, on their way to the desktop and back.
//
// They arrive from a Space's profile rather than from one page, so the origin
// is all there is to say who sent one. Deciding whether that origin is entitled
// to interrupt the reader is the core's; naming the sender in words the reader
// can act on is this object's; showing it is the desktop's. The three are kept
// apart so a platform with no notification service still refuses exactly the
// notifications it should, and so a page is told its notification closed rather
// than being left waiting on an answer nobody can give.
QtObject {
    id: root

    required property var browser
    // Whether the window may raise notifications at all. A Private window may
    // not: a desktop notification records the origin in a list that outlives
    // the private session and is read by whoever is at the machine.
    property bool allowed: true

    // The desktop is asked to take the reader to a tab, which is the window's
    // to do — the answer may mean changing Space and raising the window.
    signal activationRequested(string spaceId, string tabId)

    // What the desktop is showing on Omaweb's behalf, by the key it was given.
    // Omaweb's own key rather than the desktop's, because the page waiting on
    // each one belongs to a tab in a Space and none of that is the desktop's
    // to keep. The page hears a click or a dismissal, and nothing until then.
    readonly property var pending: ({})

    // One profile, once. A Space's profile may be built by the window or by the
    // engine host — a Space whose pages are only being retained may never have
    // been visited — so whoever builds one brings it here.
    function watch(spaceId, host) {
        if (!host || host.notificationObserversConnected)
            return
        host.notificationObserversConnected = true
        host.notificationPresented.connect(function (notificationId, origin, title, message) {
            root.present(spaceId, host, notificationId, origin, title, message)
        })
    }

    function present(spaceId, host, notificationId, origin, title, message) {
        const target = root.allowed ? root.browser.notificationTarget(spaceId, origin) : null
        // A page whose Space has been put away, and which nothing is keeping
        // running, has no business interrupting: only a retained tab can speak
        // for an inactive Space.
        if (!target || !target.tabId) {
            host.dismissNotification(notificationId)
            return
        }
        const key = spaceId + ":" + notificationId
        // Origin and Space, always and first: which site is asking, and which
        // browsing identity it is asking in. The page's own words follow.
        const heading = target.origin + " · " + target.spaceName
        const detail = title.length > 0 && message.length > 0 ? title + " — " + message : (
                                                                    title.length > 0 ? title :
                                                                                       message)
        root.pending[key] = {
            "spaceId": spaceId,
            "tabId": target.tabId,
            "host": host,
            "notificationId": notificationId,
            "heading": heading,
            "detail": detail
        }
        // Nothing on this desktop to show it with. The reader will not see it,
        // so the page is told it closed rather than left waiting.
        if (!SystemNotifier.present(key, heading, detail)) {
            host.dismissNotification(notificationId)
        }
    }

    // The reader answered one — or another window's reader did, since the
    // desktop's notifications reach every window in the process. A key this
    // window never handed out is not this window's to answer.
    function answer(key, activated) {
        const waiting = root.pending[key]
        if (!waiting)
            return
        delete root.pending[key]
        if (!activated) {
            waiting.host.dismissNotification(waiting.notificationId)
            return
        }
        waiting.host.activateNotification(waiting.notificationId)
        root.activationRequested(waiting.spaceId, waiting.tabId)
    }

    property Connections desktop: Connections {
        target: SystemNotifier

        function onActivated(key) {
            root.answer(key, true)
        }
        function onDismissed(key) {
            root.answer(key, false)
        }
    }
}
