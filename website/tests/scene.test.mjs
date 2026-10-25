// What the Scene host hands a Scene and when it lets one draw, through `scene.js`'s exports. The
// host itself needs a browser; these are the decisions it makes, which do not.

import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import test from "node:test";

import {
  STILL_MEDIA,
  SceneLight,
  drawsFrames,
  frameDue,
  glassLayers,
  glassRoll,
  grade,
  gradeHue,
  halation,
  sceneInput,
} from "../scene.js";

const road = { id: "road", pitch: 4 };
const retro82 = { ground: "#020c17", text: "#f6dcac", accent: "#faa968", muted: "#3f8f8a" };

test("input: the theme's roles arrive as RGB, and a dark ground is dark", () => {
  const input = sceneInput(road, { palette: retro82, width: 1440, height: 900 });
  assert.deepEqual(input.palette, {
    ground: [2, 12, 23],
    text: [246, 220, 172],
    accent: [250, 169, 104],
    muted: [63, 143, 138],
  });
  assert.equal(input.dark, true);
});

// Omaweb hands a page that asks the reader's palette as `rgb()` (EngineView.qml), and a browser
// can report a colour in either comma or space syntax; the landing page's themes are hex.
test("input: a role given as rgb() arrives as the same RGB as its hex", () => {
  const given = { ...retro82, ground: "rgb(2 12 23)", text: " rgb(246, 220, 172)" };
  const input = sceneInput(road, { palette: given, width: 10, height: 10 });
  assert.deepEqual(
    [input.palette.ground, input.palette.text],
    [
      [2, 12, 23],
      [246, 220, 172],
    ],
  );
});

test("input: a light theme's ground is not dark", () => {
  const latte = { ground: "#eff1f5", text: "#4c4f69", accent: "#1e66f5", muted: "#9ca0b0" };
  assert.equal(sceneInput(road, { palette: latte, width: 10, height: 10 }).dark, false);
});

test("input: the display is the area in the Scene's pixels, rounded up", () => {
  const input = sceneInput(road, { palette: retro82, width: 1441, height: 900 });
  assert.deepEqual([input.width, input.height, input.pitch], [361, 225, 4]);
});

test("input: each declared option is the reader's choice, or else its first value", () => {
  const scene = { ...road, options: { bands: ["4", "3"], road: ["widest", "wider", "wide"] } };
  const base = { palette: retro82, width: 10, height: 10 };
  assert.deepEqual(sceneInput(scene, base).options, { bands: "4", road: "widest" });
  assert.deepEqual(sceneInput(scene, { ...base, chosen: { bands: "3", road: "narrow" } }).options, {
    bands: "3",
    road: "widest",
  });
});

test("input: moving, the Scene gets the time and how hard the reader navigates, 0 to 1", () => {
  const input = sceneInput(road, {
    palette: retro82,
    width: 10,
    height: 10,
    time: 12.5,
    navigating: 3,
  });
  assert.deepEqual([input.time, input.navigating, input.reducedMotion], [12.5, 1, false]);
});

test("input: with reduced motion or on a phone the Scene holds still and nobody navigates", () => {
  for (const still of [{ reducedMotion: true }, { phone: true }]) {
    const environment = { palette: retro82, width: 10, height: 10, navigating: 0.8, ...still };
    const input = sceneInput(road, environment);
    assert.deepEqual([input.reducedMotion, input.navigating], [true, 0]);
  }
});

test("input: the radio's beat reaches a moving Scene, 0 to 1, and a still one gets none", () => {
  const moving = { palette: retro82, width: 10, height: 10 };
  assert.equal(sceneInput(road, { ...moving, beat: 0.4 }).beat, 0.4);
  assert.equal(sceneInput(road, { ...moving, beat: 2 }).beat, 1);
  assert.equal(sceneInput(road, moving).beat, 0);
  assert.equal(sceneInput(road, { ...moving, beat: 0.4, reducedMotion: true }).beat, 0);
});

test("frames: only while on screen, in a shown tab, in the focused window, and moving", () => {
  const moving = { onScreen: true, pageHidden: false, focused: true };
  assert.equal(drawsFrames(moving), true);
  for (const change of [
    { onScreen: false },
    { pageHidden: true },
    { focused: false },
    { reducedMotion: true },
    { phone: true },
  ]) {
    assert.equal(drawsFrames({ ...moving, ...change }), false, JSON.stringify(change));
  }
});

test("frames: a Scene capped at 30 fps draws on every other 60 Hz tick", () => {
  const ticks = [0, 16.7, 33.3, 50, 66.7, 83.3, 100];
  let last = -Infinity;
  const drawn = ticks.filter((now) => {
    if (!frameDue(now, last, 30)) return false;
    last = now;
    return true;
  });
  assert.deepEqual(drawn, [0, 33.3, 66.7, 100]);
});

test("frames: a Scene with no cap draws on every tick", () => {
  assert.equal(frameDue(16.7, 0, undefined), true);
});

