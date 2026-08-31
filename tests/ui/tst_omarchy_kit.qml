import QtQuick
import QtQuick.Controls
import QtTest
import qs.Commons
import qs.Ui as Omarchy
import "../../src/ui" as Tanto

// The vendored Omarchy kit reaches Tanto through three seams: the Quickshell
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

        Tanto.ActionButton {
            colors: testCase.colorsFixture
            label: "Add subscription"
        }
    }

    Component {
        id: chromeButtonComponent

        Tanto.ChromeButton {
            foreground: testCase.colorsFixture.mutedText
            accent: testCase.colorsFixture.accent
            icon: "settings"
            accessibleName: "Browsing settings and downloads"
        }
    }

    Component {
        id: sectionLabelComponent

        Tanto.SectionLabel {
            colors: testCase.colorsFixture
            text: "pinned"
        }
    }

    Component {
        id: settingFieldComponent

        Tanto.SettingField {
            colors: testCase.colorsFixture
            placeholder: "list name"
            accessibleName: "Subscription name"
        }
    }

    Component {
        id: multilineFieldComponent

        Tanto.MultilineField {
            colors: testCase.colorsFixture
            placeholder: "one rule per line"
            accessibleName: "User rules"
        }
    }

    Component {
        id: toggleComponent

        Omarchy.Toggle {
            foreground: testCase.colorsFixture.text
            accent: testCase.colorsFixture.accent
            label: "Keyboard navigation"
            description: "Tanto's own command layer."
        }
    }

    // `Style` and `Color` are Quickshell-backed singletons; a missing shim type
    // takes the whole kit down, and the failure surfaces here as a token that
    // never resolved.
    function test_commonsTokensResolve() {
        verify(Style.font.body > 0)
        verify(Style.spacing.controlPaddingX >= 0)
        verify(Color.foreground.a > 0)
        verify(Border.none() !== undefined)
    }

    // Tanto's own Typography object is gone: the kit's scale is the only type
    // scale, so every size a Tanto surface asks for has to exist on it.
    function test_theKitScaleCoversEverySizeTantoAsksFor() {
        verify(Style.font.caption > 0)
        verify(Style.font.body >= Style.font.caption)
        verify(Style.font.title >= Style.font.body)
        verify(Style.font.heading >= Style.font.title)
        verify(Style.font.iconLarge > 0)
        verify(Style.font.family.length > 0)
    }

    function test_actionButtonUsesTheKitAndTantoPalette() {
        const button = createTemporaryObject(actionButtonComponent, testCase)
        verify(button !== null)
        // Sized by the kit's padding and the label, not by Tanto's old fixed
        // 30px box.
        verify(button.implicitWidth > 0)
        verify(button.implicitHeight > 0)
        compare(button.text, "Add subscription")
        compare(String(button.foreground), "#f3f1fa")
        compare(String(button.accent), "#9b87ff")
        // Type comes from the kit's scale now, not a per-instance block.
        compare(button.fontSize, Style.font.body)
        compare(button.fontFamily, Style.font.family)
    }

    function test_destructiveActionButtonTakesThePrivateAccent() {
        const button = createTemporaryObject(actionButtonComponent, testCase,
            { destructive: true })
        compare(String(button.accent), "#dc6bce")
    }

    function test_actionButtonClickReachesTheCallSite() {
        let clicks = 0
        const button = createTemporaryObject(actionButtonComponent, testCase)
        button.clicked.connect(function() { clicks += 1 })
        // Tanto is keyboard-driven, and the activation keys are the kit's.
        button.forceActiveFocus()
        verify(button.activeFocus)
        keyClick(Qt.Key_Return)
        keyClick(Qt.Key_Space)
        compare(clicks, 2)
    }

    function test_chromeButtonIsTheKitsBorderlessButton() {
        const button = createTemporaryObject(chromeButtonComponent, testCase)
        verify(button !== null)
        // Chrome is borderless and transparent at rest; the kit paints hover,
        // focus and pressed fills itself, which is why Tanto's
        // `hoverBackground` no longer exists.
        compare(button.bordered, false)
        verify(button.hoverBackground === undefined)
        compare(String(button.foreground), "#8d88a3")
        compare(String(button.accent), "#9b87ff")
        // A glyph is sized by the icon token, not by the body text token.
        compare(button.icon, "settings")
        compare(button.iconSize, Style.font.icon)
    }

    function test_chromeButtonClickReachesTheCallSite() {
        let clicks = 0
        const button = createTemporaryObject(chromeButtonComponent, testCase)
        button.clicked.connect(function() { clicks += 1 })
        button.forceActiveFocus()
        verify(button.activeFocus)
        keyClick(Qt.Key_Return)
        keyClick(Qt.Key_Space)
        compare(clicks, 2)
    }

    function test_sectionLabelIsTheKitsPanelSectionHeader() {
        const label = createTemporaryObject(sectionLabelComponent, testCase)
        verify(label !== null)
        compare(label.fontSize, Style.font.caption)
        compare(String(label.foreground), "#f3f1fa")
        // The kit derives its own muting from `foreground` rather than reading
        // a separate muted role, and it neither letter-spaces nor upper-cases.
        verify(String(label.color) !== String(label.foreground))
        compare(label.font.capitalization, Font.MixedCase)
        compare(label.font.letterSpacing, 0)
        compare(label.text, "pinned")
    }

    // A Qt Quick Controls native style refuses the kit's replaced `background`
    // and paints its own instead, so the shim pins the style the kit draws on.
    // Without it the settings fields silently render as platform boxes, and
    // nothing else in the suite notices.
    function test_theControlsStyleLetsTheKitPaintItsOwnBackground() {
        compare(controlsStyle, "Basic")
    }

    function test_settingFieldIsTheKitsTextField() {
        const field = createTemporaryObject(settingFieldComponent, testCase)
        verify(field !== null)
        compare(field.placeholderText, "list name")
        compare(String(field.foreground), "#f3f1fa")
        compare(String(field.accent), "#9b87ff")
        compare(field.font.pixelSize, Style.font.body)

        // The dialogs drive the field by name, so the two calls they make have
        // to survive the swap.
        field.text = "EasyList"
        field.focusInput()
        verify(field.activeFocus)
        field.selectAllText()
        compare(field.selectedText, "EasyList")
    }

    function test_destructiveSettingFieldTakesThePrivateAccent() {
        const field = createTemporaryObject(settingFieldComponent, testCase,
            { destructive: true })
        compare(String(field.accent), "#dc6bce")
    }

    // The kit has no text area, so the rules editor is Tanto's own control —
    // but it reads the kit's tokens rather than a second set of values.
    function test_multilineFieldHoldsSeveralLinesOnTheKitsTokens() {
        const field = createTemporaryObject(multilineFieldComponent, testCase)
        verify(field !== null)
        compare(field.placeholderText, "one rule per line")
        compare(field.font.pixelSize, Style.font.body)
        compare(field.font.family, Style.font.family)
        // A rule per line, and the editor is tall enough to show a few of them
        // without the call site having to name a pixel height.
        compare(field.wrapMode, TextArea.NoWrap)
        verify(field.implicitHeight > Style.font.body * 4)
        field.text = "||ads.example.com^\n##.banner"
        compare(field.lineCount, 2)
    }

    // The kit's Toggle is stateless about the value: it reports the click and
    // the call site flips the model. A settings row that flipped `checked`
    // itself would double-toggle here.
    function test_toggleLeavesTheValueToTheCallSite() {
        let clicks = 0
        const toggle = createTemporaryObject(toggleComponent, testCase)
        verify(toggle !== null)
        toggle.clicked.connect(function() { clicks += 1 })
        compare(toggle.checked, false)
        toggle.forceActiveFocus()
        verify(toggle.activeFocus)
        keyClick(Qt.Key_Space)
        compare(clicks, 1)
        compare(toggle.checked, false)
    }
}
