import QtQuick

QtObject {
    id: root

    // Every binding comes from the keyboard-navigation configuration file, so
    // rebinding, sharing, and backing up the keymap is editing one JSON file.
    property var configuration
    property bool pageCommandsEnabled: configuration ? configuration.enabled : false

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

    // Omaweb ships for Linux, so its hints name Linux keys wherever it is run:
    // a build on another desktop is a development convenience, and a hint that
    // followed the host's symbols there would put that host's keys into every
    // screenshot taken on it.
    function displayFor(binding) {
        return binding.replace("Primary+", "Ctrl+");
    }

    // The one key a control is labelled with while Primary is held: its chord,
    // or its single key when it has none. A numbered command names the number
    // its binding ends in. A single key is only offered while single keys are
    // answered, so a label never promises a key the window would ignore.
    function labelFor(command, number) {
        let single = "";
        for (const binding in browserBindings) {
            if (browserBindings[binding] !== command) {
                continue;
            }
            if (number !== undefined && binding.slice(-1) !== String(number)) {
                continue;
            }
            if (isChord(binding)) {
                return displayFor(binding);
            }
            if (single.length === 0 && pageCommandsEnabled) {
                single = binding;
            }
        }
        return single;
    }

    // The keys the Omnibar's field answers, by the name of the key and what it
    // does there. The field takes its keys from here and its hint row names
    // them from here, so a key the row shows is a key the field answers.
    property var omnibarBindings: ({
                                       "Up": "previous",
                                       "Down": "next",
                                       "Return": "go",
                                       "Enter": "go",
                                       "Backspace": "leave"
                                   })

    function keyName(key) {
        switch (key) {
        case Qt.Key_Up:
            return "Up";
        case Qt.Key_Down:
            return "Down";
        case Qt.Key_Left:
            return "Left";
        case Qt.Key_Right:
            return "Right";
        case Qt.Key_Return:
            return "Return";
        case Qt.Key_Enter:
            return "Enter";
        case Qt.Key_Backspace:
            return "Backspace";
        }
        return "";
    }

    // What the field does on a key, or an empty string for a key it leaves
    // to typing.
    function omnibarActionFor(key) {
        return omnibarBindings[keyName(key)] || "";
    }

    // The keys that do any of `actions`, by name, for the caps to draw. Return
    // and the keypad's Enter are one key to the reader, so one is named.
    function omnibarKeysFor(actions) {
        const keys = [];
        for (const binding in omnibarBindings) {
            const name = binding === "Enter" ? "Return" : binding;
            if (actions.indexOf(omnibarBindings[binding]) >= 0 && keys.indexOf(name) < 0)
                keys.push(name);
        }
        return keys;
    }

    readonly property var omnibarKeys: ({
                                            "select": omnibarKeysFor(["previous", "next"]),
                                            "go": omnibarKeysFor(["go"]),
                                            "run": omnibarKeysFor(["go"]),
                                            "leave": omnibarKeysFor(["leave"])
                                        })

    // Every binding that invokes a command, formatted for the Omnibar.
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
