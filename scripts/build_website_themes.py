#!/usr/bin/env python3
"""Capture the website's interface shots, one set per Omarchy theme.

The website shows the real browser window, in one Omarchy theme at a time, and
lets the reader switch. The captures are generated here, from each theme's own
`colors.toml`, so a chrome change or an upstream palette change is a re-run
rather than a photo session and a round of eyedropping.

It writes, into `website/assets/shots/` or the directory named with `--out`:

- `<theme>/<state>.webp`, one capture per interface state: the window alone,
  with the alpha it was captured at and nothing drawn around it, since the page
  stands it on a ground of its own.
- `themes.css`, each theme's palette roles on whichever element carries
  `data-theme`, so the page themes the frame and not itself, and `--border`,
  the colour the theme's Hyprland draws the active window's border in. The
  roles are the ones Omaweb itself resolved rather than an approximation of
  them: a theme file names a handful of colours, Omaweb derives every role it
  draws from them and floors the quiet ones against the ground they sit on, so
  the roles are read back out of the browser through `--dump-palette`.

Which themes the page offers is the page's own choice, in its swatches; a theme
built here that no swatch names is never shown.

## Headless, and no pointer

Nothing here needs a compositor, a pointer or a screen grab:

    QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \\
      ./build/dev/omaweb-ui-lab --tabs --show settings:tabs --capture out.png

`OMAWEB_THEME_FILE` points the lab at a rendered theme without installing it,
and `OMAWEB_NO_OMARCHY_TEMPLATE` stops the run writing into the reader's own
Omarchy configuration. The capture keeps real alpha: the chrome comes out at
its theme's opacity, as it stands on a desktop.

## The page in the shots is drawn by the lab

`omaweb-ui-lab` runs no engine, so where a webpage would be it draws a stand-in.
The states that show the browser in use pass `--browse`, which ends the seeded
day on a photo essay the stand-in draws in the page palette; the one that does
not shows Settings, which covers the page anyway. Every state is therefore
real chrome over a drawn page, captured headlessly, and a shot of a real site
would need the browser on a live compositor and `grim`, which gives up every
property above.

## Running it

    scripts/build_website_themes.py --lab ./build/dev/omaweb-ui-lab \\
      --themes <a checkout of Omarchy>/themes

Captures are set in JetBrains Mono, and the run stops if Qt resolves anything
else: a shot in the machine's fallback monospace is wrong in a way nothing
downstream catches. Install the family, or name another installed one with
`OMAWEB_CAPTURE_FONT_FAMILY`. `OMAWEB_CAPTURE_FONT_FILE` is ignored by this
script.

The one thing to install is a WebP encoder: `cwebp` from libwebp, or
ImageMagick. Everything else is the standard library.

Captures are taken at twice the size the page draws them, so the browser's own
type is rendered at two device pixels per logical one rather than resampled.
That costs bytes, and the captures are encoded lossless anyway: each is also
the file a reader opens full size.

Themes are read from `/usr/share/omarchy/themes` and `~/.config/omarchy/themes`,
the user directory winning, and from any directory named with `--themes`, which
wins over both: a checkout of the Omarchy repository's `themes/` serves on a
machine that has no Omarchy installed. A theme found in none of them is skipped
with a word about it. Nothing is written outside this repository unless `--out`
says so.

The images are deliberately not gated in CI. Rendering differs across machines
and fonts, and a flaky gate on a picture is worse than a stale picture.
"""

from __future__ import annotations

import argparse
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
import tomllib

ROOT = pathlib.Path(__file__).resolve().parent.parent
TEMPLATE = ROOT / "integrations" / "omarchy" / "omaweb.json.tpl"
SHOTS = ROOT / "website" / "assets" / "shots"
CAPTURE_FONT_FAMILY = os.environ.get("OMAWEB_CAPTURE_FONT_FAMILY", "").strip() or "JetBrains Mono"

THEME_DIRECTORIES = [
    pathlib.Path.home() / ".config" / "omarchy" / "themes",
    pathlib.Path("/usr/share/omarchy/themes"),
]

# The themes this can build, in the order the page offers them: warm first, to
# sit with the page's own palette. The page names which it shows.
THEMES = [
    ("matte-black", "Matte Black"),
    ("ristretto", "Ristretto"),
    ("retro-82", "Retro 82"),
    ("gruvbox", "Gruvbox"),
    ("everforest", "Everforest"),
    ("catppuccin", "Catppuccin"),
    ("tokyo-night", "Tokyo Night"),
    ("nord", "Nord"),
    ("hackerman", "Hackerman"),
]

