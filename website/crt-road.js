// The CRT road: the night drive behind the landing page's Start page, seen through CRT glass. A
// desert under a banded sun, its ground a smooth gradient lit by the sun's glow, layered ridges,
// a wide road with only its dashed centre line, now and then a saguaro, a rock or a lone sign at
// the roadside, and a rare shooting star. It is drawn in the theme's colours at the browser's own
// Start page pitch of 4 px, and the host scales it up pixelated under the glass.
//
// The geometry and the palette mixes are src/ui/NightRoad.qml's. What does not move is drawn
// once per size, theme and option into a cached layer; each frame copies it and draws what moves.

const PIXEL = 4;
const BLACK = [0, 0, 0];
const WHITE = [255, 255, 255];

// Half the road's width at the bottom edge, as a share of the width.
const ROADS = { widest: 1, wider: 0.8, wide: 0.62 };

// The three ridges, far to near: a seed for their peaks, how tall they stand against the sky
// above the horizon, and how wide the notch the road runs out through.
const RIDGES = [
  [1.3, 0.3, 0.16],
  [4.2, 0.2, 0.22],
  [8.9, 0.11, 0.3],
];

function mix(a, b, amount) {
  return a.map((channel, index) => channel + (b[index] - channel) * amount);
}

function css(colour, alpha = 1) {
  const [red, green, blue] = colour.map(Math.round);
  return `rgba(${red}, ${green}, ${blue}, ${alpha})`;
}

function fraction(value) {
  return value - Math.floor(value);
}

function wrap(value, span) {
  return ((value % span) + span) % span;
}

// A repeatable scatter: the same index always lands in the same place.
function hash(index) {
  return fraction(Math.sin(index * 91.345 + 7.13) * 47453.5453);
}

// A night in the theme. A light theme's night is drawn from its dark text, so the ground stays
// dark and the lit lines stay light.
function colours({ palette, dark }) {
  const ground = dark ? palette.ground : mix(palette.text, BLACK, 0.45);
  const light = dark ? palette.text : palette.ground;
  const deep = mix(ground, BLACK, dark ? 0.45 : 0.72);
  const glow = palette.accent;
  return {
    ground,
    light,
    glow,
    skyTop: deep,
    skyLow: mix(ground, glow, 0.28),
    groundNear: mix(deep, BLACK, 0.35),
    sunTop: mix(light, WHITE, 0.4),
    sunLow: mix(glow, light, 0.25),
  };
}

function geometry(width, height, road) {
  const horizonY = Math.round(height / 2);
  const depth = height - horizonY;
  const halfWidth = width * road;
  return {
    w: width,
    h: height,
    horizonY,
    depth,
    vx: width / 2,
    halfWidth,
    // A point on the road at depth z, where z = 1 is the bottom edge, and lateral position u,
    // where the road's edges are at -1 and 1.
    screenY: (z) => horizonY + depth / z,
    screenX: (u, z) => width / 2 + (u * halfWidth) / z,
  };
}

// The drive: travel advances with the clock at a speed that eases toward what `navigating` asks,
// and the sun brightens with it. Kept in the Scene's state between frames.
function motion(input) {
  const s = input.state;
  if (input.reducedMotion) return { travel: 0.7, speed: 1, lit: 0 };
  if (s.last === undefined) Object.assign(s, { last: input.time, travel: 0, speed: 1, lit: 0 });
  const step = Math.max(0, Math.min(0.1, input.time - s.last));
  s.last = input.time;
  s.speed += (1 + 8 * input.navigating - s.speed) * Math.min(1, step * 2.2);
  s.lit += (input.navigating - s.lit) * Math.min(1, step * 3);
  s.travel += step * 1.6 * 0.8 * s.speed;
  return s;
}

