# The introductory film

The website opens on a film of Omaweb in use, after its hero. It is the real browser on its own
engine, recorded by a script in CI's Arch container, so a release's film is that release's browser.
The film is silent, about 40 seconds long, with its captions burned in. Each caption is up before
the action it names starts, so it is read first, and each beat shows what its caption claims. In
order:

1. The Omnibar. `tra` finds the trail shoe open in this Space, the tracing guide open in Work, and
   Kestrel's suggestions. The reader picks a suggestion.
1. Link hints on the search results: `f` puts a label on every link, and the camera closes in to
   read them.
1. A Space switch. The pointer rests on each Space's square in the footer, which names its Space,
   and the reader switches to Work. The footer stays on camera, so its colour is seen to change.
1. Work's split, two docs pages side by side, with the sidebar hidden so they take the whole window.
1. Blocking on a magazine, its ad slots empty. The camera closes on the blocked count and the page
   loads again under it, so the count is seen to climb. The ad network and the tracker are never
   reached.
1. A theme change on one Start page, from Retro 82 to Tokyo Night, across the whole browser.
1. An Agent at work in a Space of its own. Each command it runs, `omaweb space new`, `open`, `look`
   and one batched `do`, takes the caption's place as it runs, and the invoice is sent.
1. The end card: the logo and the line that installs Omaweb.

The recording has no pointer. Its browser is built with `OMAWEB_FILM_HOOKS`, which adds one verb,
`omaweb film-hover`: the window holds a Space's name on show through the footer button's own hover
state and answers where the button is. The edit then draws `pointer.png` there, gliding between the
squares. `pointer.svg` is its source. No shipped build has the verb.

## Recording it

```sh
scripts/record_film.sh
```

writes `omaweb.webm`, `omaweb.mp4`, `poster.webp` and `captions.vtt` to `build/film/`, from the
commit checked out. Re-record it for each release. The script needs Docker on the host and nothing
else. It builds the browser in the container against `omaweb-qtwebengine`, as CI's `arch-linux` job
does, and runs it under headless cage. On an arm64 machine, name an arm64 image and limit the CPUs
if the build runs out of memory:

```sh
OMAWEB_FILM_IMAGE=lopsided/archlinux:devel OMAWEB_FILM_DOCKER='--cpuset-cpus 0-5' \
    scripts/record_film.sh
```

On OrbStack, pacman cannot apply its Landlock sandbox inside a container, so the engine's install
fails with "Landlock ruleset could not be applied". The script installs as CI does and does not turn
the sandbox off; on OrbStack, set `DisableSandboxFilesystem` in the container's `/etc/pacman.conf`
yourself before the install, or record on a Linux host.

Each beat is checked as it plays, and a beat that did not happen stops the run before anything is
encoded: a Space not on show, a tab at the wrong address, a page that did not widen, an ad that was
fetched, link hints that never came up, chrome that kept its colour, or an Agent command that
failed. The browser's own log is left in `build/film/browser.log`. The encoder steps each file's
quality down until it fits its share of the 5 MB budget, and the run fails if one still does not.
The finished MP4 is then read frame by frame with ffmpeg's `signalstats`, and the run fails if any
frame's mean brightness is over 120 of 255 or more than 18 above the frames around it, which is a
white frame flashing on a dark site.

The WebM is VP9 rather than AV1, because the aarch64 Arch ffmpeg has no software AV1 encoder.
Browsers that play AV1 in WebM play VP9 too.

## What it browses

`sites/` holds the pages, one directory per host: Fernwood, an outdoor store; Quillstack, a
developer docs site; The Halyard, a magazine with ads; Tallyhaus, an invoicing dashboard; Kestrel,
the search engine; and AdSprout and PixelMint, the magazine's ad network and tracker. Every name is
under `.test`, which no real site can hold. The container's hosts file points them at loopback, and
the script serves them itself. A request for any other name fails the run. Docker writes the hosts
file afresh when a container starts, so a recording rerun in a restarted container stops before the
browser starts and names the sites it lost.

The sites are dark, as the website is, so the film reads as one piece with the page around it. The
trail shoe and the lighthouse are photographs supplied for the film by the project's owner, kept as
WebP beside the pages that show them.

`sites/_film/report.js` is on every page. It tells the script where the page is, how wide its
viewport is, how many ad slots it drew and how many link hints are on show. `sites/_film/end.html`
is the end card, which every site serves at `/film/end/`. `themes/` holds the two Omarchy palettes
the film switches between, rendered through Omaweb's own template as Omarchy would render them.

`tests/scripts/tst_record_film.py` holds the sites to reaching nothing outside the film, and holds
the server and each beat check to the answers a broken beat would give.

## Publishing it

The files are not kept in the repository. They are the assets of the `film` release, and the
website's build (`website/build/film.mjs`) copies them into the site, so the page plays them from
its own address and its Content-Security-Policy stays `default-src 'self'`. The build fails when one
is missing. Watch a recording before uploading it, then replace the release's copies:

```sh
gh release upload film build/film/omaweb.webm build/film/omaweb.mp4 build/film/poster.webp \
    --clobber
```

The release is made once, before the first upload, and kept off the latest release so the engine and
the browser keep that place:

```sh
gh release create film --title "Introductory film" --latest=false \
    --notes "The website's introductory film. film/README.md says how it is recorded."
```

The live site picks a new upload up at its next build. To preview the film locally, copy the three
files into `website/assets/film/`, where Git ignores them, and run `scripts/serve_website.sh`.
