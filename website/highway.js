// The drive's arithmetic: on a wide screen that allows motion, the landing page is a road. Each
// stop (the Start page, each card, the install) rises from the horizon at the sun, holds while it
// is read and passes the camera, and the scroll is the throttle. Cruise control drives the same
// road on the night radio's cues. These are the decisions; `drive.js` wires them to the page.

// Night road runs at about 107.5 beats a minute, so a bar is 2.23 seconds. Its beat comes in after
// eight bars and its sections turn every eight: the Start page holds the intro, each card four
// bars, and with eight cards the install lands on the section at bar 40.
export const BAR = 240 / 107.5;
const INTRO_BARS = 8;
const CARD_BARS = 4;

// When cruise control reaches each of `count` stops, in seconds into the song.
export function cues(count) {
  return Array.from({ length: count }, (_, index) =>
    index === 0 ? 0 : (INTRO_BARS + CARD_BARS * (index - 1)) * BAR,
  );
}

// The stop the song has reached at `time`: the last whose cue has come.
export function stopAt(time, times) {
  let index = 0;
  while (index + 1 < times.length && times[index + 1] <= time) ++index;
  return index;
}

// Where a stop stands, by `t`, its distance in stops from the one being read: past the horizon
// beyond -1, approaching to -0.3, held to 0.3 and gone past the camera by 0.85. Approaching, its
// depth falls from the horizon's to 1 and it is drawn at 1 / depth, so it grows as a thing on the
// road does, slowly far off and quickly near; passing, it grows on, drops below the camera and
// fades fast so the next one reads. The last stop is where the road ends, and holds.
const FAR = -1;
const ARRIVE = -0.3;
const LEAVE = 0.3;
const GONE = 0.85;
const HORIZON = 12;

export function placement(t, last) {
  if (last) t = Math.min(t, 0);
  if (t <= FAR || t >= GONE) return null;
  if (t < ARRIVE) {
    const near = (t - FAR) / (ARRIVE - FAR);
    const depth = 1 + (1 - near) * (HORIZON - 1);
    return {
      scale: 1 / depth,
      opacity: Math.min(1, near * 2.5),
      depth,
      drop: 0,
      reading: false,
    };
  }
  if (t <= LEAVE) return { scale: 1, opacity: 1, depth: 1, drop: 0, reading: true };
  const by = (t - LEAVE) / (GONE - LEAVE);
  const depth = 1 - 0.6 * by;
  return { scale: 1 / depth, opacity: (1 - by) ** 2.5, depth, drop: by * by, reading: false };
}

// The stop being read at `progress`, in stops along the road.
export function nearestStop(progress, count) {
  return Math.max(0, Math.min(count - 1, Math.round(progress)));
}

// The theme a clip shows at `time`, from its `[seconds, theme]` moments: the last put on by then.
export function wornAt(time, themes) {
  let worn;
  for (const [at, theme] of themes) if (at <= time) worn = theme;
  return worn;
}

// Cruise control: off; cruising, driving to each stop as its cue comes; held, the reader having
// taken the wheel, the music playing on; or parked at the install, the song playing out.
export class Cruise {
  constructor(times) {
    this.times = times;
    this.state = "off";
    this.current = -1;
  }

  start() {
    this.state = "cruising";
    this.current = -1;
  }

  // The stop to drive to at `time` in the song, when it is a new one.
  tick(time) {
    if (this.state !== "cruising") return null;
    const index = stopAt(time, this.times);
    if (index === this.times.length - 1) this.state = "parked";
    if (index === this.current) return null;
    this.current = index;
    return index;
  }

  // The reader takes the wheel; whether cruise control had it.
  hold() {
    if (this.state !== "cruising") return false;
    this.state = "held";
    return true;
  }

  // Cruise control again from stop `index`; the cue to seek the song to.
  resume(index) {
    this.state = "cruising";
    this.current = -1;
    return this.times[index];
  }

  end() {
    this.state = "off";
    this.current = -1;
  }
}
