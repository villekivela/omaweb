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

// The hush in the last bar before the drop, 0 to 1: the world stills, the engine holding at its
// peak, so the drop has something to break. It comes in over the first 30% of the bar, holds, and
// ends at the drop.
export function quiet(time) {
  const drop = INTRO_BARS * BAR;
  if (time >= drop) return 0;
  const into = Math.min(1, Math.max(0, (time - (drop - BAR)) / (0.3 * BAR)));
  return into * into * (3 - 2 * into);
}

// How high the sun stands, 0 to 1 of its height, through the intro: it sits at `SUN_FROM` and
// climbs, easing in and out, to all of it at the drop, so it is the last bright thing in the hush.
export const SUN_FROM = 0.7;
export function rising(time) {
  const toward = Math.min(1, Math.max(0, time / (INTRO_BARS * BAR)));
  return SUN_FROM + (1 - SUN_FROM) * toward * toward * (3 - 2 * toward);
}

// How far the stars streak into speed lines, 0 to 1: at once and fully at the drop, back to
// points, slowly at the last, a second after.
export function streaking(time) {
  const after = time - INTRO_BARS * BAR;
  return after < 0 ? 0 : Math.max(0, 1 - after) ** 2;
}

// The intro's bar at `time`, 0 to 7, and 8 from the drop on: the page steps its headlights and its
// headline by it.
export function introBar(time) {
  return Math.min(INTRO_BARS, Math.max(0, Math.floor(time / BAR)));
}

// How the sun flares around the drop, as brightness over the page's own: through the last bar it
// builds, slowly and then faster, to `FLARE`; at the drop it bursts to `FLARE + BURST` and fades
// over `BURST_FADE` seconds rather than going out at once.
export const FLARE = 0.2;
export const BURST = 0.5;
export const BURST_FADE = 1.2;
export function glare(time) {
  const drop = INTRO_BARS * BAR;
  if (time < drop) return FLARE * Math.max(0, (time - (drop - BAR)) / BAR) ** 2;
  return (FLARE + BURST) * Math.max(0, 1 - (time - drop) / BURST_FADE) ** 2;
}

// How hard the resting engine ticks over, 0 to `BREATH` of the throttle: the sun's glow breathes in
// and out once every `BREATH_EVERY` seconds while the page waits for the journey.
export const BREATH = 0.2;
export const BREATH_EVERY = 6;
export function breath(time) {
  return (BREATH * (1 - Math.cos((2 * Math.PI * time) / BREATH_EVERY))) / 2;
}

// How dim the page rests while it waits for the journey, 0 to 1 of its brightness, and how dim it
// is through the intro: the rest's dim lifts toward the last bar, where the sun's glare takes over
// (a dim below 0).
export const IDLE_DIM = 0.3;
export function dimming(time) {
  const toward = Math.min(1, Math.max(0, time / ((INTRO_BARS - 1) * BAR)));
  const lifted = IDLE_DIM * (1 - toward * toward * (3 - 2 * toward));
  return lifted - glare(time);
}

// How hard the engine revs while the road waits parked through the intro, 0 to 0.7 of the throttle:
// a little at first and then more, to its peak a bar before the drop, where it holds through the
// hush for the drop to release.
export function revving(time) {
  const toward = Math.min(1, Math.max(0, time / ((INTRO_BARS - 1) * BAR)));
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

// Where a press or a drag at `x` along the journey box's line, `{ left, width }`, puts the reader
// on a road of `count` stops: in stops, held to the first and the last.
export function journeyAt(x, { left, width }, count) {
  if (!width) return 0;
  return Math.min(1, Math.max(0, (x - left) / width)) * (count - 1);
}

// The stop a scroll that has stopped at `at`, in stops, settles onto: one within a fifth of a stop,
// unless it already stands on it. A scroll left well between two, as a browser's half-page scroll
// leaves it, stays where it is.
export function settleTo(at) {
  const near = Math.round(at);
  const off = Math.abs(at - near);
  return off > 0.002 && off <= 0.2 ? near : null;
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
