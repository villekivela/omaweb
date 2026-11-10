import QtQuick
import "SiteStyleRule.mjs" as SiteStyleRule

// Keeps one engine's `siteStyles` where its Space puts them, as the Space
// turns from an Agent's into the reader's. The engine restyles the page it
// shows when they change.
Connections {

    required property var engine
    // The browser's SiteStyles, or null where there are none.
    required property var styles
    // The window's BrowserController, which says which Spaces are Agents'.
    required property var browser

    function apply() {
        engine.siteStyles = SiteStyleRule.stylesFor(styles, browser, engine.spaceId);
    }

    target: browser
    ignoreUnknownSignals: true
    function onAgentSpacesChanged() {
        apply();
    }
    Component.onCompleted: apply()
}
