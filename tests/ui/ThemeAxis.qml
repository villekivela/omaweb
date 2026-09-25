import QtQuick
import qs.Commons

// The two axes a theme can move a derived layout along, a stated place to
// start them from, and the shell singleton put back the way it was found.
//
// A test that asserts a layout is derived rather than written down has to drive
// both axes — the type the theme sets and the rhythm it sets — and drive them
// one at a time, so that neither can stand in for the other. This is shared
// because more than one page asks that question, and a second copy of the token
// table would drift from the first.
QtObject {
    id: root

    property var savedFontOverrides: ({})
    property real savedSpacingScale: 1
    property bool savedSpacingScaleWithFont: true
    property var savedSpacingOverrides: ({})

    function remember() {
        root.savedFontOverrides = Style.fontOverrides;
        root.savedSpacingScale = Style.spacingScale;
        root.savedSpacingScaleWithFont = Style.spacingScaleWithFont;
        root.savedSpacingOverrides = Style.spacingOverrides;
    }

    function restore() {
        Style.fontOverrides = root.savedFontOverrides;
        Style.spacingScale = root.savedSpacingScale;
        Style.spacingScaleWithFont = root.savedSpacingScaleWithFont;
        Style.spacingOverrides = root.savedSpacingOverrides;
    }

    // The kit's own tokens at its own scale. The shell reads the desktop's
    // Omarchy theme, and a theme may set a spacing scale or pin a token, which
    // a spacing token then keeps whatever the scale does. A layout measured
    // against that desktop answers for the desktop rather than for the page,
    // so a test starts here and moves one axis from a place it names.
    function useStatedTheme() {
        Style.fontOverrides = ({});
        Style.spacingScale = 1;
        Style.spacingScaleWithFont = true;
        Style.spacingOverrides = ({});
    }

    // The type the theme sets. shell.toml pins these tokens directly, and
    // Omaweb's own theme sets the base size the kit derives them from; either
    // way the page has to re-measure. `fontBaseSize` itself cannot be driven
    // from here, because Omaweb's theme owns it and writes it straight back.
    function useTypeTokens(scale) {
        Style.fontOverrides = ({
                                   "caption": Math.round(10 * scale),
                                   "body-small": Math.round(11 * scale),
                                   "body": Math.round(12 * scale),
                                   "subtitle": Math.round(13 * scale),
                                   "title": Math.round(14 * scale),
                                   "heading": Math.round(16 * scale),
                                   "display": Math.round(24 * scale),
                                   "display-large": Math.round(28 * scale)
                               });
    }

    // The rhythm the theme sets, which is the other half of the same question:
    // `[spacing] scale` makes the whole shell denser or roomier without
    // touching the type.
    function useSpacingScale(scale) {
        Style.spacingScale = scale;
    }
}
