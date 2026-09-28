// PROTOTYPE (ui-language): throwaway. The lab's tab model has no frozen or
// Agent roles, so the variants read invented states off the seeded titles.
.pragma library

const agentColor = "#56b6c2";

function stateFor(title, pinned, active) {
    const state = {
        frozen: false,
        keepActive: false,
        sounding: false,
        agent: false,
        agentName: "",
        agentDoing: ""
    };
    if (title === "Library") {
        state.keepActive = true;
        state.sounding = true;
    } else if (title === "Inbox") {
        state.keepActive = true;
    } else if (title === "Generate the website's screenshots" || title === "localhost:3000") {
        state.agent = true;
        state.agentName = "claude-code";
        state.agentDoing = title === "localhost:3000" ? "reading the page" : "clicked “Files changed”";
    } else if (!pinned && !active && ["Qt Quick Scene Graph", "xdg-shell protocol",
                                      "Arch Linux - qt6-webengine", "QML Applications"].indexOf(
                   title) >= 0) {
        state.frozen = true;
    }
    return state;
}

// The one word the Ledger variant writes at the end of a row.
function wordFor(state, audible) {
    if (state.agent)
        return "agent";
    if (audible || state.sounding)
        return "playing";
    if (state.keepActive)
        return "kept";
    if (state.frozen)
        return "frozen";
    return "";
}
