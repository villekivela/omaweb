# Landing page prototype (#440)

Throwaway. This directory lives on `prototype/440-website-design-language` and is never merged. It
asks one question: what one design language should the website and the browser share, with the Start
page's night road as its anchor?

```sh
scripts/serve_website.sh 8440      # then open http://localhost:8440/?variant=a
```

The variants mount on localhost only. A floating bar at the bottom switches them (`←` and `→` also
work), switches the Scene, and opens the Chrome drawer, which shows what the variant would ask of
the browser's own chrome.

| Key       | Name           | What it tries                                                           |
| --------- | -------------- | ----------------------------------------------------------------------- |
| `current` | The page as is | Today's page, for reference                                             |
| `a`       | Start page     | The site opens as the browser's Start page; the walk is Omnibar rows    |
| `b`       | Display        | The pixel display is the material: lit dot-matrix type, Scene strips    |
| `c`       | The drive      | One Scene behind the whole page; scrolling drives it; content as signs  |
| `d`       | The window     | The site is an Omaweb window; sections are tabs, themes are Space cells |

`?theme=` takes any theme the page offers (`omaweb` is the site's own palette) and `?scene=` takes
`night-road` or `tunnel`. Both are remembered for the next visit.

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
| `reducedMotion` | The reader asked for less motion. Draw one still frame that reads on its own  |
| `state`         | A plain object kept between frames and cleared on a resize or a theme change  |

### What a Scene may do

- Draw anything into the context, in the palette's colours. `display.js` offers the Start page's
  lit-display pass (colourise to the accent, ordered dither) and a layer cache for what does not
  move.
- Keep caches in `state`.
- Declare `pitch` (default 1) and `seams` (draw the dark grid between display pixels).

### What it may not do

- Start or stop itself, set timers, or ask for frames. The host draws it only while it is on screen,
  the tab is shown, the window has focus and motion is allowed, which is the Start page's own rule.
- Read the page, the network or storage, or draw outside its context.
- Depend on anything but its inputs: the same inputs draw the same frame.

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

`night-road.js` is `src/ui/NightRoad.qml` ported line for line; `tunnel.js` is a new Scene written
to the contract alone.

### The same contract in the browser

Not built here. If a variant wins, the app side is its own ticket. A Scene there would be a QML
component a reader drops into `~/.config/omaweb/scenes/<name>/Scene.qml`, chosen in Settings'
interface section where the road's switch is today:

```qml
// The host sets these; the Scene binds to them and nothing else.
Item {
    property var colors        // the palette the window draws, the same object NightRoad gets
    property bool dark
    property real time         // advanced by the host's FrameAnimation, only while running
    property bool reducedMotion
    // width and height come from the host's anchors
}
```

`NightRoad.qml` nearly fits already: it takes `colors` and `running` and owns its own clock. It
moves to the contract by taking `time` from the host instead of a FrameAnimation of its own. Two of
its inputs sit outside the contract and would stay host effects: the Private window's lights-off
road, which is a palette the host passes, and the drive on commit, which speeds the road while a
page loads. Whether the drive becomes a sixth input or stays the night road's alone is an open
question for that ticket.
