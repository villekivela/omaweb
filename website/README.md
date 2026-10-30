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
  feature with a crop of the real interface, and the install. Each section is plain HTML with a
  comment saying what it is, so the words are edited where they are read; a card's crop is a
  modifier class in `styles.css`.
- `drive.js` is the landing page's behaviour: the road behind it, the Omnibar's rows, the film, the
  theme picker and the drive (below), and on a release page the road as a still header. `script.js`
  is what the landing page and the release pages share: the menu, the copy buttons, the radio and
  the release log. Both are optional: without them the road is a gradient, the page is stacked, the
  Omnibar's rows are links to their cards, and the film is an ordinary video.
- `highway.js` is what the drive decides: where each stop stands on the road, which stop the song
  has reached, and what cruise control does when the reader takes the wheel.
- `film.js` is what the film's own controls decide: what they show for its time, where a press on
  the scrubber seeks to and what a key does. `drive.js` wires them to the video, in place of the
  browser's controls, which it removes.
- `scene.js` is the Scene host and `crt-road.js` the one Scene the site ships. The contract between
  them is below.
- `assets/audio/nightroad.mp3` is the night radio: one song, Night road, which `script.js` plays on
  a loop from the Radio button in the header, or the `M` key, and stops on the next press. Nothing
  loads before that. On the drive, turning it on is the ignition.
- `assets/film/` is where the introduction film, its poster and its clips are served from. The build
  copies them there from the `film` release (`build/film.mjs`), and `film/README.md` at the
  repository root has how the film is recorded, cut and uploaded.
- `assets/og.png` is the picture a link to the site shows where it is shared: the film's poster
  frame, cut to 1200×630. Make it again from a new poster with
  `magick poster.webp -resize 1200x675 -gravity center -extent 1200x630 -strip assets/og.png`.
- `assets/fonts/` holds the self-hosted faces, each beside its SIL Open Font License: Tomorrow for
  headlines, Ioskeley Mono for text.
