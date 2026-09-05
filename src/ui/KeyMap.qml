import QtQuick

QtObject {
    id: root

    // Every binding comes from the keyboard-navigation configuration file, so
    // rebinding, sharing, and backing up the keymap is editing one JSON file.
    property var configuration
    property bool pageCommandsEnabled: configuration ? configuration.enabled : false

    readonly property string primaryLabel: Qt.platform.os === "osx" ? "⌘" : "Ctrl+"
    readonly property string shiftLabel: Qt.platform.os === "osx" ? "⇧" : "Shift+"
    readonly property string altLabel: Qt.platform.os === "osx" ? "⌥" : "Alt+"

    readonly property var browserBindings: configuration ? configuration.browserBindings : ({})

    // Chords are always live: they cannot be confused with typing on a page.
    // Single keys follow the Keyboard navigation setting.
    function isChord(binding) {
        return binding.indexOf("+") !== -1;
    }

    function sequences() {
        const out = [];
        for (const binding in browserBindings) {
            if (!isChord(binding) && binding.length > 1) {
                out.push(binding);
            }
        }
        return out;
    }

    function commandFor(binding) {
        return browserBindings[binding] !== undefined ? browserBindings[binding] : "";
    }

    // Bindings become QKeySequences so Qt dispatches them window-wide, before
    // the focused page. Editable fields on a page still win: the engine sends a
    // shortcut override for them, which is what keeps typing intact.
    // "gt" becomes "g,t"; an upper-case letter becomes an explicit Shift chord.
    // In a QKeySequence "Ctrl" is the Command key on macOS and the Control key
    // everywhere else, which is exactly what "Primary" means. "Meta" is the
    // other one — the physical Control key on macOS — so it must not appear
    // here, or the window binds ⌃L while the hints promise ⌘L.
    function keySequence(binding) {
        if (isChord(binding)) {
            return binding.replace("Primary", "Ctrl");
        }
        const steps = [];
        for (let index = 0; index < binding.length; ++index) {
            const key = binding.charAt(index);
            steps.push(key >= "A" && key <= "Z" ? "Shift+" + key : key);
        }
        return steps.join(",");
    }

    function displayFor(binding) {
        if (Qt.platform.os !== "osx") {
            return binding.replace("Primary+", "Ctrl+");
        }
        return binding.replace("Primary+", primaryLabel).replace("Alt+", altLabel).replace("Shift+",
                                                                                           shiftLabel);
    }

    // Every binding that invokes a command, formatted for the command panel.
    function keysFor(command) {
        const chords = [];
        const keys = [];
        for (const binding in browserBindings) {
            if (browserBindings[binding] !== command) {
                continue;
            }
            if (isChord(binding)) {
                chords.push(displayFor(binding));
            } else {
                keys.push(binding);
            }
        }
        if (command === "select-tab" || command === "select-space") {
            return chords.length > 0 ? chords[0].replace(/[0-9]$/, "N") : "1…9";
        }
        return chords.concat(keys).join("  ·  ");
    }

    function pageKeysFor(command) {
        const bindings = configuration ? configuration.bindings : ({});
        const keys = [];
        for (const binding in bindings) {
            if (bindings[binding] === command) {
                keys.push(binding);
            }
        }
        return keys.join("  ·  ");
    }
}
