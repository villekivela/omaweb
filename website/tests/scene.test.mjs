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
test("glass: its layers take their amounts from the shared file", () => {
  const crt = JSON.parse(
    readFileSync(new URL("../../share/scenes/crt-road.json", import.meta.url), "utf8"),
  ).crt;
  assert.deepEqual(glassLayers(crt), {
    bloom: "0.22",
    scan: "repeating-linear-gradient(transparent 0 2px, rgb(0 0 0 / 38%) 2px 3px)",
    vignette: "radial-gradient(ellipse at center, transparent 55%, rgb(0 0 0 / 55%) 100%)",
  });
  const changed = {
    ...crt,
    bloom: { scale: 3, opacity: 0.3 },
    scanlines: { every: 4, shade: 0.5 },
    vignette: { clear: 0.6, shade: 0.4 },
  };
  assert.deepEqual(glassLayers(changed), {
    bloom: "0.3",
    scan: "repeating-linear-gradient(transparent 0 3px, rgb(0 0 0 / 50%) 3px 4px)",
    vignette: "radial-gradient(ellipse at center, transparent 60%, rgb(0 0 0 / 40%) 100%)",
  });
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
