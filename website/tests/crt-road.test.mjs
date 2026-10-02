// The CRT road reads everything it draws by from share/scenes/crt-road.json, the file the
// browser's own road reads too (#496), and the site build ships that file where the page asks.

import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import test from "node:test";

import { SHARED } from "../build/site.mjs";
import { createCrtRoad } from "../crt-road.js";
import { lightProperties, sceneInput } from "../scene.js";

const parameters = JSON.parse(
  readFileSync(new URL("../../share/scenes/crt-road.json", import.meta.url), "utf8"),
);

test("road: its declarations are the shared file's", () => {
  const road = createCrtRoad(parameters);
  assert.deepEqual(
    [road.id, road.pitch, road.fps, road.glass, road.crt, road.options],
    [
      parameters.id,
      parameters.pitch,
      parameters.fps,
      parameters.glass,
      parameters.crt,
      parameters.options,
    ],
  );
});

test("road: every road width it offers has a width in the shared file", () => {
  for (const name of parameters.options.road)
    assert.equal(typeof parameters.roadWidth[name], "number");
});

test("build: the site ships the shared file at the address the page fetches", () => {
  const drive = readFileSync(new URL("../drive.js", import.meta.url), "utf8");
  const [address] = drive.match(/"([\w-]+\.json)"/).slice(1);
  assert.equal(new URL(SHARED[address]).pathname.endsWith("share/scenes/crt-road.json"), true);
});

// The road lights the page as well as its canvas: the Start page's Omnibar catches the sun at its
// rim in the sun's colours, brightening with the same beat that swells the sky's glow.
const retro82 = { ground: "#020c17", text: "#f6dcac", accent: "#faa968", muted: "#3f8f8a" };
const road = createCrtRoad(parameters);
const lit = (environment) =>
  lightProperties(
    road,
    sceneInput(road, {
      palette: retro82,
      width: 10,
      height: 10,
      ...environment,
    }),
  );

// The rim's light, worked out by hand from the shared file's `light.rim` in this theme: the sun's
// top colour 45% of the way to white over the sun, falling to its low colour and fading out, on an
// ellipse 78% of the Omnibar's width across and two sun radii, 30% of the window's height, down.
const retro82Rim =
  "radial-gradient(ellipse 78% 30svh at 50% var(--horizon), rgb(252 243 228) 6%, " +
  "rgb(250 234 205) 18%, rgb(249 182 121) 46%, rgb(249 182 121 / 0.35) 78%, " +
  "rgb(249 182 121 / 0.08) 100%)";

test("light: the Omnibar's rim takes the sun's colours and falloff from the shared file", () => {
  assert.equal(lit({})["--scene-rim"], retro82Rim);
});

test("light: the rim's bloom rests at the shared file's amount and lifts with the beat", () => {
  assert.equal(lit({})["--scene-bloom"], "0.40");
  assert.equal(lit({ beat: 0.01 })["--scene-bloom"], "0.40");
  assert.equal(lit({ beat: 0.4 })["--scene-bloom"], "0.52");
  assert.equal(lit({})["--scene-bloom-width"], "8.00");
  assert.equal(lit({})["--scene-bloom-blur"], "4.00");
});

test("light: under reduced motion or on a phone the rim holds still", () => {
  assert.equal(lit({ beat: 0.8, reducedMotion: true })["--scene-bloom"], "0.40");
  assert.equal(lit({ beat: 0.8, phone: true })["--scene-bloom"], "0.40");
});
