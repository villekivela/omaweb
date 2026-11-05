import QtQuick
import "PageDarkening.mjs" as PageDarkening

// Keeps one engine's `darkenPage` where the setting and the site's choice put
// it, as the page, the setting or the choice moves. The engine applies it to
// the page it shows, so a site turned off in Site information takes effect in
// each of its open pages in that Space, and a window opened by a page follows
// the same rule as a tab.
Connections {

    required property var engine
    // The browser's PageDarkening, or null where there is none.
    required property var setting
    // The window's BrowserController, which holds the sites turned off.
    required property var browser

    function apply() {
        engine.darkenPage = PageDarkening.wanted(setting, browser, engine.currentUrl,
                                                 engine.spaceId);
    }

    target: engine
    function onCurrentUrlChanged() {
        apply();
    }
    Component.onCompleted: apply()

    property Connections settingChanges: Connections {
        target: setting
        ignoreUnknownSignals: true
        function onEnabledChanged() {
            apply();
        }
    }

    property Connections choiceChanges: Connections {
        target: browser
        ignoreUnknownSignals: true
        function onDarkeningOffChanged() {
            apply();
        }
    }
}
