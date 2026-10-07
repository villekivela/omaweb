import QtQuick
import QtQuick.Window
import QtTest
import "../../src/ui" as Omaweb

// Hyperspace under the Start page, as a Scene: how its stars drift at rest and
// stretch into streaks on a commit for the clock and the reader the host hands
// it, how long the streaks hold, and what a Private window's field and a still
// one show.
TestCase {
    id: testCase
    name: "Hyperspace"
    when: windowShown
    width: 800
    height: 500

    Component {
        id: fieldComponent

        Omaweb.Hyperspace {
            width: 400
            height: 250
            colors: crtRoadKeyColours.dark.theme
        }
    }

    // The host in a window of its own on screen, so what it shows is drawn.
    Component {
        id: hostComponent

        Window {
            property alias host: host

            width: testCase.width
            height: testCase.height
            visible: true

            Omaweb.SceneHost {
                id: host
                anchors.fill: parent
                colors: crtRoadKeyColours.dark.theme
                scene: Component {
                    Omaweb.Hyperspace {}
                }
            }
        }
    }

    function makeHost(properties) {
        const window = createTemporaryObject(hostComponent, testCase);
        verify(window !== null);
        const host = window.host;
        for (const name in properties || {})
        host[name] = properties[name];
        tryVerify(function () {
            return host.sceneItem !== null;
        });
        return host;
    }

    function makeField(properties) {
        const field = createTemporaryObject(fieldComponent, testCase, properties || {});
        verify(field !== null);
        return field;
    }

    // Drives the field's clock by hand, a frame at the Scene's rate at a time.
    function drive(field, seconds) {
        const step = 1 / 30;
        for (let elapsed = 0; elapsed < seconds; elapsed += step)
            field.time += step;
    }

    function lightness(colour) {
        const c = Qt.color(colour);
        return (Math.max(c.r, c.g, c.b) + Math.min(c.r, c.g, c.b)) / 2;
    }

    function distance(from, to) {
        return Math.hypot(to.x - from.x, to.y - from.y);
    }

    // Every star as the field draws it now.
    function pose(field) {
        const stars = [];
        for (let index = 0; index < field.stars; ++index)
            stars.push(field.star(index));
        return JSON.stringify(stars);
    }

    // The stars that show on the page area now: lit, and with their heads on
    // it.
    function shown(field) {
        const out = [];
        for (let index = 0; index < field.stars; ++index) {
            const star = field.star(index);
            if (star.alpha > 0.05 && star.head.x >= 0 && star.head.x <= field.drawWidth && star.head.y
                    >= 0 && star.head.y <= field.drawHeight)
                out.push(star);
        }
        return out;
    }

    // How far a star's tail runs from the ray its head stands on, from the
    // centre.
    function offRay(field, star) {
        const c = field.centre;
        const hx = star.head.x - c.x;
        const hy = star.head.y - c.y;
        const tx = star.tail.x - c.x;
        const ty = star.tail.y - c.y;
        return Math.abs(hx * ty - hy * tx) / Math.max(1e-6, Math.hypot(hx, hy));
    }

    // Hyperspace is a Scene the host shows as it shows the road: in its own
    // pixels, through the CRT glass it declares, with the Omnibar resting on
    // the page area's middle and the stars streaming from behind its field.
    // It casts no light on the Omnibar's rim.
    function test_theHostShowsHyperspaceAsAScene() {
        const host = makeHost();
        const field = host.sceneItem;
        compare(field.objectName, "hyperspace");
        compare(field.parameters.id, "hyperspace");
        tryCompare(field, "width", 800 / field.pitch);
        compare(field.horizonY, 250);
        compare(field.centre, Qt.point(400, 250));
        compare(field.glass, "crt");
        compare(field.crt, field.parameters.crt);
        verify(findChild(host, "crtGlass").visible);
        compare(host.light, null);
    }

    // The field is drawn from the theme's palette and is always night: a
    // light theme's night is drawn from its dark text, so space stays dark,
    // the stars light and the nebula the accent, and a theme change follows.
    function test_spaceIsAlwaysNight_data() {
        return [
                    {
                        tag: "dark",
                        theme: "dark",
                        dark: true
                    },
                    {
                        tag: "light",
                        theme: "light",
                        dark: false
                    }
                ];
    }

    function test_spaceIsAlwaysNight(data) {
        const field = makeField({
                                    colors: crtRoadKeyColours[data.theme].theme,
                                    dark: data.dark
                                });
        verify(lightness(field.roles.deep) < 0.2, field.roles.deep);
        verify(lightness(field.roles.light) > 0.75, field.roles.light);
        verify(lightness(field.colour(field.parameters.stars.colour)) > 0.8);
        compare(field.roles.glow, Qt.color(crtRoadKeyColours[data.theme].theme.accent));

        const other = data.theme === "dark" ? "light" : "dark";
        field.colors = crtRoadKeyColours[other].theme;
        field.dark = !data.dark;
        compare(field.roles.glow, Qt.color(crtRoadKeyColours[other].theme.accent));
    }

    // At rest the stars drift slowly out from the centre, as the field comes
    // on toward the reader, and each is a star, not a streak.
    function test_theStarsDriftSlowlyOutFromTheCentre() {
        const field = makeField();
        drive(field, 1);
        compare(field.warp, 0);
        const before = [];
        for (let index = 0; index < field.stars; ++index)
            before.push(field.star(index));
        drive(field, 1);
        let outward = 0;
        let lit = 0;
        for (let index = 0; index < field.stars; ++index) {
            const star = field.star(index);
            const was = before[index];
            verify(distance(star.tail, star.head) < 0.5, "a streak " + JSON.stringify(star));
            if (star.alpha < 0.05 || was.alpha < 0.05)
                continue;
            lit += 1;
            const out = distance(field.centre, star.head) - distance(field.centre, was.head);
            if (out > 0)
                outward += 1;
            if (star.head.x >= 0 && star.head.x <= field.drawWidth && star.head.y >= 0 && star.head.y
                    <= field.drawHeight)
                verify(out < field.drawWidth * 0.15, "a star moved " + out + " in a second");
        }
        verify(lit > field.stars / 2, lit + " stars lit");
        compare(outward, lit);
    }

    // From a commit the stars stretch into streaks from the centre: each runs
    // back from the star toward the centre along its ray, long across the
    // page area, and the field rushes past. The streaks hold for as long as
    // the commit does.
    function test_aCommitStretchesTheStarsIntoStreaks() {
        const field = makeField();
        drive(field, 1);
        const resting = field.travel;
        drive(field, 1);
        const drift = field.travel - resting;

        field.navigating = 1;
        drive(field, 1);
        verify(field.warp > 0.95, field.warp);
        const start = field.travel;
        drive(field, 1);
        verify(field.travel - start > drift * 20, (field.travel - start) + " against " + drift);

        const streaks = shown(field);
        verify(streaks.length > 20, streaks.length + " streaks on the page area");
        let long = 0;
        for (const star of streaks) {
            verify(distance(field.centre, star.tail) < distance(field.centre, star.head),
                   "a tail farther out than its head " + JSON.stringify(star));
            verify(offRay(field, star) < 0.5, "a streak off its ray " + JSON.stringify(star));
            if (distance(star.tail, star.head) > field.drawWidth * 0.05)
                long += 1;
        }
        verify(long > streaks.length / 2, long + " of " + streaks.length + " streaks long");

        drive(field, 5);
        verify(field.warp > 0.999, field.warp);
    }

    // The page replaces the Scene with no collapse back to stars: while the
    // Start page fades out after a drive, the streaks hold, and once it has
    // gone the stars are back at once, for the next time it shows.
    function test_theStreaksHoldUntilTheStartPageHasGone() {
        const field = makeField({
                                    navigating: 1
                                });
        drive(field, 2);
        verify(field.warp > 0.99, field.warp);

        field.leaving = true;
        field.navigating = 0;
        drive(field, 1);
        verify(field.warp > 0.99, field.warp);

        field.leaving = false;
        compare(field.warp, 0);
        const resting = field.travel;
        drive(field, 1);
        verify(field.travel - resting < field.parameters.drift * 1.5, field.travel - resting);
        for (const star of shown(field))
            verify(distance(star.tail, star.head) < 0.5, "a streak " + JSON.stringify(star));
    }

    // A drive that ends with the Start page still on show eases back to
    // stars.
    function test_aDriveEndedInPlaceEasesBackToStars() {
        const field = makeField({
                                    navigating: 1
                                });
        drive(field, 2);
        field.navigating = 0;
        drive(field, 0.1);
        verify(field.warp > 0.3, field.warp);
        drive(field, 2);
        verify(field.warp < 0.01, field.warp);
    }

    // A reader who asked for less motion gets one frame that reads on its own:
    // the stars and the nebula, with no streaks, and nothing moves, even on
    // commit.
    function test_aStillFieldHoldsOneFrame() {
        const field = makeField({
                                    reducedMotion: true,
                                    navigating: 1
                                });
        verify(field.stars > 0);
        compare(field.nebulaStrength, 1);
        compare(field.warp, 0);
        compare(field.travel, field.parameters.still);
        verify(shown(field).length > field.stars / 2);
        const still = pose(field);
        drive(field, 30);
        compare(pose(field), still);
    }

    // A Private window's field has its lights out: the nebula alone, very
    // dim, with no stars, and no jump on commit.
    function test_anUnlitFieldIsTheNebulaOnly() {
        const lit = makeField();
        const field = makeField({
                                    unlit: true
                                });
        compare(field.stars, field.parameters.unlit.stars);
        compare(field.stars, 0);
        verify(field.nebulaStrength < 0.5, field.nebulaStrength);
        verify(lightness(field.roles.glow) > lightness(field.roles.deep),
               "a nebula as dark as space");
        verify(Math.abs(lightness(lit.roles.glow) - lightness(field.roles.glow)) > 0.05,
               "a nebula in the accent");
        const travel = field.travel;
        field.navigating = 1;
        drive(field, 3);
        compare(field.warp, 0);
        compare(field.travel, travel);
    }

    // What the host draws: the nebula, a faint cloud in the accent over the
    // night, brighter than the space around it; in a Private window, very
    // dim.
    function test_theNebulaGlowsOnTheNight() {
        const host = makeHost({
                                  glass: false,
                                  reducedMotion: true
                              });
        const field = host.sceneItem;
        tryCompare(field, "width", 800 / field.pitch);
        wait(100);
        const cloud = field.parameters.nebula[0];
        const read = function () {
            const image = grabImage(host);
            // The grab is in the display's pixels.
            const scale = image.width / host.width;
            const sample = function (x, y) {
                return Qt.color(image.pixel(Math.round(x * scale), Math.round(y * scale)));
            };
            // Neighbouring pixels, so a star on one does not decide it.
            const darkest = function (x, y) {
                let least = null;
                for (let dx = -6; dx <= 6; dx += 2)
                    for (let dy = -6; dy <= 6; dy += 2) {
                        const c = sample(x + dx, y + dy);
                        if (least === null || lightness(c) < lightness(least))
                            least = c;
                    }
                return least;
            };
            return {
                cloud: darkest(cloud.at[0] * 800, cloud.at[1] * 500),
                space: darkest(0.3 * 800, 0.2 * 500)
            };
        };
        const lit = read();
        verify(lightness(lit.cloud) > lightness(lit.space) + 0.02, "a nebula " + lit.cloud
               + " on space " + lit.space);
        // Toward the accent.
        const accent = Qt.color(crtRoadKeyColours.dark.theme.accent);
        const toward = function (c, from) {
            return (c.r - from.r) * (accent.r - from.r) + (c.g - from.g) * (accent.g - from.g) + (
                        c.b - from.b) * (accent.b - from.b);
        };
        verify(toward(lit.cloud, lit.space) > 0, "a nebula away from the accent");

        host.unlit = true;
        wait(100);
        const unlit = read();
        verify(lightness(unlit.cloud) > lightness(unlit.space), "no nebula in a Private window");
        // Very dim: less than half the lit nebula's contrast with space.
        verify(lightness(unlit.cloud) - lightness(unlit.space) < (lightness(lit.cloud) - lightness(
                                                                      lit.space)) / 2,
               "a nebula as bright in a Private window");
    }
}