function ridge(g, [seed, amplitude, notch]) {
  const points = [];
  for (let index = 0; index <= 220; index++) {
    const u = index / 220;
    const d = Math.min(1, Math.abs(u - 0.5) / notch);
    const valley = d * d * (3 - 2 * d);
    let height = 0.55 * Math.pow(1 - Math.abs(Math.sin(u * 6.3 + seed)), 1.6);
    height += 0.3 * Math.pow(1 - Math.abs(Math.sin(u * 15.7 + seed * 2.3)), 2);
    height += 0.15 * Math.pow(1 - Math.abs(Math.sin(u * 37.1 + seed * 5.1)), 2);
    height = 0.25 + height * 0.75;
    points.push([u * g.w, g.horizonY - height * amplitude * g.horizonY * valley]);
  }
  return points;
}

// The sun: a solid cap, then `bands` cuts that thicken toward the horizon.
function sun(context, g, c, radius, bands) {
  context.save();
  context.beginPath();
  const solid = radius * 0.4;
  context.rect(g.vx - radius, g.horizonY - radius, radius * 2, solid);
  const unit = (radius - solid) / (bands * 2);
  let y = g.horizonY - (radius - solid);
  for (let band = 0; band < bands; band++) {
    const gap = unit * (0.5 + band / bands);
    context.rect(g.vx - radius, y + gap, radius * 2, unit * 2 - gap);
    y += unit * 2;
  }
  context.clip();
  const disc = context.createLinearGradient(0, g.horizonY - radius, 0, g.horizonY);
  disc.addColorStop(0, css(c.sunTop));
  disc.addColorStop(1, css(c.sunLow));
  context.fillStyle = disc;
  context.beginPath();
  context.arc(g.vx, g.horizonY, radius, 0, Math.PI * 2);
  context.fill();
  context.restore();
}

// What does not move: the sky and its stars, the sunrise glow, the sun, the ridges, the desert
// and the road on it.
function still(context, g, c, bands) {
  const sky = context.createLinearGradient(0, 0, 0, g.horizonY);
  sky.addColorStop(0, css(c.skyTop));
  sky.addColorStop(0.55, css(mix(c.skyTop, c.skyLow, 0.35)));
  sky.addColorStop(1, css(c.skyLow));
  context.fillStyle = sky;
  context.fillRect(0, 0, g.w, g.horizonY + 1);

  // About a hundred stars, a few of them brighter, thinning toward the horizon.
  for (let index = 0; index < 120; index++) {
    const across = fraction(Math.sin(index * 12.9898) * 43758.5453);
    const high = Math.pow(fraction(Math.sin(index * 78.233) * 12543.123), 1.6);
    const bright = fraction(Math.sin(index * 39.425) * 9321.77);
    const size = bright > 0.93 ? 3 : bright > 0.75 ? 1.8 : 1.2;
    const alpha = bright > 0.93 ? 1 : (0.25 + 0.6 * bright) * (1 - high * 0.8);
    context.fillStyle = css(c.light, alpha);
    context.fillRect(across * g.w, high * g.horizonY * 0.9, size, size);
  }

  const halo = context.createRadialGradient(g.vx, g.horizonY, 0, g.vx, g.horizonY, g.w * 0.55);
  halo.addColorStop(0, css(c.sunLow, 0.6));
  halo.addColorStop(0.18, css(c.sunLow, 0.28));
  halo.addColorStop(0.45, css(c.glow, 0.1));
  halo.addColorStop(1, css(c.glow, 0));
  context.fillStyle = halo;
  context.fillRect(0, 0, g.w, g.horizonY);
  sun(context, g, c, g.h * 0.15, bands);

  // The ridges as filled silhouettes, each a tone darker than the one behind it.
  const tones = [
    mix(c.skyLow, c.skyTop, 0.2),
    mix(c.skyLow, c.skyTop, 0.55),
    mix(c.skyTop, BLACK, 0.25),
  ];
  RIDGES.forEach((spec, index) => {
    context.beginPath();
    for (const [x, y] of ridge(g, spec)) context.lineTo(x, y);
    context.lineTo(g.w, g.horizonY + 1);
    context.lineTo(0, g.horizonY + 1);
    context.closePath();
    context.fillStyle = css(tones[index]);
    context.fill();
  });

  // The desert at night: lit near the horizon and where the sun's glow lies on it, falling
  // quickly to dark sand in the foreground.
  const sand = context.createLinearGradient(0, g.horizonY, 0, g.h);
  sand.addColorStop(0, css(mix(c.sunLow, c.ground, 0.6)));
  sand.addColorStop(0.1, css(mix(c.ground, c.groundNear, 0.35)));
  sand.addColorStop(0.35, css(mix(c.groundNear, BLACK, 0.2)));
  sand.addColorStop(1, css(mix(c.groundNear, BLACK, 0.5)));
  context.fillStyle = sand;
  context.fillRect(0, g.horizonY, g.w, g.depth);
  context.save();
  context.translate(g.vx, g.horizonY);
  context.scale(1, 0.3);
  const glow = context.createRadialGradient(0, 0, 0, 0, 0, g.w * 0.45);
  glow.addColorStop(0, css(c.sunLow, 0.4));
  glow.addColorStop(0.5, css(c.sunLow, 0.12));
  glow.addColorStop(1, css(c.sunLow, 0));
  context.fillStyle = glow;
  context.fillRect(-g.w / 2, 0, g.w, g.depth / 0.3);
  context.restore();

  // The road: the sand a step darker at every depth, so it reads as a road through the desert,
  // lit where the desert is. Two slightly wider passes give it a soft edge rather than a line.
  const wedge = (spread, fill) => {
    context.fillStyle = fill;
    context.beginPath();
    context.moveTo(g.vx - 2, g.horizonY);
    context.lineTo(g.vx + 2, g.horizonY);
    context.lineTo(g.screenX(spread, 1), g.h);
    context.lineTo(g.screenX(-spread, 1), g.h);
    context.closePath();
    context.fill();
  };
  wedge(1.08, "rgba(0, 0, 0, 0.1)");
  wedge(1.04, "rgba(0, 0, 0, 0.1)");
  wedge(1, "rgba(0, 0, 0, 0.14)");
  // The sun on the asphalt, half the road's width.
  const streak = context.createLinearGradient(0, g.horizonY, 0, g.h);
  streak.addColorStop(0, css(c.sunTop, 0.4));
  streak.addColorStop(0.45, css(c.sunLow, 0.1));
  streak.addColorStop(1, css(c.sunLow, 0));
  wedge(0.5, streak);

  const horizon = context.createLinearGradient(0, 0, g.w, 0);
  horizon.addColorStop(0, css(c.glow, 0));
  horizon.addColorStop(0.5, css(c.sunTop, 0.9));
  horizon.addColorStop(1, css(c.glow, 0));
  context.fillStyle = horizon;
  context.fillRect(0, g.horizonY - 1, g.w, 2);
}

