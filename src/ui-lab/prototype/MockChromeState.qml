// THROWAWAY PROTOTYPE — see README.md in this directory.
// Stand-in for state the core does not model yet: several Spaces, and the
// security / permission / content-blocking status the sidebar has to surface.
// Tabs are NOT mocked; they come from the real BrowserController.
import QtQuick

QtObject {
    id: root

    property int activeSpaceIndex: 0

    readonly property var spaces: [
        { name: "Personal", color: "#9b87ff", letter: "P", tabs: 12 },
        { name: "Work", color: "#4fc3a1", letter: "W", tabs: 14 },
        { name: "Client · Acme", color: "#f0a35e", letter: "A", tabs: 6 },
        { name: "Scratch", color: "#6aa9ff", letter: "S", tabs: 2 }
    ]

    readonly property var activeSpace: spaces[activeSpaceIndex]

    // Status of the active tab's origin.
    property bool secure: true
    property int permissions: 2
    property int blocked: 37
    property bool blockingEnabled: true

    readonly property string statusSummary:
        (secure ? "TLS" : "no TLS")
        + " · " + permissions + (permissions === 1 ? " permission" : " permissions")
        + " · " + blocked + " blocked"
        + (blockingEnabled ? "" : " (off here)")
}
