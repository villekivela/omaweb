import QtQuick
import Omaweb

// The tab that is making sound, on its way to the desktop and back.
//
// Which tab that is belongs to the core, which hears it from every window at
// once; putting it where the desktop looks belongs to the platform. This joins
// the two and routes what the desktop sends back to the page it is for.
//
// Every window has one of these, because a tab's engine belongs to the window
// it was opened in and a media key has to reach that engine. They all describe
// the same one player: the announcement comes from the core, which has one
// answer, and a command that names a tab this window does not hold is left to
// the window that does.
QtObject {
    id: root

    required property var engineHost

    function publish() {
        if (MediaAnnouncer.available)
            MediaAnnouncer.announce(SoundingTabs.announcement);
    }

    property Connections sounding: Connections {
        target: SoundingTabs

        function onAnnouncementChanged() {
            root.publish();
        }
    }

    property Connections desktop: Connections {
        target: MediaAnnouncer

        // Which tab the key is for is the core's answer, not this window's.
        // A window that does not hold that tab does nothing, so one window
        // acts however many are open.
        function onCommanded(name) {
            root.engineHost.invokeMediaAction(SoundingTabs.soundingTabId(), name);
        }
    }
}