// Roadside silhouettes, each standing on the ground at (x, y) at scale s.
function saguaro(context, x, y, s) {
  const w = s * 0.13;
  context.fillRect(x - w / 2, y - s, w, s);
  context.fillRect(x - w * 2.2, y - s * 0.62, w * 1.7, w * 0.8);
  context.fillRect(x - w * 2.2, y - s * 0.86, w * 0.8, s * 0.3);
  context.fillRect(x + w * 0.5, y - s * 0.48, w * 1.5, w * 0.8);
  context.fillRect(x + w * 1.2, y - s * 0.74, w * 0.8, s * 0.3);
}

function rock(context, x, y, s) {
  context.beginPath();
  context.moveTo(x - s * 0.45, y);
  context.lineTo(x - s * 0.32, y - s * 0.22);
  context.lineTo(x - s * 0.05, y - s * 0.3);
  context.lineTo(x + s * 0.28, y - s * 0.18);
  context.lineTo(x + s * 0.42, y);
  context.closePath();
  context.fill();
}

function signpost(context, x, y, s) {
  context.fillRect(x - s * 0.025, y - s * 0.7, s * 0.05, s * 0.7);
  context.fillRect(x - s * 0.22, y - s * 0.9, s * 0.44, s * 0.24);
}

const ROADSIDE = [saguaro, rock, saguaro, signpost, rock, saguaro];

