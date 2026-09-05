import QtQuick
import QtQuick.Controls
import QtTest
import qs.Commons
import qs.Ui as Omarchy
import "../../src/ui" as Omaweb

// The vendored Omarchy kit reaches Omaweb through three seams: the Quickshell
// shim its singletons import, the `qs.Commons` tokens its components read, and
// the adapters in src/ui. Each is cheap to break during a sync, so each is
// asserted here rather than left to a panel nobody opens.
TestCase {
    id: testCase
    name: "OmarchyKit"
    when: windowShown

    property var colorsFixture: ({
                                     text: "#f3f1fa",
                                     mutedText: "#8d88a3",
                                     accent: "#9b87ff",
                                     privateAccent: "#dc6bce",
                                     border: "#4a4658",
                                     surface: "#26232f",
                                     surfaceHover: "#3d394e",
                                     windowOpaque: "#16151d"
                                 })

    Component {
        id: actionButtonComponent

        Omaweb.ActionButton {
            colors: testCase.colorsFixture
            label: "Add subscription"
        }
    }

    Component {
        id: chromeButtonComponent

        Omaweb.ChromeButton {
            foreground: testCase.colorsFixture.mutedText
            accent: testCase.colorsFixture.accent
            icon: "settings"
            accessibleName: "Browsing settings and downloads"
        }
    }

    Component {
        id: sectionLabelComponent

        Omaweb.SectionLabel {
            colors: testCase.colorsFixture
            text: "pinned"
        }
    }

    Component {
        id: settingFieldComponent

        Omaweb.SettingField {
            colors: testCase.colorsFixture
            placeholder: "list name"
            accessibleName: "Subscription name"
        }
    }

    Component {
        id: multilineFieldComponent

        Omaweb.MultilineField {
            colors: testCase.colorsFixture
            placeholder: "one rule per line"
            accessibleName: "User rules"
        }
    }

    Component {
        id: settingToggleComponent

        Omaweb.SettingToggle {
            colors: testCase.colorsFixture
            title: "Use site favicons"
        }
    }

    Component {
        id: settingCheckboxComponent

        Omaweb.SettingCheckbox {
            colors: testCase.colorsFixture
            title: "Cookies"
            note: "Signs out of sites that kept you signed in."
        }
    }

    Component {
        id: toggleComponent

        Omarchy.Toggle {
            foreground: testCase.colorsFixture.text
            accent: testCase.colorsFixture.accent
            label: "Keyboard navigation"
            description: "Omaweb's own command layer."
        }
    }

    // `Style` and `Color` are Quickshell-backed singletons; a missing shim type
    // takes the whole kit down, and the failure surfaces here as a token that
    // never resolved.
    function test_commonsTokensResolve() {
        verify(Style.font.body > 0);
        verify(Style.spacing.controlPaddingX >= 0);
        verify(Color.foreground.a > 0);
        verify(Border.none() !== undefined);
    }

    // Omaweb's own Typography object is gone: the kit's scale is the only type
    // scale, so every size a Omaweb surface asks for has to exist on it.
    function test_theKitScaleCoversEverySizeOmawebAsksFor() {
        verify(Style.font.caption > 0);
        verify(Style.font.body >= Style.font.caption);
        verify(Style.font.title >= Style.font.body);
        verify(Style.font.heading >= Style.font.title);
        verify(Style.font.iconLarge > 0);
        verify(Style.font.family.length > 0);
    }

    // The kit's Commons singletons resolve colour and type from an Omarchy
    // theme on disk, which is not Omaweb's source of truth. ThemeController is,
    // so the palette is pushed into them (#11).
    function test_theKitTypeComesFromOmawebsTheme() {
        const font = theme.palette.font;
        compare(Style.fontFamily, font.family);
        compare(Style.font.family, font.family);
        compare(Style.font.resolvedFamily, font.family);
        compare(Style.font.baseSize, font.size);
        // Never a fontconfig alias: "monospace" does not exist on macOS, where
        // asking for it draws the wrong face and costs a font-alias sweep at
        // every startup.
        verify(font.family !== "monospace");
    }

    function test_theKitPaletteComesFromOmawebsTheme() {
        compare(String(Color.foreground), String(theme.palette.text));
        // The kit paints `background` as a solid surface, so it takes the
        // opaque window rather than the alpha the desktop shows through.
        compare(String(Color.background), String(theme.palette.windowOpaque));
        compare(String(Color.accent), String(theme.palette.accent));
        compare(String(Color.muted), String(theme.palette.mutedText));
    }

    // The kit reaches for its own theme through watched files and short-lived
    // processes, and those land after startup. A palette pushed once loses to
    // whichever of them writes last, so the seam pushes again — which is what
    // makes ThemeController authoritative rather than merely first.
    function test_theKitCannotOutlastOmawebsPalette() {
        Color.loadColors("foreground = \"#ff0000\"\naccent = \"#00ff00\"");
        compare(String(Color.foreground), String(theme.palette.text));
        compare(String(Color.accent), String(theme.palette.accent));

        // A shell.toml reaching Style resets the type base size along with
        // everything else it owns.
        Color.loadUserShell("[font]\nbase-size = 20\n");
        compare(Style.font.baseSize, theme.palette.font.size);

        // Leave the kit as the rest of the suite expects to find it. The
        // palette is restored by the seam; the parsed dicts are not.
        Color.loadUserShell("");
        Color.loadColors("");
    }

    // Palette normalization can reject two different desktop palettes to the
    // same built-in colours. The desktop still switched, so the kit must
    // reread shell.toml even though ThemeController's palette did not change.
    function test_theKitRereadsDesktopStyleWhenTheThemeReloads() {
        const sentinel = 0.731;
        Color.loadShell("[controls]\nnormal-fill-alpha = " + sentinel + "\n");
        compare(Style.normalFillAlpha, sentinel);

        theme.reload();
        const reloadedAlpha = Style.normalFillAlpha;

        // Restore a predictable baseline before reporting a failure.
        Color.loadShell("");
        verify(Math.abs(reloadedAlpha - sentinel) > 0.0001);
    }

    function test_actionButtonUsesTheKitAndOmawebPalette() {
        const button = createTemporaryObject(actionButtonComponent, testCase);
        verify(button !== null);
        // Sized by the kit's padding and the label, not by Omaweb's old fixed
        // 30px box.
        verify(button.implicitWidth > 0);
        verify(button.implicitHeight > 0);
        compare(button.text, "Add subscription");
        compare(String(button.foreground), "#f3f1fa");
        compare(String(button.accent), "#9b87ff");
        // Type comes from the kit's scale now, not a per-instance block.
        compare(button.fontSize, Style.font.body);
        compare(button.fontFamily, Style.font.family);
    }

    function test_destructiveActionButtonTakesThePrivateAccent() {
        const button = createTemporaryObject(actionButtonComponent, testCase, {
                                                 destructive: true
                                             });
        compare(String(button.accent), "#dc6bce");
    }

    function test_actionButtonClickReachesTheCallSite() {
        let clicks = 0;
        const button = createTemporaryObject(actionButtonComponent, testCase);
        button.clicked.connect(function () {
            clicks += 1;
        });
        // Omaweb is keyboard-driven, and the activation keys are the kit's.
        button.forceActiveFocus();
        verify(button.activeFocus);
        keyClick(Qt.Key_Return);
        keyClick(Qt.Key_Space);
        compare(clicks, 2);
    }

    function test_chromeButtonIsTheKitsBorderlessButton() {
        const button = createTemporaryObject(chromeButtonComponent, testCase);
        verify(button !== null);
        // Chrome is borderless and transparent at rest; the kit paints hover,
        // focus and pressed fills itself, which is why Omaweb's
        // `hoverBackground` no longer exists.
        compare(button.bordered, false);
        verify(button.hoverBackground === undefined);
        compare(String(button.foreground), "#8d88a3");
        compare(String(button.accent), "#9b87ff");
        // A glyph is sized by the kit's larger icon token, not by the body
        // text token: chrome is aimed at rather than read.
        compare(button.icon, "settings");
        compare(button.iconSize, Style.font.iconLarge);
    }

    function test_chromeButtonClickReachesTheCallSite() {
        let clicks = 0;
        const button = createTemporaryObject(chromeButtonComponent, testCase);
        button.clicked.connect(function () {
            clicks += 1;
        });
        button.forceActiveFocus();
        verify(button.activeFocus);
        keyClick(Qt.Key_Return);
        keyClick(Qt.Key_Space);
        compare(clicks, 2);
    }

    function test_sectionLabelIsTheKitsPanelSectionHeader() {
        const label = createTemporaryObject(sectionLabelComponent, testCase);
        verify(label !== null);
        // Omaweb sets the label larger than the kit's caption size and in
        // capitals, so a section reads as what divides the rows rather than as
        // one more row.
        compare(label.fontSize, Style.font.subtitle);
        compare(label.font.capitalization, Font.AllUppercase);
        compare(String(label.foreground), "#f3f1fa");
        // The kit derives its own muting from `foreground` rather than reading
        // a separate muted role, and Omaweb does not letter-space on top of the
        // capitals.
        verify(String(label.color) !== String(label.foreground));
        compare(label.font.letterSpacing, 0);
        // The capitals are drawn, not written: what a screen reader is handed
        // is still the label as the call site set it.
        compare(label.text, "pinned");
        compare(label.Accessible.name, "pinned");
        // The label carries its own separation, because the containers it sits
        // in cannot: a Column's spacing is the same gap on both sides, and the
        // settings pane sets none at all. It belongs to what follows it, so
        // there is more room above it than below.
        verify(label.topPadding > label.bottomPadding);
        verify(label.bottomPadding > 0);

        // That lean is separation from what precedes the label, so a call site
        // with nothing above it — or one centring the label in a bar of its own
        // — asks for `overshoot` instead of for zero: the kit reserves that
        // sliver so a tall glyph is not clipped, and no placement gives it up.
        verify(label.overshoot > 0);
        verify(label.topPadding > label.overshoot);
        compare(label.topPadding, Style.spacing.huge + label.overshoot);
    }

    // A Qt Quick Controls native style refuses the kit's replaced `background`
    // and paints its own instead, so the shim pins the style the kit draws on.
    // Without it the settings fields silently render as platform boxes, and
    // nothing else in the suite notices.
    function test_theControlsStyleLetsTheKitPaintItsOwnBackground() {
        compare(controlsStyle, "Basic");
    }

    function test_settingFieldIsTheKitsTextField() {
        const field = createTemporaryObject(settingFieldComponent, testCase);
        verify(field !== null);
        compare(field.placeholderText, "list name");
        compare(String(field.foreground), "#f3f1fa");
        compare(String(field.accent), "#9b87ff");
        compare(field.font.pixelSize, Style.font.body);

        // The dialogs drive the field by name, so the two calls they make have
        // to survive the swap.
        field.text = "EasyList";
        field.focusInput();
        verify(field.activeFocus);
        field.selectAllText();
        compare(field.selectedText, "EasyList");
    }

    function test_destructiveSettingFieldTakesThePrivateAccent() {
        const field = createTemporaryObject(settingFieldComponent, testCase, {
                                                destructive: true
                                            });
        compare(String(field.accent), "#dc6bce");
    }

    // The kit has no text area, so the rules editor is Omaweb's own control —
    // but it reads the kit's tokens rather than a second set of values.
    function test_multilineFieldHoldsSeveralLinesOnTheKitsTokens() {
        const field = createTemporaryObject(multilineFieldComponent, testCase);
        verify(field !== null);
        compare(field.placeholderText, "one rule per line");
        compare(field.font.pixelSize, Style.font.body);
        compare(field.font.family, Style.font.family);
        // A rule per line, and the editor is tall enough to show a few of them
        // without the call site having to name a pixel height.
        compare(field.wrapMode, TextArea.NoWrap);
        verify(field.implicitHeight > Style.font.body * 4);
        field.text = "||ads.example.com^\n##.banner";
        compare(field.lineCount, 2);
    }

    // The kit ships no standalone checkbox, so `SettingCheckbox` lifts the one
    // `MultiSelect` draws as a popup-list delegate. A sync that restyles the
    // kit's checkbox has to be seen here rather than in a dialog nobody opens.
    function test_settingCheckboxKeepsTheKitsCheckboxShape() {
        const checkbox = createTemporaryObject(settingCheckboxComponent, testCase, {
                                                   width: 300
                                               });
        verify(checkbox !== null);
        const box = findChild(checkbox, "settingCheckboxBox");
        const tick = findChild(checkbox, "settingCheckboxTick");
        verify(box !== null);
        verify(tick !== null);

        compare(box.width, Style.space(16));
        compare(box.height, Style.space(16));
        compare(box.radius, Math.max(2, Style.cornerRadius / 2));
        compare(tick.text, "\u2713");

        // The two states are told apart by the box's own fill and border,
        // which is what a kit sync would restyle. Nothing here asserts
        // `visible`: the test case is not on screen, so every item in it
        // reports false whatever its binding says.
        const foreground = testCase.colorsFixture.text;
        const accent = testCase.colorsFixture.accent;
        compare(String(box.color), "#00000000");
        compare(String(Border.color(box.borderSpec)), String(Border.color(Border.controlSpec(
                                                                              "normal", foreground,
                                                                              accent))));

        checkbox.checked = true;
        compare(String(box.color), String(Style.selectedFillFor(foreground, accent)));
        compare(String(tick.color), String(Style.selectedStateColor(foreground, accent)));
        compare(String(Border.color(box.borderSpec)), String(Border.color(Border.controlSpec(
                                                                              "selected", foreground,
                                                                              accent))));
    }

    // Stateless about the value, as the kit's Toggle is. `Space` ticks it and
    // `Return` does not: a form of these sits in a dialog whose Return confirms
    // the whole form.
    function test_settingCheckboxLeavesTheValueToTheCallSiteAndOnlyAnswersSpace() {
        let clicks = 0;
        const checkbox = createTemporaryObject(settingCheckboxComponent, testCase, {
                                                   width: 300
                                               });
        checkbox.clicked.connect(function () {
            clicks += 1;
        });
        checkbox.forceActiveFocus();
        verify(checkbox.activeFocus);
        keyClick(Qt.Key_Space);
        compare(clicks, 1);
        compare(checkbox.checked, false);
        keyClick(Qt.Key_Return);
        compare(clicks, 1);
    }

    // One control per contract, and the two say so to a screen reader as well
    // as on the screen: a switch changed the browser, a checkbox is an argument
    // to something that has not happened yet (ADR 0031).
    function test_aSettingIsASwitchAndASelectionIsACheckbox() {
        const setting = createTemporaryObject(settingToggleComponent, testCase);
        const selection = createTemporaryObject(settingCheckboxComponent, testCase);
        // QAccessible::Switch is 0x87. Naming the number rather than the
        // enumerator is the point: if the QML attached type ever stops
        // exposing `Accessible.Switch`, an assertion against `undefined` would
        // pass against `undefined`.
        compare(setting.Accessible.role, 0x87);
        compare(selection.Accessible.role, Accessible.CheckBox);
        verify(setting.Accessible.role !== selection.Accessible.role);
    }

    // The kit's Toggle is stateless about the value: it reports the click and
    // the call site flips the model. A settings row that flipped `checked`
    // itself would double-toggle here.
    function test_toggleLeavesTheValueToTheCallSite() {
        let clicks = 0;
        const toggle = createTemporaryObject(toggleComponent, testCase);
        verify(toggle !== null);
        toggle.clicked.connect(function () {
            clicks += 1;
        });
        compare(toggle.checked, false);
        toggle.forceActiveFocus();
        verify(toggle.activeFocus);
        keyClick(Qt.Key_Space);
        compare(clicks, 1);
        compare(toggle.checked, false);
    }
}
