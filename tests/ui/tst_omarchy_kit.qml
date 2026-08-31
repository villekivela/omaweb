import QtQuick
import QtTest
import qs.Commons
import qs.Ui as Omarchy

// The vendored Omarchy kit reaches Tanto through two seams: the Quickshell shim
// its singletons import, and the `qs.Commons` tokens its components read. Both
// are cheap to break during a sync, so both are asserted here rather than left
// to whichever surface adopts a component next.
TestCase {
    id: testCase
    name: "OmarchyKit"
    when: windowShown
    width: 200
    height: 60

    property var fonts: ({ family: "Menlo", size: 12 })

    Component {
        id: buttonComponent

        Omarchy.Button {
            text: "Add subscription"
            focusable: true
            bordered: true
            foreground: "#f3f1fa"
            accent: "#9b87ff"
            fontFamily: testCase.fonts.family
            fontSize: testCase.fonts.size
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

    function test_componentsPaintFromCallerSuppliedColors() {
        const button = createTemporaryObject(buttonComponent, testCase)
        verify(button !== null)
        // Sized by the kit's padding tokens and the label.
        verify(button.implicitWidth > 0)
        verify(button.implicitHeight > 0)
        // Tanto passes colour per instance so ThemeController stays the source
        // of truth for the palette.
        compare(String(button.foreground), "#f3f1fa")
        compare(String(button.accent), "#9b87ff")
    }

    function test_keyboardActivationReachesTheCallSite() {
        let clicks = 0
        const button = createTemporaryObject(buttonComponent, testCase)
        button.clicked.connect(function() { clicks += 1 })
        // Tanto is keyboard-driven, and the activation keys are the kit's.
        button.forceActiveFocus()
        verify(button.activeFocus)
        keyClick(Qt.Key_Return)
        keyClick(Qt.Key_Space)
        compare(clicks, 2)
    }
}