# The interface states worth a picture, and the lab arguments that reach each.
# `--tabs` seeds every one of them: a Space at rest draws neither the Pinned
# section nor the tab list, and the vertical strip with Pinned tabs above it is
# what distinguishes this browser at a glance. `--browse` ends on a page rather
# than the blank tab, whose Start page is the shortcut sheet, so the browser is
# shown in use; `--spaces` adds the Work Space beside Personal that the tour's
# first step is about; `--sample-lists` shows the filter lists a first run has.
# The Omnibar is shown with a query typed, so it lists what two letters find
# across a web search, the open tabs and the Space's history, rather than the
# address it opens on.
STATES = [
    ("space", ["--tabs", "--spaces", "--browse"]),
    ("collapsed", ["--tabs", "--browse", "--show", "collapsed"]),
    (
        "omnibar",
        ["--tabs", "--spaces", "--browse", "--show", "omnibar-settled", "--omnibar-query", "ar"],
    ),
    ("blocking", ["--tabs", "--sample-lists", "--show", "settings:content-blocking"]),
]

# Two states the lab can reach and this deliberately does not ship. Site
# information is about the page on show and the lab has no page, so it draws
# the panel saying so. `--private` paints the private palette, which is the
# theme's own ground with its private accent cast over it: a difference the
# browser is right to keep quiet and a screenshot cannot carry, and the shot
# comes out as the ordinary window with its Pinned section missing. Both are
# one argument away for anyone who wants to look.

# The palette roles the website spends, taken from the palette Omaweb resolved.
# The opaque grounds rather than the translucent ones: what the page draws is
# flat, and the translucency a theme names is already in the captures, which
# keep their alpha.
RESOLVED_ROLES = {
    "--bg": "windowOpaque",
    "--sidebar": "sidebarOpaque",
    "--fg": "text",
    "--accent": "accent",
    "--urgent": "urgent",
}

# And the roles the page takes from the theme's own name for them instead.
# Omaweb floors quiet text for contrast against every ground it draws it on,
# one of which is a hover fill the template spends the theme's own `muted` on;
# a theme whose `muted` is a mid-grey pushes that floor to near-white, which is
# the right answer for a browser drawing text on six grounds and the wrong one
# for a page with a single ground, where it would read brighter than the
# foreground it is meant to be quieter than.
NAMED_ROLES = {"--muted": "mutedText"}

# Captured at twice the size Qt would lay the window out at, so the type is
# rendered at two device pixels per logical one rather than resampled down to
# them. The page draws the lead shot at around 1200 CSS pixels; a capture at
# the same width lands the browser's 12px body type at nine, which is what
# made the shots read as soft. Qt scales the whole layout, so the shot is the
# same window at the same proportions and only the pixel count changes.
SCALE = 2


# ------------------------------------------------------------------ themes


def theme_colors(name: str) -> dict[str, str] | None:
    """The theme's own `colors.toml`, or None where it is not installed."""
    for directory in THEME_DIRECTORIES:
        colors = directory / name / "colors.toml"
        if colors.is_file():
            with colors.open("rb") as handle:
                return {
                    key: value
                    for key, value in tomllib.load(handle).items()
                    if isinstance(value, str)
                }
    return None


def render_theme_file(
    colors: dict[str, str], target: pathlib.Path, opacity: dict[str, float] | None
) -> dict:
    """Substitute the shipped Omarchy template against one theme's palette.

    Omarchy renders this itself at theme-switch time, into its own state
    directory. A screenshot run cannot switch the reader's desktop six times,
    so it renders the same template here instead.

    The template names only `colors.toml` keys, so every token here is a colour
    this theme has to define. The family list is names rather than tokens, and
    the browser resolves it the same way a desktop's own palette is resolved.
    """
    rendered = TEMPLATE.read_text(encoding="utf-8")
    for key, value in colors.items():
        rendered = rendered.replace("{{ %s }}" % key, value)
    remaining = re.findall(r"\{\{[^}]*\}\}", rendered)
    if remaining:
        raise SystemExit(f"the template names colours this theme does not: {remaining}")
    theme = json.loads(rendered)
    families = theme["font"]["families"]
    theme["font"]["families"] = [CAPTURE_FONT_FAMILY] + [
        fallback for fallback in families if fallback != CAPTURE_FONT_FAMILY
    ]
    if opacity:
        theme["opacity"].update(opacity)
    target.write_text(json.dumps(theme, indent=2) + "\n", encoding="utf-8")
    return theme