// The road holds still and the feature cards become a plain list on the same screens: the host's
// query and the stylesheet's fallback are one string, so they cannot drift apart.
test("fallback: the features read as a list exactly where the road holds still", () => {
  const styles = readFileSync(new URL("../styles.css", import.meta.url), "utf8");
  assert.equal(STILL_MEDIA, "(max-width: 860px), (prefers-reduced-motion: reduce)");
  const block = styles.split(`@media ${STILL_MEDIA} {`)[1];
  assert.ok(block, "styles.css has no fallback block for the still media");
  assert.match(block.split(/\n}\n/)[0], /\.cards\b/);
});

// A release page shows the road as a still header: its host holds the Scene still whatever the
// reader's settings, and draws the one frame it lays out with.
test("input: a host held still tells the Scene to hold still and draws no frames", () => {
  const environment = { palette: retro82, width: 10, height: 10, navigating: 1, held: true };
  assert.equal(sceneInput(road, environment).reducedMotion, true);
  assert.equal(drawsFrames({ ...environment, onScreen: true, focused: true }), false);
});

// The CRT glass draws by the shared file's `crt` block, as the browser's glass does (#496): change
// an amount there and the glass changes with it.
const sharedCrt = (id) =>
  JSON.parse(readFileSync(new URL(`../../share/scenes/${id}.json`, import.meta.url), "utf8")).crt;

// The alpha of each stop in a gradient of `rgb(0 0 0 / a%) at` stops, and where the last one ends.
function stops(gradient) {
  const alphas = [...gradient.matchAll(/rgb\(0 0 0 \/ ([\d.]+)%\)/g)].map((m) => Number(m[1]));
  const ends = [...gradient.matchAll(/([\d.]+)px/g)].map((m) => Number(m[1]));
  return { alphas, period: ends[ends.length - 1] };
}

test("glass: its layers take their amounts from the shared file", () => {
  const crt = sharedCrt("crt-road");
  const layers = glassLayers(crt, { pitch: 4, ratio: 1 });
  assert.equal(layers.bloom, "0.22");
  assert.equal(
    layers.vignette,
    "radial-gradient(ellipse at center, transparent 55%, rgb(0 0 0 / 55%) 100%)",
  );
  // The beam's lift and the mask's, which the picture is brightened by before they darken it.
  assert.equal(layers.lift, "brightness(1.403)");
  assert.deepEqual(layers.halo, { scale: 8, blur: "24px", opacity: "0.25" });
  const changed = glassLayers(
    {
      ...crt,
      bloom: { scale: 3, opacity: 0.3 },
      scanlines: { ...crt.scanlines, lift: 1 },
      mask: { dim: 0.5, lift: 1.5 },
      halation: { threshold: 0.5, strength: 0.4, reach: 16, spacing: 12 },
      vignette: { clear: 0.6, shade: 0.4 },
    },
    { pitch: 4, ratio: 1 },
  );
  assert.equal(changed.bloom, "0.3");
  assert.equal(
    changed.vignette,
    "radial-gradient(ellipse at center, transparent 60%, rgb(0 0 0 / 40%) 100%)",
  );
  assert.equal(changed.lift, "brightness(1.5)");
  assert.deepEqual(changed.halo, { scale: 4, blur: "12px", opacity: "0.4" });
  assert.match(changed.mask, /rgb\(255 128 128\)/);
});

// A tube draws one pass of its beam to each row of the picture, so the lines stand as far apart as
// the Scene's pixels are tall, darkest at the edges between rows and clear at their middles.
test("glass: a raster Scene's lines stand one to each of its rows", () => {
  const crt = sharedCrt("crt-road");
  for (const pitch of [4, 3]) {
    const { alphas, period } = stops(glassLayers(crt, { pitch, ratio: 1 }).scan);
    assert.equal(period, pitch);
    assert.equal(Math.min(...alphas), alphas[Math.floor(alphas.length / 2)]);
    assert.equal(Math.max(...alphas), alphas[0]);
    assert.equal(alphas[0], alphas[alphas.length - 1]);
  }
});

// The mask is a tube's, whatever the Scene's pitch: one red, one green and one blue stripe, each
// one device pixel wide.
test("glass: the mask's stripes are one device pixel wide", () => {
  const crt = sharedCrt("crt-road");
  assert.equal(
    glassLayers(crt, { pitch: 4, ratio: 2 }).mask,
    "repeating-linear-gradient(90deg, rgb(255 184 184) 0 0.5px, rgb(184 255 184) 0.5px 1px, rgb(184 184 255) 1px 1.5px)",
  );
  assert.match(
    glassLayers(crt, { pitch: 4, ratio: 1 }).mask,
    /1px 2px, rgb\(184 184 255\) 2px 3px\)$/,
  );
});

// The halation counts only what is brighter than the threshold, rescaled to 0 to 1.
test("glass: the halation counts only what is brighter than its threshold", () => {
  const crt = sharedCrt("crt-road");
  assert.equal(halation(0.35, crt), 0);
  assert.equal(halation(0.2, crt), 0);
  assert.equal(halation(1, crt), 1);
  assert.ok(Math.abs(halation(0.675, crt) - 0.5) < 1e-9);
});

