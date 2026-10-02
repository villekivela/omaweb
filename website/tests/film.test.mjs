// What the introduction's controls show and do, through `film.js`'s exports. Wiring them to the
// video needs a browser; what they decide from its time and the reader's keys does not.

import assert from "node:assert/strict";
import test from "node:test";

import { filmKey, filmState, seekAt } from "../film.js";

test("state: a playing film presses the button and fills the scrubber to its time", () => {
  const state = filmState({ paused: false, currentTime: 15, duration: 60 });
  assert.equal(state.playing, true);
  assert.equal(state.progress, 0.25);
  assert.equal(state.elapsed, "0:15");
  assert.equal(state.total, "1:00");
  assert.equal(state.valueNow, 15);
  assert.equal(state.valueMax, 60);
  assert.equal(state.valueText, "0:15 of 1:00");
});

// Until its metadata arrives a video's duration is NaN, and a live stream's Infinity.
test("state: a film whose length is not known yet reads as empty, not as NaN", () => {
  for (const duration of [NaN, Infinity, 0]) {
    const state = filmState({ paused: true, currentTime: 0, duration });
    assert.equal(state.playing, false);
    assert.equal(state.progress, 0);
    assert.equal(state.total, "0:00");
    assert.equal(state.valueMax, 0);
    assert.equal(state.valueText, "0:00 of 0:00");
  }
});

test("seek: a press on the scrubber seeks to its share of the track", () => {
  const track = { left: 100, width: 400 };
  assert.equal(seekAt(200, track, 60), 15);
  assert.equal(seekAt(300, track, 60), 30);
});

// A drag carries on past the track's ends while the pointer is held. The film loops, so a seek to
// its very end would start it again; the end is its last tenth of a second.
test("seek: a drag past either end of the track holds at the start or the last frame", () => {
  const track = { left: 100, width: 400 };
  assert.equal(seekAt(20, track, 60), 0);
  assert.equal(seekAt(500, track, 60), 59.9);
  assert.equal(seekAt(900, track, 60), 59.9);
});

test("seek: a film whose length is not known yet stays at the start", () => {
  assert.equal(seekAt(300, { left: 100, width: 400 }, NaN), 0);
  assert.equal(seekAt(300, { left: 100, width: 0 }, 60), 0);
});

const film = { currentTime: 30, duration: 60 };

test("keys: Space and K play or pause the film", () => {
  for (const key of [" ", "k", "K"]) assert.deepEqual(filmKey(key, film), { toggle: true });
});

test("keys: the arrows step the film five seconds back or on", () => {
  assert.deepEqual(filmKey("ArrowLeft", film), { seek: 25 });
  assert.deepEqual(filmKey("ArrowDown", film), { seek: 25 });
  assert.deepEqual(filmKey("ArrowRight", film), { seek: 35 });
  assert.deepEqual(filmKey("ArrowUp", film), { seek: 35 });
});

test("keys: Page Up and Page Down step a tenth of the film, Home and End go to its ends", () => {
  assert.deepEqual(filmKey("PageDown", film), { seek: 24 });
  assert.deepEqual(filmKey("PageUp", film), { seek: 36 });
  assert.deepEqual(filmKey("Home", film), { seek: 0 });
  assert.deepEqual(filmKey("End", film), { seek: 59.9 });
});

test("keys: a step past either end holds at the start or the last frame", () => {
  assert.deepEqual(filmKey("ArrowLeft", { currentTime: 2, duration: 60 }), { seek: 0 });
  assert.deepEqual(filmKey("ArrowRight", { currentTime: 58, duration: 60 }), { seek: 59.9 });
});

test("keys: other keys, and seeking a film whose length is not known yet, do nothing", () => {
  assert.equal(filmKey("j", film), null);
  assert.equal(filmKey("Enter", film), null);
  assert.equal(filmKey("ArrowRight", { currentTime: 0, duration: NaN }), null);
});
