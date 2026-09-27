# Website

The site at <https://omaweb.app>: one landing page and the release pages. The landing page is plain
HTML, CSS and scripts, served as they are, and it loads nothing from another origin, so it fits the
Content-Security-Policy `vercel.json` sends. `docs/development.md` has how it builds and deploys.

```sh
npm ci                              # once: the Markdown parser the release pages use
node build/site.mjs                 # write dist/, the site as it deploys
../scripts/serve_website.sh         # build without the network and serve dist/
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
  theme, and `themes.css`, each theme's colours for the frame the captures stand in.
- `build/` is the build: `site.mjs` copies the site into `dist/` and writes the release pages with
  `releases.mjs`, `release.html` and `render.mjs`.

## The release pages

`build/releases.mjs` reads every published release from GitHub, including the engine builds and the
package repository, and writes `dist/releases/index.html`, the newest browser release, and a page
per release at `dist/releases/<tag>/`. Each page is `index.html` with its `<main>` swapped for
`build/release.html`, so the header and the footer have one copy, and release notes go through
`build/render.mjs`, which decides what a body may become. `GITHUB_TOKEN`, if set, raises the API's
rate limit.

## Regenerating the shots

The shots are generated, not drawn. Build `omaweb-ui-lab` and point the script at a checkout of
Omarchy's `themes/` directory:

```sh
scripts/build_website_themes.py --lab <path to omaweb-ui-lab> --themes <omarchy>/themes
```

Re-run it after a chrome change or an upstream theme change. The page offers the themes its swatches
name, in `index.html`; a theme the script builds but no swatch names is never shown.
