// The drive's arithmetic, through `highway.js`'s exports: where each stop stands on the road, which
// stop the song has reached, and what cruise control does when the reader takes the wheel. The page
// wires these to the browser; they need none.

import assert from "node:assert/strict";
import test from "node:test";

import { BAR, Cruise, cues, nearestStop, placement, stopAt, wornAt } from "../highway.js";

// The Start page, eight cards and the install.
const STOPS = 10;

test("cues: the Start page holds the intro and each card four bars from the beat's entry", () => {
  const times = cues(STOPS);
  assert.equal(times.length, STOPS);
  assert.equal(times[0], 0);
  // The beat comes in after eight bars, at about 18 seconds.
  assert.ok(Math.abs(times[1] - 17.86) < 0.01, times[1]);
  for (let index = 2; index < STOPS; ++index) {
    assert.ok(Math.abs(times[index] - times[index - 1] - 4 * BAR) < 1e-9);
  }
  // The install lands on the section that starts at bar 40, about 1:29.
  assert.ok(Math.abs(times[STOPS - 1] - 40 * BAR) < 1e-9);
  assert.ok(Math.abs(times[STOPS - 1] - 89.3) < 0.1);
});

test("cues: the stop the song has reached is the last whose cue has come", () => {
  const times = cues(STOPS);
  assert.equal(stopAt(0, times), 0);
  assert.equal(stopAt(times[1] - 0.01, times), 0);
  assert.equal(stopAt(times[1], times), 1);
  assert.equal(stopAt(times[4] + 1, times), 4);
  assert.equal(stopAt(400, times), STOPS - 1);
});

test("placement: a stop rises from the horizon, holds while read, and passes the camera", () => {
  assert.equal(placement(-1.2, false), null);
  const far = placement(-0.95, false);
  const near = placement(-0.4, false);
  assert.ok(far.scale < 0.15, far.scale);
  assert.ok(near.scale > far.scale && near.scale < 1);
  assert.ok(far.opacity < near.opacity);
  assert.equal(far.reading, false);
  for (const t of [-0.3, 0, 0.3]) {
    assert.deepEqual(placement(t, false), {
      scale: 1,
      opacity: 1,
      depth: 1,
      drop: 0,
      reading: true,
    });
  }
  const passing = placement(0.6, false);
  assert.ok(passing.scale > 1);
  assert.ok(passing.opacity < 0.5);
  assert.ok(passing.drop > 0);
  assert.equal(placement(0.9, false), null);
});

test("placement: a nearer stop stands in front of a farther one", () => {
  const leaving = placement(0.5, false);
  const coming = placement(-0.5, false);
  assert.ok(leaving.depth < coming.depth);
});

test("placement: the last stop holds where the road ends instead of passing", () => {
  assert.deepEqual(placement(2, true), placement(0, true));
  assert.equal(placement(2, true).reading, true);
});

test("nearest: the stop being read is the nearest whole stop, within the road", () => {
  assert.equal(nearestStop(-0.4, STOPS), 0);
  assert.equal(nearestStop(3.4, STOPS), 3);
  assert.equal(nearestStop(3.6, STOPS), 4);
  assert.equal(nearestStop(42, STOPS), STOPS - 1);
});

test("worn: a clip's theme at a moment is the last one put on by then", () => {
  const themes = [
    [0, "ethereal-nebula-silver"],
    [4.1, "lumon"],
    [9.93, "ethereal-nebula-silver"],
  ];
  assert.equal(wornAt(0, themes), "ethereal-nebula-silver");
  assert.equal(wornAt(4.09, themes), "ethereal-nebula-silver");
  assert.equal(wornAt(4.1, themes), "lumon");
  assert.equal(wornAt(12, themes), "ethereal-nebula-silver");
  assert.equal(wornAt(3, []), undefined);
});

test("cruise: from the ignition it drives to each stop as its cue comes, and parks", () => {
  const cruise = new Cruise(cues(STOPS));
  assert.equal(cruise.state, "off");
  assert.equal(cruise.tick(0), null);
  cruise.start();
  assert.equal(cruise.state, "cruising");
  assert.equal(cruise.tick(0), 0);
  assert.equal(cruise.tick(1), null);
  assert.equal(cruise.tick(18), 1);
  assert.equal(cruise.tick(30), 2);
  assert.equal(cruise.tick(89.4), STOPS - 1);
  assert.equal(cruise.state, "parked");
  assert.equal(cruise.tick(120), null);
});

test("cruise: the reader takes the wheel, and nothing drives until they hand it back", () => {
  const cruise = new Cruise(cues(STOPS));
  cruise.start();
  cruise.tick(20);
  assert.equal(cruise.hold(), true);
  assert.equal(cruise.state, "held");
  assert.equal(cruise.tick(60), null);
  // Taking a wheel already taken, or one nobody is driving, changes nothing.
  assert.equal(cruise.hold(), false);
  assert.equal(new Cruise(cues(STOPS)).hold(), false);
});

test("cruise: resuming carries on from the stop being read, at its cue", () => {
  const times = cues(STOPS);
  const cruise = new Cruise(times);
  cruise.start();
  cruise.tick(20);
  cruise.hold();
  const cue = cruise.resume(5);
  assert.equal(cue, times[5]);
  assert.equal(cruise.state, "cruising");
  // The stop it resumes from is driven to again, wherever the page was.
  assert.equal(cruise.tick(cue), 5);
});

test("cruise: stopping ends the drive from any state", () => {
  for (const before of ["cruising", "held", "parked"]) {
    const cruise = new Cruise(cues(STOPS));
    cruise.start();
    if (before === "held") cruise.hold();
    if (before === "parked") cruise.tick(200);
    cruise.end();
    assert.equal(cruise.state, "off", before);
    assert.equal(cruise.tick(0), null);
  }
});
