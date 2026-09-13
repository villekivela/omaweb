import QtQuick

// The lift a sheet makes as it opens, from a little below its place to its
// place in the chrome's overlay time, and the drop it makes as it closes,
// back the way it came and quicker. A sheet's `transform` is one of these,
// its opacity follows `progress`, and its visibility follows `showing`, so
// it stays drawn for the length of the drop. `ease` is the reader's chrome ease.
Translate {
    id: lift

    property bool shown: false
    property bool ease: true
    // How far below its place the sheet starts; negative starts it above,
    // for a panel that unfolds from a control over it.
    property real distance: 24
    // 0 just off its place, 1 at rest.
    property real progress: 1
    property bool leaving: false
    readonly property bool showing: shown || leaving
    property NumberAnimation arrival: NumberAnimation {
        target: lift
        property: "progress"
        to: 1
        duration: 180
        easing.type: Easing.OutCubic
    }
    property NumberAnimation departure: NumberAnimation {
        target: lift
        property: "progress"
        to: 0
        duration: 120
        easing.type: Easing.InCubic
        onFinished: {
            lift.leaving = false;
            lift.progress = 1;
        }
    }

    y: (1 - progress) * distance

    onShownChanged: {
        if (shown) {
            departure.stop();
            leaving = false;
            if (!ease) {
                progress = 1;
                return;
            }
            // A sheet asked back mid-drop lifts from where it is.
            if (progress === 1)
                progress = 0;
            arrival.restart();
            return;
        }
        arrival.stop();
        if (!ease) {
            progress = 1;
            return;
        }
        leaving = true;
        departure.restart();
    }
}