// What moves: the sun brightening as the reader navigates, a rare shooting star, the centre
// line and the roadside.
function moving(context, g, c, m, input) {
  if (m.lit > 0.02) {
    const halo = context.createRadialGradient(g.vx, g.horizonY, 0, g.vx, g.horizonY, g.w * 0.4);
    halo.addColorStop(0, css(c.sunLow, 0.3 * m.lit));
    halo.addColorStop(1, css(c.sunLow, 0));
    context.fillStyle = halo;
    context.fillRect(0, 0, g.w, g.h);
  }

  // A shooting star in some fourteen-second slots, for under a second.
  const slot = Math.floor(input.time / 14);
  const into = input.time - slot * 14;
  if (!input.reducedMotion && into < 0.8 && hash(slot) > 0.35) {
    const length = g.w * 0.12;
    const run = into / 0.8;
    const x = (0.15 + 0.7 * hash(slot + 1)) * g.w + length * run;
    const y = (0.08 + 0.3 * hash(slot + 2)) * g.horizonY + length * 0.35 * run;
    const trail = context.createLinearGradient(x - length * 0.5, y - length * 0.175, x, y);
    trail.addColorStop(0, css(c.light, 0));
    trail.addColorStop(1, css(mix(c.light, WHITE, 0.5), 1 - run * 0.6));
    context.strokeStyle = trail;
    context.lineWidth = 3;
    context.beginPath();
    context.moveTo(x - length * 0.5, y - length * 0.175);
    context.lineTo(x, y);
    context.stroke();
  }

  // The dashed centre line, its marks stretching as the road speeds up.
  const length = 0.42 + Math.min(0.7, (m.speed - 1) * 0.1);
  const scale = g.halfWidth / (g.w * 0.42);
  context.fillStyle = css(mix(c.sunTop, WHITE, 0.35));
  for (let index = 0; index < 26; index++) {
    const d = 1 + wrap(index * 1.15 - m.travel, 26 * 1.15);
    const near = g.screenY(d);
    const far = g.screenY(d + length);
    const width = Math.max(1, 12 / d) * scale;
    context.globalAlpha = Math.max(0, Math.min(1, 1.3 - d / 30) * Math.min(1, (d - 1) * 2.5));
    context.fillRect(g.vx - width / 2, far, width, Math.max(1, near - far));
  }

  // Six silhouettes on a long loop, far apart.
  const span = 44;
  context.fillStyle = css(mix(c.groundNear, BLACK, 0.55));
  ROADSIDE.forEach((draw, index) => {
    const d = 1.15 + wrap(index * (span / ROADSIDE.length) + hash(index) * 3 - m.travel, span);
    const side = hash(index + 10) > 0.5 ? 1 : -1;
    const out = 1.25 + hash(index + 20) * 1.4;
    const size = (g.depth * (draw === saguaro ? 0.55 : 0.35)) / d;
    context.globalAlpha = Math.max(0, Math.min(1, (span - d) / 10)) * Math.min(1, (d - 1.15) * 3);
    draw(context, g.screenX(side * out, d), g.screenY(d), size);
  });
  context.globalAlpha = 1;
}

export const crtRoad = {
  id: "crt-road",
  name: "CRT road",
  pitch: PIXEL,
  glass: "crt",
  // The road moves in whole pixels; 30 frames a second is all its motion shows.
  fps: 30,
  options: { bands: ["4", "3"], road: ["widest", "wider", "wide"] },

  draw(context, input) {
    const g = geometry(input.width * PIXEL, input.height * PIXEL, ROADS[input.options.road]);
    const c = colours(input);
    const s = input.state;
    if (!s.still) {
      s.still = document.createElement("canvas");
      s.still.width = input.width;
      s.still.height = input.height;
      const layer = s.still.getContext("2d");
      layer.scale(1 / PIXEL, 1 / PIXEL);
      still(layer, g, c, Number(input.options.bands));
    }
    context.setTransform(1, 0, 0, 1, 0, 0);
    context.drawImage(s.still, 0, 0);
    context.setTransform(1 / PIXEL, 0, 0, 1 / PIXEL, 0, 0);
    moving(context, g, c, motion(input), input);
    context.setTransform(1, 0, 0, 1, 0, 0);
  },
};
