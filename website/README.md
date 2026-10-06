# Website

The site at <https://omaweb.app>: one landing page and the release pages. The landing page is plain
HTML, CSS and scripts, served as they are, and it loads nothing from another origin, so it fits the
Content-Security-Policy `vercel.json` sends. `scripts/check_website_csp.py` holds it to that, for
the pages and for the markup the scripts build. `docs/development.md` has how it builds and deploys.

```sh
npm ci                              # once: the Markdown parser the release pages use
node build/site.mjs                 # write dist/, the site as it deploys
../scripts/serve_website.sh         # build without the network and serve dist/
node --test 'build/*.test.mjs' 'tests/*.test.mjs'
```

## What is where

- `index.html` and `styles.css` are the page. The landing page opens on the Start page, the Omnibar
  resting on a night road with the features as its rows, then the introduction film, a card for each
  feature with a crop of the real interface, the install and a link to the release notes. Each
  section is plain HTML with a comment saying what it is, so the words are edited where they are
  read; a card's crop is a modifier class in `styles.css`.
- `drive.js` is the landing page's behaviour: the road behind it, the Omnibar's rows, the film and
  the theme picker, and on a release page the road as a still header. `script.js` is what the
  landing page and the release pages share: the menu, the copy buttons and the release log. Both are
  optional: without them the road is a gradient, the Omnibar's rows are links to their cards, and
  the film is an ordinary video.
- `film.js` is what the film's own controls decide: what they show for its time, where a press on
  the scrubber seeks to and what a key does. `drive.js` wires them to the video, in place of the
  browser's controls, which it removes.
- `scene.js` is the Scene host and `crt-road.js` the one Scene the site ships. The contract between
  them is below.
- `assets/audio/nightroad.mp3` is the night radio: one song, Night road, which `script.js` plays on
  a loop from the Radio button, on the Omnibar or a release page's header, or the `M` key, and stops
  on the next press. Nothing loads before that.
- `assets/film/` is where the introduction film and its poster are served from. The build copies
  them there from the `film` release (`build/film.mjs`), and `film/README.md` at the repository root
  has how the film is recorded and uploaded.
- `assets/og.png` is the picture a link to the site shows where it is shared: the film's poster
  frame, cut to 1200×630. Make it again from a new poster with
  `magick poster.webp -resize 1200x675 -gravity center -extent 1200x630 -strip assets/og.png`.
- `assets/art/` holds the textures and `assets/fonts/` the self-hosted faces, each beside its SIL
  Open Font License: Tomorrow for headlines, Ioskeley Mono for text.
- `assets/shots/` holds the captures of the real interface, one directory per Omarchy theme, and
  `themes.css`, each theme's roles on whichever element carries `data-theme`. The page wears Retro
  82 unless the reader picks another theme, and the road, the cards and the captures follow it.
- `tests/` holds the page's tests and `build/` the build: `site.mjs` copies the site into `dist/`
  and writes the release pages with `releases.mjs`, `release.html` and `render.mjs`. Neither is
  served.

## Scenes

