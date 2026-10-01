# Landing page prototype (#440)

Throwaway. This directory lives on `prototype/440-website-design-language` and is never merged. It
asks one question: what one design language should the website and the browser share, with the Start
page's night road as its anchor?

```sh
scripts/serve_website.sh 8440      # then open http://localhost:8440/?variant=drive
```

The page mounts on localhost only. A floating bar at the bottom switches between `current`, the page
as it is, and `drive`, the prototype (`←` and `→` also work). It also switches the Scene and opens
the Chrome drawer, which shows what the page would ask of the browser's own chrome.

`drive` is the user's combination of two earlier variants, A and C, kept in commit `bd04d5c` of this
branch. The first screen is the Start page: the Omnibar resting on the road's horizon, its rows the
walk. Scrolling past it is the drive. The road dims under a scrim, speeds up with the scroll, and
the rest of the page passes as signs set close together: the walk with one pinned capture, a gantry
of features, the keys and the install. On a phone or with reduced motion the road holds a still
frame and the signs read as an ordinary list.

The Scenes are treatments of the night drive, for comparing live. The user's pick is `crt-road`, the
default:

| `?scene=`  | Name          | What it changes                                                          |
| ---------- | ------------- | ------------------------------------------------------------------------ |
| `vector`   | Vector        | Thin crisp lines in the accent, a deep sky, sparse stars, fog; no pixels |
| `led`      | LED matrix    | A coarse sign of round LEDs with soft bloom; the headline in the matrix  |
| `dither`   | Dither        | One bit: the theme's ground or accent per pixel, by an 8 by 8 Bayer      |
| `pixel`    | Refined pixel | Today's road with larger pixels, five theme tones, no seams or halo      |
| `crt-road` | CRT road      | The refined desert drive through the CRT; bands and road width options   |
| `crt`      | CRT           | Scanlines, phosphor bloom, a rolling refresh band and a faint flicker    |

