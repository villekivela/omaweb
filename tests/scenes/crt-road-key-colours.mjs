// The CRT road's key colours as the website draws them, for the browser's road to be held to.
//
// The website's Scene (website/crt-road.js) is run against a 2D context that draws nothing and
// records what it was asked to fill with, and the colours that name the road are read off the
// record: the sky, the stars, the sun, the ridges, the desert, the centre line and the roadside.
// The browser's test (tests/ui/tst_nightroad.qml) reads the same colours from its own road and
// compares them with crt-road-key-colours.json, which this writes:
//
//   node tests/scenes/crt-road-key-colours.mjs > tests/scenes/crt-road-key-colours.json
//
// crt-road-key-colours.test.mjs fails when the website's colours and the file part.

import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";

import { createCrtRoad } from "../../website/crt-road.js";

const root = fileURLToPath(new URL("../../", import.meta.url));

// The themes both roads are compared in, as the browser's palette names them.
export const THEMES = {
  dark: { windowOpaque: "#16151d", text: "#f3f1fa", accent: "#9b87ff" },
  light: { windowOpaque: "#f4f3f8", text: "#1c1b22", accent: "#3366cc" },
};

function hex(value) {
  return value
    .replace(/^#/, "")
    .match(/../g)
    .map((pair) => parseInt(pair, 16));
}

function lightness([red, green, blue]) {
  return (Math.max(red, green, blue) + Math.min(red, green, blue)) / 2 / 255;
}

// "rgba(r, g, b, a)" as "#rrggbb", the alpha dropped: a key colour is the colour, not how much of
// it a mark lets through.
function opaque(css) {
  const [red, green, blue] = css
    .match(/[\d.]+/g)
    .slice(0, 3)
    .map(Number);
  return "#" + [red, green, blue].map((channel) => channel.toString(16).padStart(2, "0")).join("");
}

// A context that records every fill: what it filled with, solid or a gradient's stops, and
// whether the Scene was drawing its still layer or what moves over it.
function recorder() {
  const fills = [];
  let phase = "still";
  const gradient = () => {
    const stops = [];
    return { stops, addColorStop: (position, colour) => stops.push([position, colour]) };
  };
  const context = new Proxy(
    {
      fillStyle: "",
      strokeStyle: "",
      createLinearGradient: gradient,
      createRadialGradient: gradient,
      drawImage() {
        phase = "moving";
      },
      fill() {
        fills.push({ phase, style: context.fillStyle });
      },
      fillRect() {
        fills.push({ phase, style: context.fillStyle });
      },
    },
    {
      get(target, name) {
        return name in target ? target[name] : () => {};
      },
    },
  );
  return { context, fills };
}

export function parameters() {
  return JSON.parse(readFileSync(`${root}share/scenes/crt-road.json`, "utf8"));
}

// The road's key colours in one theme, drawn at time 0 with the reader still.
export function keyColours(theme) {
  const record = recorder();
  const layer = recorder();
  globalThis.document = { createElement: () => ({ getContext: () => layer.context }) };
  const palette = {
    ground: hex(theme.windowOpaque),
    text: hex(theme.text),
    accent: hex(theme.accent),
  };
  const scene = createCrtRoad(parameters());
  scene.draw(record.context, {
    width: 360,
    height: 225,
    pitch: scene.pitch,
    time: 0,
    navigating: 0,
    beat: 0,
    reducedMotion: false,
    palette,
    dark: lightness(palette.ground) <= 0.6,
    options: Object.fromEntries(
      Object.entries(scene.options).map(([name, values]) => [name, values[0]]),
    ),
    state: {},
  });

  const still = layer.fills;
  const moving = record.fills.filter((fill) => fill.phase === "moving");
  const gradients = still.filter((fill) => typeof fill.style !== "string");
  const solids = still.filter((fill) => typeof fill.style === "string");
  const [sky, , sun, desert] = gradients;
  const stars = solids.length - 3 - parameters().road.edges.length;
  const ridges = solids.slice(stars, stars + 3);
  const marks = [...new Set(moving.map((fill) => fill.style))];
  return {
    skyTop: opaque(sky.style.stops[0][1]),
    skyLow: opaque(sky.style.stops.at(-1)[1]),
    light: opaque(solids[0].style),
    sunTop: opaque(sun.style.stops[0][1]),
    sunLow: opaque(sun.style.stops.at(-1)[1]),
    ridges: ridges.map((fill) => opaque(fill.style)),
    desertNear: opaque(desert.style.stops[0][1]),
    centreLine: opaque(marks[0]),
    roadside: opaque(marks[1]),
  };
}

export function allKeyColours() {
  return Object.fromEntries(
    Object.entries(THEMES).map(([name, theme]) => [name, { theme, colours: keyColours(theme) }]),
  );
}

if (process.argv[1] === fileURLToPath(import.meta.url)) {
  process.stdout.write(JSON.stringify(allKeyColours(), null, 2) + "\n");
}
