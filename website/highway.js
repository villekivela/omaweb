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

// How hard the engine revs while the road waits parked through the intro, 0 to 0.7 of the throttle:
// a little at first and then more, to the drop where the beat comes in and the drive sets off.
export function revving(time) {
  const toward = Math.min(1, Math.max(0, time / (INTRO_BARS * BAR)));
  return 0.7 * toward ** 2;
}

// The throw forward as the drive sets off at the drop, 0 to 1 over the first 45% of the first
// glide (`t`, 0 to 1): full at once, falling away to nothing. The page scales it by `LAUNCH`.
export const LAUNCH = 1;
export function launch(t) {
  return Math.max(0, 1 - t / 0.45) ** 2;
}

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

// The reveal of a theme the film changes, as Omarchy's desktop draws it
// (/usr/share/omarchy/shell/plugins/background/Background.qml): the new theme opens from a slanted
// line through the middle of the viewport and widens to both edges. The band's edges lean by
// `SLANT`, x moving that many px per px down, over `REVEAL_MS`, eased in and out as a cubic (the
// standard `cubic-bezier` of ease-in-out cubic, which QML's `InOutCubic` is a close match for).
const SLANT = -0.18;
export const REVEAL_MS = 420;
export const REVEAL_EASING = "cubic-bezier(0.65, 0, 0.35, 1)";

// The `clip-path` of the new theme's view when the band has spread `spread` (0 to 1) of the way to
// covering a `width` x `height` viewport.
export function revealClip(spread, width, height) {
  const lean = -SLANT * (height / 2);
  const half = spread * (width / 2 + lean);
  const edge = (centre, y) => `${centre}px ${y}px`;
  const top = width / 2 + lean;
  const bottom = width / 2 - lean;
  return `polygon(${[
    edge(top - half, 0),
    edge(top + half, 0),
    edge(bottom + half, height),
    edge(bottom - half, height),
  ].join(", ")})`;
}

// What a key the reader presses does to the drive: Esc stops it wherever their focus is, M is the
// radio's own, a key on one of cruise control's own controls (`own`) is that control's, and any
// other takes the wheel.
export function keyAnswer(key, own) {
  if (key === "Escape") return "stop";
  if (key.toLowerCase() === "m" || own) return null;
  return "hold";
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
