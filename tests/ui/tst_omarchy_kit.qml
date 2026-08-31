import QtQuick
import QtTest
import qs.Commons
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
        accent: "#9b87ff",
        privateAccent: "#dc6bce",
        border: "#4a4658",
        surfaceHover: "#3d394e",
        windowOpaque: "#16151d"
    })
    property var fonts: ({ family: "Menlo", size: 12 })

    Component {
        id: actionButtonComponent

        Tanto.ActionButton {
            colors: testCase.colorsFixture
            typography: testCase.fonts
            label: "Add subscription"
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
}
