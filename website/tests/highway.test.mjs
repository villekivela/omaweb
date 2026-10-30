// The drive's arithmetic, through `highway.js`'s exports: where each stop stands on the road, which
// stop the song has reached, and what cruise control does when the reader takes the wheel. The page
// wires these to the browser; they need none.

import assert from "node:assert/strict";
import test from "node:test";

import {
  BAR,
  Cruise,
  cues,
  keyAnswer,
  launch,
  nearestStop,
  placement,
  quiet,
  revving,
  revealClip,
  stopAt,
  wornAt,
} from "../highway.js";

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

test("keys: Esc stops the drive wherever the reader's focus is", () => {
  assert.equal(keyAnswer("Escape", false), "stop");
  // Focus stays on Ignite or Resume cruise once pressed.
  assert.equal(keyAnswer("Escape", true), "stop");
});

test("keys: M is the radio's, a cruise control's own key is its, and any other takes the wheel", () => {
  assert.equal(keyAnswer("m", false), null);
  assert.equal(keyAnswer("M", false), null);
  assert.equal(keyAnswer("Enter", true), null);
  for (const key of ["j", "k", "ArrowDown", " ", "f", "o"]) {
    assert.equal(keyAnswer(key, false), "hold", key);
  }
});

// The new theme opens from a slanted line through the middle of the viewport, as Omarchy's desktop
// does, its edges leaning 0.18 px left for every px down. On a 1000 x 500 viewport the line runs
// from x 545 at the top to x 455 at the bottom.
test("reveal: the new theme opens from a slanted centre line and widens to cover the viewport", () => {
  assert.equal(revealClip(0, 1000, 500), "polygon(545px 0px, 545px 0px, 455px 500px, 455px 500px)");
  assert.equal(
    revealClip(0.5, 1000, 500),
    "polygon(272.5px 0px, 817.5px 0px, 727.5px 500px, 182.5px 500px)",
  );
  assert.equal(revealClip(1, 1000, 500), "polygon(0px 0px, 1090px 0px, 1000px 500px, -90px 500px)");
});

// The intro builds to the drop: the road waits parked while the engine revs, a little at first and
// then more, up to 0.7 of the throttle as the beat comes in at bar 8, and the drive sets off at it.
test("revving: the intro builds slowly, then faster, to 0.7 at the drop", () => {
  const drop = 8 * BAR;
  assert.equal(revving(0), 0);
  assert.equal(revving(drop / 2), 0.175);
  assert.equal(revving(drop), 0.7);
  assert.equal(revving(drop * 3), 0.7);
  assert.equal(revving(-1), 0);
});

// At the drop the road does not ease into its speed: it starts at it, as a throw forward, and the
// throw falls away over the first 45% of the glide, before the glide's own swell takes over.
test("launch: full at the drop, falling away to nothing by 45% of the glide", () => {
  assert.equal(launch(0), 1);
  assert.equal(launch(0.225), 0.25);
  assert.equal(launch(0.45), 0);
  assert.equal(launch(1), 0);
  assert.ok(launch(0.1) > launch(0.2));
});

// The last bar before the drop is a hush: the world dims and stills, the engine cut, so the drop
// has something to break. It comes in over the first 30% of the bar, holds, and ends at the drop.
test("quiet: the last bar before the drop hushes the world, and the drop ends it", () => {
  const drop = 8 * BAR;
  assert.equal(quiet(0), 0);
  assert.equal(quiet(drop - 1.01 * BAR), 0);
  assert.ok(Math.abs(quiet(drop - 0.85 * BAR) - 0.5) < 1e-9);
  assert.equal(quiet(drop - 0.7 * BAR), 1);
  assert.equal(quiet(drop - 0.01), 1);
  assert.equal(quiet(drop), 0);
  assert.equal(quiet(drop + 5), 0);
  // The engine is cut in the hush, and a rev is not asked for in it.
  assert.equal(revving(drop - 0.5), 0);
});