- `assets/shots/` holds the captures of the real interface, one directory per Omarchy theme, and
  `themes.css`, each theme's roles on whichever element carries `data-theme`, its keyword colour
  among them for the glass's grade. The page wears Ethereal Nebula Silver unless the reader picks
  another theme, and the road, the cards and the captures follow it. Ethereal Nebula Silver is the
  theme the desktop film is recorded in (`film/README.md`), so the site's default and the film's
  theme change together: the markup's `data-theme`, `theme.js`'s fallback and the film's desktop
  theme.
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
  screen: "raster", // "vector" for a Scene of lines, which the glass shows without a raster
  options: { bands: ["4", "3"] }, // the reader's choices, the first the default
  draw(context, input) {},
  light(input) {}, // the light it casts on the page; leave it out for none
};
```

`light(input)` returns the colours, amounts and CSS values the Scene lights the page with, by name:
a colour as an RGB triple, an amount as a number and a CSS value as a string. The host sets each on
every element the page names as lit, as `--scene-<name>`, an amount to two places, and writes only
what changed and nothing while that element is off screen. The CRT road casts the light its file's
`light` block describes, which the browser's Omnibar is lit by too: `rim`, a radial gradient in the
sun's colours centred on the sun, and `bloom`, `bloom-width` and `bloom-blur`, the blurred ring over
the rim. The bloom's opacity rests at the file's amount and lifts with the radio's beat as it swells
the sky around the sun, not at all below the glow's threshold or under `reducedMotion`. The landing
page lights the Omnibar with it: its rim catches the sun's light, brightest over the sun and
brighter on the beat. On the drive it lights every card and the install too.

`glass: "crt"` asks the host to show the canvas through a CRT. The browser composites the canvas
scaled up pixelated, a bloom, the beam's scanlines with one line to each of the Scene's rows, the
halation, a phosphor mask of red, green and blue stripes one device pixel wide, and darker corners.
A rolling band and a faint flicker are drawn into the Scene's picture. The flicker darkens by 2 to
4.5 percent and never flashes, and both stop under `reducedMotion`. The CRT is the host's, so a
Scene draws only its small picture and a frame costs the same however large the window. A Scene that
declares the glass also gives its amounts as `crt`, which the host draws by.

The glass also grades the picture toward a second hue, as a sunset: the bright parts take the hue at
their own brightness, more of them lower in the frame. The hue is the theme's keyword colour,
`--keyword`, or, where that is within 30 degrees of the accent's hue, nearly grey or missing, the
accent's hue turned half way round at the tint every Omarchy theme is rendered with. A release page
wears the landing page's theme, so its `--keyword` is that theme's: Omaweb hands a release page the
reader's own accent but no keyword colour, so in Omaweb the site theme's keyword is checked against
the reader's accent. The release page's header holds the road still without the glass, so it draws
no grade. A Scene whose `crt` gives no `grade` is not graded. The grade is drawn into the Scene's
picture after the halation is taken from it and before the bloom, as the browser's glass draws it.

`screen: "vector"` declares a Scene of lines, which a vector screen traced with its beam and had no
raster. The glass then draws no scanlines, mask or band, and keeps the brighter of each frame and
the last one faded, as the phosphor persisted. It keeps no trail under `reducedMotion`. A Scene that
leaves `screen` out is a raster one.

The browser's glass (src/ui/shaders/crtglass.frag) works the same sums: every line of the beam is as
wide as the beam half way between its narrowest and widest, the picture and the bloom are lifted and
clipped before the scanlines and the mask darken them, and the halation is cut at its threshold, in
a small pass of its own (src/ui/shaders/halation.frag), before the glass blurs it. It differs in two
small ways. Its blur is 3 by 3 binomial taps that spread as far as `halation.spacing`, where this
one is a Gaussian. Its bloom is graded after the copy is made smaller, where this one is made
smaller from the graded picture. The browser's road draws what moves into a canvas at each frame as
this one does, so its edges cover a share of a pixel, and paces its frames on the window's, as this
host does with `frameDue`.

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

- `pitch`, `fps`, `glass`, `screen` and `options` are the Scene's declarations, and `roadWidth` the
  half width at the foreground each `road` option means.
- `crt` is the glass's amounts, which both hosts draw by: the bloom's scale and opacity, the beam's
  narrowest and widest, falloff, gap shade and lift, the mask's dimming and lift, the halation's
  threshold, strength, reach and spacing, the grade's top and depth in the frame and its highlights'
  and midtones' brightness range and strength, the vignette, the band's timing and strength, and the
  flicker's least, range and frequencies.
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

## The drive

On a wide screen that allows motion, the landing page is a road, and the scroll is the throttle.
`drive.js` decides it once, as the page opens, by the same query `scene.js` holds the road still
under; a phone or a reader who asked for less motion keeps the stacked page. The page's height
becomes a track of one window's height to each stop, with a snap where each is read, and each stop
(the Start page, each card and the install) a layer over the road. `highway.js` places it by the
scroll: past the horizon, risen from the sun and growing, held while it is read, then growing on,
dropping below the camera and fading as the next one arrives. The install is where the road ends,
and holds. The road runs faster with the scroll than on the stacked page, and at speed through a
glide between stops. The scrim stands aside, the header stays in view, and the film has no stop of
its own: each card plays its own beat of it instead.

Ignite, the play button at the Omnibar's end, Radio in the header, `M` and Return on the empty
Omnibar all start cruise control: the radio plays Night road from its start, without its loop, and
the page drives to each stop on the song's cues. The Start page holds the eight bars before the beat
comes in, each card four, and the install lands on the section at bar 40. A radio that does not play
leaves the drive to a timer on the same cues. Anything the reader does takes the wheel: the drive
lets go where it is, and the music plays on, on its loop. A journey box under the cards carries the
words of the stop being read and offers Drive, Resume cruise, which carries on from the stop being
read with the song sought to its cue, and Stop, which with `Esc` or the radio turned off ends the
drive and the music; its line of the stops is the page's scrollbar. Keyboard first lights its keys,
and Return on an Omnibar row drives to its card. The page keeps no keys of its own beyond `Esc` and
`M`, so a browser's own, such as Omaweb's, reach it.

`assets/film/clips.json` names the cards with a clip of the film, `clip-<card>.webm` and `.mp4` with
`clip-<card>.webp`, its first frame. A card shows that frame from the horizon on, plays the clip
from its start when it is read and loops while it is, with the film's own controls; without
`clips.json` the cards keep their captures. The Theme card's clip names the theme the recording
wears at each moment, and the page wears each from its frame on, its picker naming it, and the
reader's theme again when the card is no longer read. A change of theme, the clip's or the reader's
from a swatch or `T`, opens with the desktop's diagonal reveal, a View Transition whose new view's
`clip-path` grows from a slanted centre line to the viewport's edges (`revealClip` in `highway.js`);
with reduced motion, or no View Transitions, it is at once. Leaving the card puts the reader's theme
back at once. Lumon, which the desktop film changes to, is among the page's themes for that, with no
swatch. Every card and the install catch the road's light as the Omnibar does.

## The release pages

`build/releases.mjs` reads every published release from GitHub, including the engine builds and the
package repository, and writes `dist/releases/index.html`, the newest browser release, and a page
per release at `dist/releases/<tag>/`. Each page is `index.html` with its `<main>` swapped for
`build/release.html`, so the header and the footer have one copy, and release notes go through
`build/render.mjs`, which decides what a body may become. `GITHUB_TOKEN`, if set, raises the API's
rate limit.

A release page is calm: the road as a thin still header that fades into the page, with no motion and
no glass, then the version, its date and its notes. Every release is listed beside them on a wide
screen, in a column that stays in view and scrolls on its own, and under them on a narrow one. It
wears the landing page's theme, Ethereal Nebula Silver or the one the reader picked there, which
`theme.js` keeps for the visit and applies in the head before the page paints. It also carries
`<meta name="omaweb-palette">`, so in Omaweb the window's palette arrives as `--omaweb-*`, and every
theme in `themes.css` defers to it: read in Omaweb, a release page is in the reader's own theme.

## Regenerating the shots

The shots are generated, not drawn. Build `omaweb-ui-lab` and point the script at a checkout of
Omarchy's `themes/` directory:

```sh
scripts/build_website_themes.py --lab <path to omaweb-ui-lab> --themes <omarchy>/themes
```

The Start page's road and the blur under the Omnibar, the shortcut sheet and Settings are drawn with
MultiEffect, which the software renderer leaves out, so the script captures them, and the Agents
footer with its page, under `cage` on its headless backend; the script's docstring lists what that
needs. Re-run it after a chrome change or an upstream theme change. The window's Status island
announces a release, the latest `v*` tag unless `--release` names another: before a tag, name the
release the shots are for, such as `--release v0.19.0`. The run fetches the sample tabs' icons from
their sites, so it needs the network; none of them is kept here. The page offers the themes its
swatches name, in `index.html`; a theme the script builds but no swatch names is never shown.