CRT road is the user's refinement of the pixel road under the CRT. The ground grid, rail posts and
road edges are gone; the desert is a smooth gradient lit by the sun's glow, the ridges are filled
silhouettes in layered tones, a saguaro, a rock or a lone sign now and then passes at the roadside,
and a shooting star crosses about every half minute. About half the stars, a few brighter. Its two
options are on the bar: the sun's bands (4, the default, or 3) and the road's width at the
foreground (widest, the default, wider or wide; today's road is narrower than all three).

### Performance

`?perf=1` measures the Scene for 8 s at rest on the Start page, then 8 s while scrolling, in a fixed
1440 by 900 box at the device's density, and reports to the local server; `?perf=idle` does the same
with the Scene not drawing, which is the page's own floor. Measured on this Mac:

| Browser     | Before: fps, main thread a frame | After: fps, main thread a frame | CPU over the floor, after |
| ----------- | -------------------------------- | ------------------------------- | ------------------------- |
| Firefox 156 | 10, 94 ms                        | 30, 0.7 ms                      | about 7 to 13%            |
| Chrome 154  | 30, 0.9 ms                       | 30, 0.3 ms                      | about 10%                 |
| Safari 26.6 | 6.5, 0.3 ms                      | 30, 0.2 ms                      | about 20%                 |

Before, the CRT was five passes of 2D canvas at the full device resolution every frame: 93 ms of
Firefox's main thread, and in Safari a GPU stall that held it at 6.5 fps. Now the Scene draws a 360
by 225 picture from a cached still layer, the band and the flicker go into that picture, and the
scanlines, bloom and vignette are layers the browser composites.

The CRT's flicker darkens the picture by 2 to 4.5 percent, never a flash, and like the band it stops
for reduced motion.

`?theme=` takes any theme the page offers (`omaweb` is the site's own palette). The theme and the
Scene are remembered for the next visit.

The captures in `assets/shots/` are new: `start`, `omnibar` and `agents` join the existing states,
all from `scripts/build_website_themes.py`. The Start page and the Omnibar are drawn with
MultiEffect, which the software backend leaves out, so those two run under headless cage on an
output the capture's size. The Omnibar's engine suggestions come from a stub the script serves, via
the lab's `--sample-suggestions`.

## The Scene contract

A Scene is a drawing and nothing else. The page or the browser hosts it; the Scene never decides
when it runs, never reads the page, and never fetches anything.

### What a Scene receives

Each frame, `draw(ctx, input)` gets a 2D context the size of its display and:

| Input           | Meaning                                                                       |
| --------------- | ----------------------------------------------------------------------------- |
| `palette`       | The theme's roles as RGB triples: `ground`, `text`, `accent`, `muted`         |
| `dark`          | Whether the theme's ground is dark. A light theme still gets a night to draw  |
| `width, height` | The display's size in display pixels: the area divided by the Scene's `pitch` |
| `pitch`         | Logical pixels per display pixel, as the Scene declared it                    |
| `time`          | Seconds the Scene has run. It stops while the Scene is not drawing            |
| `navigating`    | How hard the reader is navigating now, 0 to 1. See below                      |
| `reducedMotion` | Hold still: the reader asked for less motion, or the host is on a phone       |
| `state`         | A plain object kept between frames and cleared on a resize or a theme change  |
| `options`       | The reader's choice for each option the Scene declares, or its first value    |

`navigating` is the reader moving through the web. In the browser it is 1 from the moment a
destination is committed until its page paints, the night road's drive today. On the website it is
the scroll's speed, easing back to 0 when the scroll stops. A Scene shows it however it likes; the
road Scenes speed up toward it and light the sun with it. Under `reducedMotion` it is always 0.

### What a Scene may do

- Draw anything into the context, in the palette's colours. `road.js` holds the night drive the road
  Scenes share: its geometry, palette mixes and motion, and the road filled in the theme's colours
  for a Scene to map to a display of its own. `display.js` offers colour mixing.
- Keep caches, and its own motion, in `state`.
- Declare `pitch`: logical pixels per display pixel (default 1), or `"device"` for one display pixel
  per device pixel. Declare `seams` to draw the dark grid between display pixels, and `fps` to cap
  its frame rate at what its motion needs. Declare `glass: "crt"` to be shown through the CRT the
  host composites over the canvas: scanlines, a bloom, the rolling band, the flicker and the
  vignette. The Scene then draws only its low-resolution picture; in the browser the glass would be
  one ShaderEffect over the Scene. Declare `options`, each a name and its values, for choices the
  reader makes; the site shows them on the bar, the browser would show them in Settings.

### What it may not do

- Start or stop itself, set timers, or ask for frames. The host draws it only while it is on screen,
  the tab is shown, the window has focus and motion is allowed, which is the Start page's own rule.
- Read the page, the network or storage, or draw outside its context.
- Depend on anything but its inputs: the same inputs, from the same start, draw the same frames.
- Flash. Any flicker stays within a few percent of brightness and stops for `reducedMotion`.

### Adding one

Write a classic script that registers itself after `scenes/host.js` has loaded, and list it in
`boot.js`:

```js
OmawebScenes.register({
  id: "rain",
  name: "Rain",
  pitch: 4,
  seams: true,
  draw: function (ctx, input) {
    // draw in input.palette, at input.width by input.height
  },
});
```

`road.js` is `src/ui/NightRoad.qml` ported line for line. Each treatment is one file under
`scenes/`.

### The same contract in the browser

Not built here. The app side is its own ticket. A Scene there would be a QML component a reader
drops into `~/.config/omaweb/scenes/<name>/Scene.qml`, chosen in Settings' interface section where
the road's switch is today:

```qml
// The host sets these; the Scene binds to them and nothing else.
Item {
    property var colors        // the palette the window draws, the same object NightRoad gets
    property bool dark
    property real time         // advanced by the host's FrameAnimation, only while running
    property real navigating   // 1 from a commit until its page paints
    property var options       // the reader's choices, from Settings
    property bool reducedMotion
    // width and height come from the host's anchors
}
```

`NightRoad.qml` nearly fits already: it takes `colors` and `running` and owns its own clock. It
moves to the contract by taking `time` from the host instead of a FrameAnimation of its own, and its
`driving` becomes `navigating`. The Private window's lights-off road stays a host effect: a palette
the host passes.
