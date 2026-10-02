// The CRT road reads everything it draws by from share/scenes/crt-road.json, the file the
// browser's own road reads too (#496), and the site build ships that file where the page asks.

import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import test from "node:test";

import { SHARED } from "../build/site.mjs";
import { createCrtRoad } from "../crt-road.js";

const parameters = JSON.parse(
  readFileSync(new URL("../../share/scenes/crt-road.json", import.meta.url), "utf8"),
);

test("road: its declarations are the shared file's", () => {
  const road = createCrtRoad(parameters);
  assert.deepEqual(
    [road.id, road.pitch, road.fps, road.glass, road.options],
    [parameters.id, parameters.pitch, parameters.fps, parameters.glass, parameters.options],
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
