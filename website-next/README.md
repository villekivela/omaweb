# Website preview

A preview of the redesigned landing page, kept beside `website/` rather than in it. It is plain
HTML, CSS and scripts with no build step, and it loads nothing from another origin, so it already
fits the Content-Security-Policy the live site sends. Nothing deploys it.

Serve the directory with any static server that types `.webp` and `.woff2`, for example:

```sh
python3 -m http.server --directory website-next 8002
```

## What is where

- `index.html`, `styles.css` and `script.js` are the page. The script is optional: without it the
  page reads in order, the walkthrough shows its first shot and the dashboard screen shows its first
  line.
- `audio.js` is the night radio: two stations of drive music synthesized live with the Web Audio
  API. It makes no sound until a reader presses the hidden dashboard button or `M`, and each press
  tunes to the next station, then off.
- `assets/art/` holds the illustrations and textures. Its README says what each file is.
- `assets/fonts/` holds the self-hosted faces, each beside its SIL Open Font License: Tomorrow for
  headlines, Ioskeley Mono for text, Prompt Black Italic for the poster.
- `assets/shots/` holds the walkthrough's captures of the real interface, one directory per Omarchy
  theme, and `themes.css`, each theme's colours for the frame the captures sit in.

## The release pages

`releases/` is written, not committed. `build/releases.mjs` reads every published release from
GitHub, including the engine builds and the package repository, and writes `releases/index.html`,
the newest browser release, and a page per release at `releases/<tag>/`:

```sh
node website-next/build/releases.mjs
```

Each page is `index.html` with its `<main>` swapped for `build/release.html`, so the header and the
footer have one copy. Release notes go through `website/build/render.mjs`, the live site's own
renderer, which needs `website/`'s dependencies installed (`npm ci` in `website/`). `GITHUB_TOKEN`,
if set, raises the API's rate limit.

## Regenerating the shots

The shots are generated, not drawn. Build `omaweb-ui-lab`, point the script at a checkout of
Omarchy's `themes/` directory, and ask for the window alone:

```sh
scripts/build_website_themes.py --lab <path to omaweb-ui-lab> --themes <omarchy>/themes \
  --template-opacity --window-only website-next/assets/shots
```

`--window-only` writes the window with a transparent background and leaves `website/` untouched.
Re-run it after a chrome change or an upstream theme change. The page offers the themes its swatches
name, in `index.html`; a theme the script builds but no swatch names is never shown.
