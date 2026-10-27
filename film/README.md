# The introductory film

The website opens on a film of Omaweb in use, after its hero. It is the real browser on its own
engine, recorded by a script, so a release's film is that release's browser. The release's film is
recorded on the maintainer's Omarchy desktop, so it shows Hyprland's blur behind the translucent
window and the wallpaper through it. A machine without that desktop records it in CI's Arch
container. The film is silent, about 40 seconds long, with its captions burned in. Each caption is
up before the action it names starts, so it is read first, and each beat shows what its caption
claims. In order:

1. The Omnibar. `tra` finds the trail shoe open in this Space, the tracing guide open in Work, and
   Kestrel's suggestions. The reader picks a suggestion.
1. Link hints on the search results: `f` puts a label on every link, and the camera closes in to
   read them.
1. A Space switch. The pointer rests on each Space's square in the footer, which names its Space,
   and the reader switches to Work. The footer stays on camera, so its colour is seen to change.
1. Work's split, two docs pages side by side, with the sidebar hidden so they take the whole window.
1. Blocking on a magazine, which loads again with its ad slots empty. The pointer clicks the Site
   information button, Site information opens, and the pointer opens its blocked tile. The camera
   closes on the card, whose list of refused requests names the ad network and the tracker, and
   holds it long enough to read. Neither is ever reached.
1. A theme change on one Start page, across the whole browser: from Retro 82 to Tokyo Night in the
   container, and on the desktop the desktop's own theme switched to Lumon and back.
1. An Agent at work in a Space of its own. Each command it runs, `omaweb space new`, `open`, `look`
   and one batched `do`, takes the caption's place as it runs, its tab's row in the sidebar names
   its last act after each, and the invoice is sent. The camera then closes on the row, the pointer
   rests on it so Stop shows beside Close, and a click on Stop hands the tab back to the reader.
1. The end card: the logo and the line that installs Omaweb.

The recording has no pointer. Its browser is built with `OMAWEB_FILM_HOOKS`, which adds three verbs.
`omaweb film-hover` has the window hold a Space's name on show through the footer button's own hover
state and answer where the button is. `omaweb film-row <tab>` has it hold an Agent tab's row as
hovered, so Stop shows, and answer where Stop is and what the row says; `--where` only answers, and
`--stop` presses Stop as a click would. `omaweb film-site button` and `omaweb film-site blocked`
answer where the Site information button and the card's blocked tile are, and `--press` clicks them;
with neither, it answers whether the card is open, at which detail, what its blocked list names and
where the card is, which the camera frames. The edit draws `pointer.png` where the window answered,
gliding between the places. `pointer.svg` is its source. No shipped build has any of them.

## Recording it on the desktop

```sh
scripts/record_film.py desktop
```

records the film on the Hyprland session it is run from, with the same beats and the same edit as
the container. It needs a clean checkout, since the film is of the commit, and `wf-recorder`,
`wtype`, `grim`, `ffmpeg` and the fonts the container installs; it names any that are missing with
the line that installs them. Then it:

1. Builds the film's browser (`OMAWEB_FILM_HOOKS`) into `build/film-desktop` against Omaweb's engine
   under `/usr/lib/omaweb`, with two jobs.
1. Says that it takes the keyboard for the run, and waits for Enter.
1. Holds notifications (`omarchy-shell notifications setDnd on`, or a mako `do-not-disturb` mode
   where there is no Omarchy shell), moves the pointer to a bottom corner of the monitor, and
   switches to a workspace of its own, `omaweb-film`.
1. Starts the browser there with a home, profile, Agent socket and D-Bus of its own under a
   temporary directory, wearing the reader's theme (below), floats its window and sizes it through
   `hyprctl` to 1600 by 900 logical pixels in the middle of the monitor, the browser's size under
   cage. The monitor's mode is not changed.
1. Records only the window, with `wf-recorder -g`, so the bar, other windows and other monitors stay
   out, and cuts the film as the container does, into `build/film/`.

The desktop film wears the reader's theme: the one the desktop is wearing when the run starts. The
film's browser finds Omarchy's `~/.local/state/omarchy/current` through its own home and follows it
as the reader's Omaweb does. The Theme beat switches the desktop's theme, as a reader does, with
`omarchy-theme-set lumon`, then switches it back to the theme the run started with, so the beat
shows the real switch, wallpaper and blur included, and most of the film is in the reader's theme.
Notifications are already held, so the switch raises none on camera. The beat checks the chrome
against each theme's `colors.toml`.

```sh
scripts/record_film.py desktop --change-to gruvbox
```

switches to another installed Omarchy theme, looked for in `~/.config/omarchy/themes` and then
`/usr/share/omarchy/themes`. A theme whose sidebar colour is too near the desktop's for the check to
tell apart is refused before the run changes anything, and the refusal names one that is far enough.
A desktop with no current Omarchy theme, or without `omarchy-theme-set`, stops the run there too.
The theme the run started with is in the ledger before the first switch, and every way out puts it
back: the run ending, a missed beat, Ctrl+C, and the next run's recovery after a killed one.

The film's sidebar is as opaque as the reader's own. The run reads `sidebar-opacity` from the
reader's Omaweb settings, `settings.json` under `$OMAWEB_CONFIG_ROOT`, `$XDG_CONFIG_HOME/omaweb` or
`~/.config/omaweb`, and sets it in the film's profile. Where the reader set none, or the file is
missing or unreadable, the film keeps the theme's opacity. How much of Hyprland's blur shows depends
on the wallpaper behind the window: over a dark one, the sidebar can still read as solid.

```sh
scripts/record_film.py desktop --workspace 9
```

