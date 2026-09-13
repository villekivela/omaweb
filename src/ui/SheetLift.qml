import QtQuick

// The lift a sheet makes as it opens: from a little below its place to its
// place, in the chrome's overlay time. A sheet's `transform` is one of these
// and its opacity follows `progress`, so the Start page, settings and history
// arrive as one kind of thing. `ease` is the reader's refusal of the sidebar
// ease, which refuses this too.
Translate {
    id: lift

    property bool shown: false
    property bool ease: true
    // How far below its place the sheet starts; negative starts it above,
    // for a panel that unfolds from a control over it.
    property real distance: 24
    // 0 just below, 1 at rest.
    property real progress: 1
    property NumberAnimation arrival: NumberAnimation {
        target: lift
        property: "progress"
        to: 1
        duration: 180
        easing.type: Easing.OutCubic
    }

    y: (1 - progress) * distance

    onShownChanged: {
        if (!shown) {
            arrival.stop();
            progress = 1;
            return;
        }
        if (ease) {
            progress = 0;
            arrival.restart();
        }
    }
}
