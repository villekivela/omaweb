# Handoff: regenerate the website shots (#204)

Branch `feat/204-matrix-rain-wallpaper`. Delete this file before merging.

## Done

- `scripts/build_website_themes.py` draws the wallpaper as blocky matrix rain, blurs the desktop
  under the window by default (`--no-blur` opts out), and takes `--themes DIR` for an Omarchy
  checkout's `themes/` directory.
- `website/`: a `<canvas class="t-rain">` behind the header and hero draws the same rain in the
  active palette, faded and slowly falling. Verified in Chrome; theme switching repaints it.

## Left: the shots themselves

They must be rendered on Linux. The lab reads `Qt.platform.os`, so a macOS render labels shortcuts
`⌘` instead of `Ctrl+` and falls back to a sans face. On the Omarchy box:

```sh
cmake --build --preset dev --target omaweb-ui-lab
OMAWEB_CAPTURE_FONT_FAMILY="JetBrainsMono Nerd Font" \
  scripts/build_website_themes.py --lab ./build/dev/omaweb-ui-lab
```

The installed themes are read from `~/.config/omarchy/themes` and `/usr/share/omarchy/themes`.
Oligarchy is not in the Omarchy repository; pass its checkout's parent directory with `--themes` if
it is not installed. Then inspect a shot, commit `website/assets/shots`, `website/themes.css`,
`website/favicon.svg` and `website/assets/icons`, and open the PR.

## Open question

At the template's opacity (sheet 0.92, sidebar 0.95) the window reads as opaque over the mostly dark
wallpaper: probed page pixels differ by two levels between dense and empty ground. Decide whether
captures should lower opacity (an override next to `OMAWEB_CAPTURE_FONT_FAMILY` in
`render_theme_file`), or leave it honest.