records in workspace 9 in place of `omaweb-film`, by number or by name. The workspace has to be
empty: a run whose workspace holds a window stops before it changes anything, since the film must
not cover or move the reader's windows. The run comes back to the workspace it started on.

Before each key it sends, the run asks Hyprland which window has the keyboard. If it is not the
film's, because the pointer was nudged onto another window or a window took focus, the run stops
there, says which window has it, and puts the desktop back, so no key reaches anything of the
reader's.

Whether the run ends, fails, or is stopped with Ctrl+C or by its terminal closing, it stops the
film's browser and the recorder, switches back to the workspace it started on, puts the pointer back
and restores notifications as they were. The reader's own Omaweb is never started, and of its
profile only the sidebar opacity in `settings.json` is read, never written. A monitor at Hyprland's
scale of 1.5 records the window at cage's 2400 by 1350; at another scale the edit scales the
recording to that size.

Hyprland 0.56 takes its dispatchers as Lua (`hl.dsp`), as Omarchy 4's scripts send them, and answers
one it could not do with an error rather than `ok`. After Enter, and before it changes anything, the
run asks for two things that change nothing in the same forms: the pointer moved to where it is, and
the keyboard given to the window that has it. A Hyprland that refuses either stops the run there.

```sh
scripts/record_film.py desktop --dry-run
```

builds nothing and changes nothing. It reads the monitor, the workspace, the pointer, the
notification state, the desktop's theme and the reader's sidebar opacity, then prints each command
the run would issue, in order, the theme switches among them, and what it would have stopped on.

### If a run is killed

A run killed outright (`kill -9`, or a crash) cannot put anything back. It keeps a ledger in
`build/film/desktop-run.json` from before it changes anything until it has put everything back, and
the next `scripts/record_film.py desktop`, once Enter is pressed, stops the film's browser and the
recorder that ledger names, if those process numbers are still theirs, and restores notifications as
that run found them. The ledger names the film's workspace and the desktop's theme too: the next run
switches the theme back if it is not the one the killed run started with, and switches back to the
workspace the killed run started on if the film's is still on show. To put things back by hand
instead:

```sh
pkill -INT -x wf-recorder                 # the recorder, which otherwise fills the disk
pkill -f build/film-desktop/omaweb-browser # the film's browser, never the installed one
omarchy-shell notifications setDnd off     # or makoctl mode -r do-not-disturb
omarchy-theme-set "$(jq -r .theme build/film/desktop-run.json)" # if the ledger names a theme
rm build/film/desktop-run.json
```

Then switch back to the workspace you were on. The `omaweb-film` workspace goes once its window has;
a workspace given with `--workspace` is left empty.

## Recording it in a container

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

The script gives the container 1 GiB of `/dev/shm`, where the browser's processes share their pages.
Docker's default is 64 MB, and in it the magazine's renderer is killed with exit code 9 at its first
load. The recording checks for 256 MB free before the browser starts, so a container with too little
stops with a sentence that names `/dev/shm`.

Each beat is checked as it plays, and a beat that did not happen stops the run before anything is
encoded: a Space not on show, a tab at the wrong address, a page that did not widen, an ad that was
fetched, a blocked list that is not on show or does not name both, link hints that never came up,
chrome that kept its colour, an Agent command that failed, an Agent's row that did not name each new
act, or a tab still the Agent's after Stop. The browser's own log is left in
`build/film/browser.log`. The encoder steps each file's quality down until it fits its share of the
5 MB budget, and the run fails if one still does not. The finished MP4 is then read frame by frame
with ffmpeg's `signalstats`, and the run fails if any frame's mean brightness is over 120 of 255 or
more than 18 above the frames around it, which is a white frame flashing on a dark site.

The WebM is VP9 rather than AV1, because the aarch64 Arch ffmpeg has no software AV1 encoder.
Browsers that play AV1 in WebM play VP9 too.

## What it browses

`sites/` holds the pages, one directory per host: Fernwood, an outdoor store; Quillstack, a
developer docs site; The Halyard, a magazine with ads; Tallyhaus, an invoicing dashboard; Kestrel,
the search engine; and AdSprout and PixelMint, the magazine's ad network and tracker. Every name is
under `.test`, which no real site can hold. The container's hosts file points them at loopback, and
the script serves them itself. On the desktop, a Chromium host resolver rule sends them to the
script's server instead, and resolves every other name to nothing. Kestrel's suggestions are the
exception: the browser asks for them itself, through Qt's network stack, which resolves names
through the system and never sees that rule. On the desktop the profile names Kestrel's suggestions
by the server's address and port, and the server answers that address as Kestrel. A request for any
other name fails the run. Docker writes the hosts file afresh when a container starts, so a
recording rerun in a restarted container stops before the browser starts and names the sites it
lost.

The sites are dark, as the website is, so the film reads as one piece with the page around it. The
trail shoe, the daypack, the tent and the lighthouse are photographs supplied for the film by the
project's owner, kept as WebP beside the pages that show them.

`sites/_film/report.js` is on every page. It tells the script where the page is, how wide its
viewport is, how many ad slots it drew and how many link hints are on show. `sites/_film/end.html`
is the end card, which every site serves at `/film/end/`. `themes/` holds the two Omarchy palettes
the container's film switches between, rendered through Omaweb's own template as Omarchy would
render them.

`tests/scripts/tst_record_film.py` holds the sites to reaching nothing outside the film, and holds
the server and each beat check to the answers a broken beat would give. It holds the desktop mode to
the commands it runs, to the window's place, and to putting the desktop back after a run that ended,
failed or was stopped.

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
