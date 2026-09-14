import QtQuick
import QtQuick.Controls
import QtTest
import "../../src/ui" as Omaweb

// The chrome's scrollbar, which is an overlay: it draws nothing until the list
// moves or the pointer arrives, and every colour it does draw comes from the
// window's palette rather than from Qt's.
//
// The palette is the half worth pinning. Qt's Basic scrollbar reads
// `palette.mid` and `palette.dark` from a `QPalette` Omaweb never sets, so a
// bar that quietly falls back to the stock painting looks right on a dark theme
// and wrong everywhere else — a regression that shows up as a grey sliver
// nobody files. Asserting against two palettes is what catches it: one
// hard-coded colour can satisfy a single fixture.
TestCase {
    id: testCase
    name: "ChromeScrollBar"
    when: windowShown

    readonly property var colorsFixture: ({
                                              accent: "#9b87ff",
                                              border: "#4a4658",
                                              mutedText: "#8d88a3",
                                              separator: "#4a4658",
                                              surface: "#26232f"
                                          })

    // A second theme sharing no colour with the first, so a value written down
    // rather than read cannot pass both.
    readonly property var otherColorsFixture: ({
                                                   accent: "#1d6f42",
                                                   border: "#c9c4b8",
                                                   mutedText: "#7a7566",
                                                   separator: "#ded9cd",
                                                   surface: "#f4f1e8"
                                               })

    Component {
        id: barComponent

        Omaweb.ChromeScrollBar {
            colors: testCase.colorsFixture
            orientation: Qt.Vertical
            height: 400
            size: 0.25
        }
    }

    // A real scrolling region, because the bar's geometry is only meaningful
    // once a view has placed it: content taller than the view, so the bar has
    // something to represent.
    Component {
        id: viewComponent

        ScrollView {
            id: probeView

            width: 400
            height: 600
            contentWidth: availableWidth

            ScrollBar.vertical: Omaweb.ChromeScrollBar {
                view: probeView
                colors: testCase.colorsFixture
            }

            Column {
                width: 400

                Repeater {
                    model: 60

                    Rectangle {
                        width: 400
                        height: 40
                        color: "#202020"
                    }
                }
            }
        }
    }

    property var liveBar: null

    function cleanup() {
        if (liveBar !== null) {
            liveBar.destroy();
            liveBar = null;
        }
    }

    function makeBar() {
        liveBar = barComponent.createObject(testCase);
        verify(liveBar !== null);
        return liveBar;
    }

    // The thumb is the child that carries the colour; the gutter around it is
    // deliberately unpainted, so it is reached rather than assumed.
    function thumbOf(bar) {
        verify(bar.contentItem !== null);
        compare(bar.contentItem.children.length, 1);
        return bar.contentItem.children[0];
    }

    // The gutter is the pointer target and is wider than the thumb it rests, so
    // the reader aims at the edge of the list rather than at a six-pixel line.
    function test_theGutterIsWiderThanTheThumbItRests() {
        const bar = makeBar();
        verify(bar.implicitWidth > bar.restingThumb);
        verify(bar.hoveredThumb > bar.restingThumb);
        verify(bar.implicitWidth >= bar.hoveredThumb);
    }

    // Idle draws nothing at all. An overlay that rested visible would be a
    // permanent stripe over every list in the chrome.
    function test_nothingIsDrawnWhileTheListIsStill() {
        const bar = makeBar();
        compare(bar.active, false);
        compare(bar.opacity, 0);
    }

    // Scrolling shows the bar, and the track stays away: a bare thumb says how
    // far down the list stands without drawing a second shape over it.
    function test_scrollingShowsTheThumbWithoutTheTrack() {
        const bar = makeBar();
        bar.active = true;
        compare(bar.opacity, 1);
        compare(bar.expanded, false);
        compare(bar.trackOpacity, 0);
        compare(bar.thumbThickness, bar.restingThumb);
    }

    // Every colour is read from the window's palette, so a theme change and a
    // Private window both reach the bar without it knowing either exists.
    //
    // Waited for rather than read: the thumb crossfades between states, so the
    // colour a theme change asks for arrives over the Behavior's duration
    // rather than on the assignment, and sampling it straight away reads a
    // blend of the two themes — neither of the values worth asserting.
    function test_theThumbTakesItsColoursFromThePalette() {
        const bar = makeBar();
        const thumb = thumbOf(bar);

        tryCompare(thumb, "color", testCase.colorsFixture.border);

        bar.colors = testCase.otherColorsFixture;
        tryCompare(thumb, "color", testCase.otherColorsFixture.border);
    }

    // The track is the same story, down to the hairline that divides it from
    // the list it overlays.
    function test_theTrackTakesItsColoursFromThePalette() {
        const bar = makeBar();
        const track = bar.background;
        verify(track !== null);
        compare(track.children.length, 1);
        const hairline = track.children[0];

        compare(String(track.color), testCase.colorsFixture.surface);
        compare(String(hairline.color), testCase.colorsFixture.separator);

        bar.colors = testCase.otherColorsFixture;
        compare(String(track.color), testCase.otherColorsFixture.surface);
        compare(String(hairline.color), testCase.otherColorsFixture.separator);
    }

    // The hairline divides the bar from the list, so it runs along the inner
    // edge rather than around the track or along the window's own edge.
    function test_theHairlineIsAnEdgeRatherThanABorder() {
        const bar = makeBar();
        const hairline = bar.background.children[0];
        compare(hairline.width, 1);
        compare(hairline.height, bar.background.height);
    }

    // A long list still has to offer something catchable: proportional sizing
    // alone leaves a few pixels of thumb on a list of any depth. Capped as well
    // as floored: before the view has laid the bar out there is no length to
    // take a ratio of, and a ratio above 1 asks for a thumb longer than the
    // track it runs in.
    function test_theThumbKeepsALengthWorthGrabbing() {
        const bar = makeBar();
        bar.size = 0.001;
        verify(bar.minimumSize > 0);
        verify(bar.minimumSize <= 0.5);
    }

    // Qt's `ScrollView` does not lay its scrollbars out. The parent, the
    // position and the length are bindings inside the default `ScrollBar` its
    // style file declares, so replacing `ScrollBar.vertical` drops them along
    // with the painting and leaves the bar at the view's top corner at its
    // implicit size.
    //
    // Asserted against a real view for that reason: a bar built on its own is
    // unlaid whether or not it carries the bindings, so a fixture that skips
    // the view cannot tell working from broken.
    function test_theBarIsLaidOutDownTheEdgeOfItsView() {
        const view = viewComponent.createObject(testCase);
        verify(view !== null);
        const bar = view.ScrollBar.vertical;
        verify(bar !== null);

        tryCompare(bar, "height", view.availableHeight);
        compare(bar.parent, view);
        compare(bar.y, view.topPadding);
        // Down the trailing edge, fully inside the view.
        compare(bar.x, view.width - bar.width);
        verify(bar.width > 0);
        verify(view.availableHeight > bar.width);

        view.destroy();
    }
}