// The grade's second hue is the theme's keyword colour, as the browser's (src/ui/SceneHost.qml).
test("grade: the second hue is the theme's keyword colour", () => {
  assert.deepEqual(
    gradeHue({ accent: [155, 135, 255], keyword: [198, 120, 221] }),
    [198, 120, 221],
  );
});

// Where the keyword colour would add nothing, or the page has none, as a release page read in
// another browser, the hue is the accent's turned half way round at the tint: blue's opposite,
// yellow, at half saturation and half lightness.
test("grade: without a keyword colour that adds anything, the accent's opposite", () => {
  const tint = { saturation: 0.5, lightness: 0.5 };
  for (const keyword of [[77, 51, 255], [140, 140, 115], undefined]) {
    assert.deepEqual(gradeHue({ accent: [0, 0, 255], keyword }, tint), [191, 191, 64]);
  }
});

// The bright parts take the hue at their own brightness, more of them lower in the frame. White
// above the sunset's top stays white; near the bottom it is past both the highlights' and the
// midtones' range, so it goes 0.7 of the way to red at white's brightness, which clips to full red
// and leaves 0.3 of the green and blue. A dark part keeps its colour.
test("grade: the bright parts take the hue, more of them lower in the frame", () => {
  const crt = sharedCrt("crt-road");
  const red = [255, 0, 0];
  assert.deepEqual(grade([1, 1, 1], 0.05, crt, red), [1, 1, 1]);
  const low = grade([1, 1, 1], 0.95, crt, red);
  assert.deepEqual(
    low.map((channel) => Math.round(channel * 1000) / 1000),
    [1, 0.3, 0.3],
  );
  assert.deepEqual(grade([0.05, 0.05, 0.05], 0.95, crt, red), [0.05, 0.05, 0.05]);
});

// A vector screen traced its lines with the beam and had no raster: no scanlines, no mask and no
// refresh band, and nothing to lift back. Its flicker stays.
test("glass: a vector Scene has no scanlines, mask or band", () => {
  const crt = sharedCrt("radar");
  const layers = glassLayers(crt, { pitch: 2, ratio: 1, vector: true });
  assert.equal(layers.scan, "none");
  assert.equal(layers.mask, "none");
  assert.equal(layers.lift, "none");
  const roll = glassRoll(crt, { time: 1, height: 100 });
  assert.equal(roll.strength, 0);
  assert.ok(roll.flicker >= 0.02 && roll.flicker <= 0.045);
});

test("glass: the band and the flicker take theirs from the shared file", () => {
  const crt = JSON.parse(
    readFileSync(new URL("../../share/scenes/crt-road.json", import.meta.url), "utf8"),
  ).crt;
  // A third of the way through the band's seven seconds, on a picture 100 pixels tall.
  const roll = glassRoll(crt, { time: 7 / 3, height: 100 });
  assert.ok(Math.abs(roll.y - (100 * 1.4) / 3 + 20) < 1e-9);
  assert.ok(Math.abs(roll.reach - 7) < 1e-9);
  assert.equal(roll.strength, 0.05);
  assert.ok(roll.flicker >= 0.02 && roll.flicker <= 0.045);
  const slower = glassRoll(
    {
      ...crt,
      band: { ...crt.band, every: 14, strength: 0.1 },
      flicker: { ...crt.flicker, least: 0.1 },
    },
    { time: 7 / 3, height: 100 },
  );
  assert.ok(Math.abs(slower.y - (100 * 1.4) / 6 + 20) < 1e-9);
  assert.equal(slower.strength, 0.1);
  assert.ok(slower.flicker >= 0.1);
});

// The Scene's light reaches one element on the page, the Start page's Omnibar, through its style:
// thirty frames a second, so it writes only what changed, and nothing while the element is off
// screen; back on screen, it catches up at once.
test("light: the lit element gets what changed, and nothing while it is off screen", () => {
  const written = [];
  const light = new SceneLight({
    style: { setProperty: (name, value) => written.push([name, value]) },
  });
  light.setOnScreen(true);
  light.cast({ "--scene-sun-top": "rgb(250 234 205)", "--scene-glow": "0.00" });
  light.cast({ "--scene-sun-top": "rgb(250 234 205)", "--scene-glow": "0.40" });
  assert.deepEqual(written, [
    ["--scene-sun-top", "rgb(250 234 205)"],
    ["--scene-glow", "0.00"],
    ["--scene-glow", "0.40"],
  ]);
  written.length = 0;
  light.setOnScreen(false);
  light.cast({ "--scene-sun-top": "rgb(250 234 205)", "--scene-glow": "0.80" });
  assert.deepEqual(written, []);
  light.setOnScreen(true);
  assert.deepEqual(written, [["--scene-glow", "0.80"]]);
});