def resolved_palette(lab: pathlib.Path, theme_file: pathlib.Path) -> dict:
    """The palette Omaweb draws, read back out of the browser itself."""
    with tempfile.TemporaryDirectory() as scratch:
        dump = pathlib.Path(scratch) / "palette.json"
        run_lab(lab, theme_file, ["--dump-palette", str(dump), "--validate-qml"])
        palette = json.loads(dump.read_text(encoding="utf-8"))
        resolved = palette.get("font", {}).get("family")
        if resolved != CAPTURE_FONT_FAMILY:
            raise SystemExit(
                f"captures are set in {CAPTURE_FONT_FAMILY}, but Qt resolved {resolved!r}; "
                "install it, or name an installed family with OMAWEB_CAPTURE_FONT_FAMILY"
            )
        return palette


def run_lab(lab: pathlib.Path, theme_file: pathlib.Path, arguments: list[str]) -> None:
    environment = dict(os.environ)
    # Website captures use installed fonts, even if the lab has a font-file override.
    environment.pop("OMAWEB_CAPTURE_FONT_FILE", None)
    environment.update(
        {
            "OMAWEB_THEME_FILE": str(theme_file),
            # Without this, following the Omarchy theme writes a template into
            # the reader's own configuration directory.
            "OMAWEB_NO_OMARCHY_TEMPLATE": "1",
            "QT_QPA_PLATFORM": "offscreen",
            "QT_QUICK_BACKEND": "software",
            "QT_SCALE_FACTOR": str(SCALE),
        }
    )
    result = subprocess.run(
        [str(lab), *arguments], env=environment, capture_output=True, text=True
    )
    if result.returncode != 0:
        sys.exit(f"{lab.name} {' '.join(arguments)} failed:\n{result.stderr}")


# --------------------------------------------------------------- colours


def channels(color: str) -> tuple[int, int, int]:
    """`#rrggbb` or Qt's `#aarrggbb`, as opaque RGB."""
    digits = color.lstrip("#")
    if len(digits) == 8:
        digits = digits[2:]
    if len(digits) != 6:
        raise SystemExit(f"not a colour this script can read: {color}")
    return tuple(int(digits[index : index + 2], 16) for index in (0, 2, 4))


def hex_color(color: str) -> str:
    red, green, blue = channels(color)
    return f"#{red:02x}{green:02x}{blue:02x}"


def active_border_stops(
    value: str | None, fallback: tuple[int, int, int]
) -> list[tuple[tuple[int, int, int], float]]:
    """RGB stops and opacity from an Omarchy active-border declaration."""
    stops = []
    for rgb, opacity in re.findall(r"(?:#|rgba\()?([0-9a-fA-F]{6})([0-9a-fA-F]{2})?", value or ""):
        stops.append((channels(rgb), int(opacity, 16) / 255 if opacity else 1.0))
    return stops or [(fallback, 1.0)]


# ---------------------------------------------------------------- encode


def find_encoder() -> str | None:
    """The tool that turns a PNG on stdin into WebP on stdout.

    WebP because the captures are taken at twice the size the page draws them,
    and lossless WebP is a third of the PNG. `cwebp` is libwebp's own tool;
    ImageMagick is the fallback because a machine that renders this site tends
    to have it already.
    """
    for name in ("cwebp", "magick", "convert"):
        if shutil.which(name):
            return name
    return None


def encoding(encoder: str) -> list[str]:
    """The command line for one encode.

    Lossless: the capture is the file a reader opens to read the type in.
    """
    if encoder == "cwebp":
        # -z 9 is the slowest and smallest of the lossless presets.
        return ["cwebp", "-quiet", "-lossless", "-z", "9", "-o", "-", "--", "-"]
    # ImageMagick reads its operators between the input and the output.
    return [encoder, "png:-", "-define", "webp:lossless=true", "webp:-"]


def write_webp(path: pathlib.Path, encoder: str, png: bytes) -> None:
    """Encode one composite."""
    command = encoding(encoder)
    result = subprocess.run(command, input=png, capture_output=True)
    if result.returncode != 0 or not result.stdout:
        sys.exit(f"{encoder} could not encode {path.name}: {result.stderr.decode().strip()}")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(result.stdout)