A Scene is the drawing behind the Start page, and nothing else, though it may name the light it
casts, which the host lays on the page. The host owns its canvas and its clock, decides when it may
draw, and hands it everything it may know. The browser's Start page is to take the same contract
(#496), so a reader's own Scene could stand behind both.

### What a Scene receives

`draw(context, input)` is called with the 2D context of a canvas the size of the Scene's display,
and an input:

| Input           | What it is                                                                                                                           |
| --------------- | ------------------------------------------------------------------------------------------------------------------------------------ |
| `palette`       | The theme's roles as RGB triples: `ground`, `text`, `accent` and `muted`                                                             |
| `dark`          | Whether the theme's ground is dark. A light theme still asks for a night                                                             |
| `width, height` | The display in the Scene's pixels: the canvas's size divided by its `pitch`                                                          |
| `pitch`         | CSS pixels to one of the Scene's pixels, as the Scene declared it                                                                    |
| `time`          | Seconds the Scene has run. The clock stops while the Scene does not draw                                                             |
| `navigating`    | How hard the reader is moving, 0 to 1: the scroll's speed, or in the browser 1 from a commit until its page paints                   |
| `beat`          | The radio's beat, 0 to 1: how far the song's bass rises over its recent level, struck at once and let fall; 0 while the radio is off |
| `reducedMotion` | Hold still: the reader asked for less motion, or the page is on a phone                                                              |
| `options`       | The reader's choice for each option the Scene declares, or the first value                                                           |
| `state`         | An object kept between frames and emptied on a new size or theme                                                                     |

Under `reducedMotion` the Scene draws one frame that reads on its own, and `navigating` and `beat`
are 0.

### What a Scene declares

```js
export const myScene = {
  id: "my-scene",
  name: "My scene",
  pitch: 4, // CSS pixels to one of the Scene's pixels; 1 by default
  fps: 30, // the most frames a second its motion needs; uncapped by default
  glass: "crt", // shown through the CRT glass; leave it out for none
  options: { bands: ["4", "3"] }, // the reader's choices, the first the default
  draw(context, input) {},
  light(input) {}, // the light it casts on the page; leave it out for none
};
```

`light(input)` returns the colours, amounts and CSS values the Scene lights the page with, by name:
a colour as an RGB triple, an amount as a number and a CSS value as a string. The host sets each on
the one element the page names as lit, as `--scene-<name>`, an amount to two places, and writes only
what changed and nothing while that element is off screen. The CRT road casts the light its file's
`light` block describes, which the browser's Omnibar is lit by too: `rim`, a radial gradient in the
sun's colours centred on the sun, and `bloom`, `bloom-width` and `bloom-blur`, the blurred ring over
the rim. The bloom's opacity rests at the file's amount and lifts with the radio's beat as it swells
the sky around the sun, not at all below the glow's threshold or under `reducedMotion`. The landing
page lights the Omnibar with it: its rim catches the sun's light, brightest over the sun and
brighter on the beat.

`glass: "crt"` asks the host to show the canvas through a CRT: scaled up pixelated, with a bloom,
scanlines and darker corners the browser composites, and a rolling band and a faint flicker drawn
into the Scene's picture. The flicker darkens by 2 to 4.5 percent and never flashes, and both stop
under `reducedMotion`. The CRT is the host's, so a Scene draws only its small picture and a frame
costs the same however large the window. A Scene that declares the glass also gives its amounts as
`crt`, which the host draws by.

### What a Scene may not do

- Start or stop itself, set timers or ask for frames. The host draws it only while its canvas is on
  screen, the tab is shown, the window has focus and nothing asks it to hold still, at no more than
  its `fps`.
- Read the page, the network or storage, or draw outside its canvas.
- Depend on anything but its input: the same inputs from the same start draw the same frames.

### The CRT road

`crt-road.js` is the night drive: a desert under a banded sun, its ground lit by the sun's glow,
layered ridges, the widest road with only its dashed centre line, now and then a saguaro, a rock or
a lone sign at the roadside, and a rare shooting star. What does not move is drawn once per size and
theme into a layer the Scene keeps; each frame copies it and draws the centre line, the roadside and
the shooting star. It declares `bands` (4 or 3) and `road` (widest, wider or wide), and the site
takes the defaults.

Everything it draws by is in `share/scenes/crt-road.json`, outside `website/`, because the browser's
own Start page road reads the same file (#496). The script holds only how it draws:

- `pitch`, `fps`, `glass` and `options` are the Scene's declarations, and `roadWidth` the half width
  at the foreground each `road` option means.
- `crt` is the glass's amounts, which both hosts draw by: the bloom's scale and opacity, the
  scanlines' spacing and shade, the vignette, the band's timing and strength, and the flicker's
  least, range and frequencies.
- `night` is how the night's colours are mixed from the theme's roles; every other colour names a
  role (`ground`, `light`, `glow`, `skyTop`, `skyLow`, `groundNear`, `sunTop`, `sunLow`, `black`,
  `white`) or a `mix` of two at an amount, and a gradient is a list of `[position, colour]` or
  `[position, colour, alpha]` stops.
- `horizon`, `sky`, `stars`, `halo`, `sun`, `ridges`, `desert` and `road` are the still layer;
  `centreLine`, `roadside`, `shootingStar`, `navigatingGlow` and `beatGlow`, the sky around the sun
  lit with the radio's beat, are what moves, at the `motion` timings, and `scatter` seeds the
  repeatable placing.

Lengths are shares of the Scene's width or height unless the key says otherwise, and times are in
seconds. `build/site.mjs` copies the file into `dist/` beside the page, and `drive.js` fetches it
before the road starts; a page that cannot fetch it keeps the gradient the road stands on. Change
the road's look there, not in the script.

Measured with the Scene in a 1440 by 900 box at twice the density, at rest on the Start page and
while scrolling, it holds 30 frames a second in Firefox, Chrome and Safari for about a millisecond
of main thread a frame.

## The release pages

`build/releases.mjs` reads every published release from GitHub, including the engine builds and the
package repository, and writes `dist/releases/index.html`, the newest browser release, and a page
per release at `dist/releases/<tag>/`. Each page is `index.html` with its `<main>` swapped for
`build/release.html`, so the header and the footer have one copy, and release notes go through
`build/render.mjs`, which decides what a body may become. `GITHUB_TOKEN`, if set, raises the API's
rate limit.

A release page is calm: the road as a thin still header, with no motion and no glass, then one
column with the version, its date and its notes, and every release under them. It wears the landing
page's theme, Retro 82 or the one the reader picked there, which `theme.js` keeps for the visit and
applies in the head before the page paints. It also carries `<meta name="omaweb-palette">`, so in
Omaweb the window's palette arrives as `--omaweb-*`, and every theme in `themes.css` defers to it:
read in Omaweb, a release page is in the reader's own theme.

## Regenerating the shots

The shots are generated, not drawn. Build `omaweb-ui-lab` and point the script at a checkout of
Omarchy's `themes/` directory:

```sh
scripts/build_website_themes.py --lab <path to omaweb-ui-lab> --themes <omarchy>/themes
```

The Start page's road and the Omnibar are drawn with MultiEffect, which the software renderer leaves
out, so the script captures them, and the Agents footer with its page, under `cage` on its headless
backend; the script's docstring lists what that needs. Re-run it after a chrome change or an
upstream theme change. The page offers the themes its swatches name, in `index.html`; a theme the
script builds but no swatch names is never shown.