# ------------------------------------------------------------------ assets


def stylesheet(palettes: dict[str, tuple[dict, dict]], borders: dict[str, str]) -> str:
    """Each theme's roles, on any element rather than the body."""
    lines = [
        "/* Generated by scripts/build_website_themes.py. Do not edit.",
        " *",
        " * Each theme's roles, as Omaweb resolved them, on whichever element carries",
        " * data-theme, and --border, the theme's own active-window border colour.",
        " */",
        "",
    ]
    for name, _ in THEMES:
        if name not in palettes:
            continue
        palette, named = palettes[name]
        lines.append(f'[data-theme="{name}"] {{')
        for token, role in RESOLVED_ROLES.items():
            lines.append(f"  {token}: {hex_color(palette[role])};")
        for token, role in NAMED_ROLES.items():
            lines.append(f"  {token}: {hex_color(named[role])};")
        lines.append(f"  --border: {borders[name]};")
        lines.append("}")
        lines.append("")
    return "\n".join(lines)


# -------------------------------------------------------------------- main


def build(
    theme: str, lab: pathlib.Path, scratch: pathlib.Path, encoder: str, out: pathlib.Path
) -> tuple[dict, dict, str] | None:
    colors = theme_colors(theme)
    if colors is None:
        print(f"  {theme}: not installed, skipped")
        return None

    theme_file = scratch / f"{theme}.json"
    named = render_theme_file(colors, theme_file, None)
    palette = resolved_palette(lab, theme_file)

    for state, arguments in STATES:
        capture = scratch / f"{theme}-{state}.png"
        run_lab(lab, theme_file, [*arguments, "--capture", str(capture)])
        write_webp(out / theme / f"{state}.webp", encoder, capture.read_bytes())
        print(f"  {theme}/{state}.webp")

    # The first stop of the theme's own active-border gradient, or its accent
    # where it names none: the page draws the frame one colour.
    stops = active_border_stops(colors.get("hyprland_active_border"), channels(palette["accent"]))
    return palette, named, "#%02x%02x%02x" % stops[0][0]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--lab",
        type=pathlib.Path,
        default=ROOT / "build" / "dev" / "omaweb-ui-lab",
        help="the omaweb-ui-lab binary to capture with",
    )
    parser.add_argument("--theme", action="append", help="build only this theme; repeatable")
    parser.add_argument(
        "--themes",
        action="append",
        type=pathlib.Path,
        default=[],
        help="a directory of Omarchy themes to read before the installed ones; repeatable",
    )
    parser.add_argument(
        "--out",
        type=pathlib.Path,
        default=SHOTS,
        metavar="DIR",
        help="where to write the captures and themes.css (default: website/assets/shots)",
    )
    arguments = parser.parse_args()

    if not arguments.lab.is_file():
        return fail(f"no lab at {arguments.lab}; build the dev or ui preset first")
    encoder = find_encoder()
    if encoder is None:
        return fail(
            "no WebP encoder found; install libwebp for cwebp, or imagemagick.\n"
            "The page asks for .webp, so writing PNG here would produce files it "
            "cannot load."
        )
    THEME_DIRECTORIES[:0] = arguments.themes
    if not any(directory.is_dir() for directory in THEME_DIRECTORIES):
        print("No Omarchy themes on this machine; nothing to build.")
        return 0

    wanted = [entry for entry in THEMES if not arguments.theme or entry[0] in arguments.theme]
    if not wanted:
        return fail(f"no such theme: {', '.join(arguments.theme)}")

    built = {}
    with tempfile.TemporaryDirectory() as directory:
        scratch = pathlib.Path(directory)
        for theme, _ in wanted:
            print(f"{theme}:")
            result = build(theme, arguments.lab, scratch, encoder, arguments.out)
            if result is not None:
                built[theme] = result

    if not built:
        return fail("none of the requested themes are installed")

    roles = {name: (palette, named) for name, (palette, named, _) in built.items()}
    borders = {name: border for name, (_, _, border) in built.items()}
    target = arguments.out / "themes.css"
    target.write_text(stylesheet(roles, borders), encoding="utf-8")
    print(f"  {target}")
    return 0


def fail(message: str) -> int:
    print(message, file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
