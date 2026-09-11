#!/usr/bin/env python3
"""Render the website's per-theme assets: palettes, wallpapers, favicons, shots.

The website draws the browser in one palette at a time and lets the reader
switch. Everything that palette touches is generated here, from the Omarchy
theme's own `colors.toml`, so a chrome change or an upstream palette change is
a re-run rather than a photo session and a round of eyedropping.

Per theme it writes:

- `website/themes.css` -- one `body[data-theme]` block, holding the palette
  roles Omaweb itself resolved rather than an approximation of them. A theme
  file names a handful of colours; Omaweb derives every role it draws from
  them and floors the quiet ones against the ground they sit on, so the roles
  are read back out of the browser through `omaweb-ui-lab --dump-palette`.
- `website/assets/icons/favicon-<theme>.svg`, the application icon in the
  theme's own ground and foreground, and `wordmark-<theme>.svg`, the wordmark
  in its foreground. The default theme's favicon is also written to
  `website/favicon.svg`, which is where a browser asks for it.
- `website/assets/shots/<theme>/<state>.webp`, one capture per interface state,
  composited over a wallpaper generated from the same palette, and a
  `<state>-thumb.webp` beside it for the grid that opens it.

The wallpaper is one drawing in every palette, colours only: square cells on a
coarse grid, raining from the top edge and thinning out as they fall, in a few
quantised tints of the theme's accent. Each capture takes the active-window
border from the same theme, and the desktop under the window is blurred, as a
compositor with blur on shows it. The window's surfaces are captured a step more
translucent than the template ships them, so the blurred desktop reads through
them as frosted glass rather than as a two-level tint; `--template-opacity`
keeps the shipped values. Keeping one drawing across the set lets the
themes read as one family without asking any of them for a wallpaper of its
own, and the website draws the same picture live behind its hero.

## Headless, and no pointer

Nothing here needs a compositor, a pointer or a screen grab:

    QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \\
      ./build/dev/omaweb-ui-lab --tabs --show settings:tabs --capture out.png

`OMAWEB_THEME_FILE` points the lab at a rendered theme without installing it,
and `OMAWEB_NO_OMARCHY_TEMPLATE` stops the run writing into the reader's own
Omarchy configuration. The capture keeps real alpha -- the chrome comes out at
its theme's opacity -- which is what makes compositing over a wallpaper give
what a desktop shows rather than a flat approximation of it.

## The page in the shots is the Start page

`omaweb-ui-lab` runs no engine, so where a webpage would be it draws the Start
page. Every state captured here is therefore real chrome: a Space with a day's
worth of tabs and a new tab on show, two sections of Settings, History, and the
window with its sidebar hidden. A shot with a real page in it needs the browser
on a live compositor and `grim`, which gives up every property above and means
whoever runs it is capturing their own screen. If the site ever wants one hero
shot with a page in it, take that one by hand, once: it is the exception, and
this is why.

## Running it

    scripts/build_website_themes.py --lab ./build/dev/omaweb-ui-lab

Captures are set in JetBrains Mono, the type the website itself is set in,
and the run stops if Qt resolves anything else: a shot in the machine's
fallback monospace is wrong in a way nothing downstream catches. Install the
family, or name another installed one with `OMAWEB_CAPTURE_FONT_FAMILY`:

    OMAWEB_CAPTURE_FONT_FAMILY="JetBrainsMono Nerd Font" \\
      scripts/build_website_themes.py --lab ./build/dev/omaweb-ui-lab

`OMAWEB_CAPTURE_FONT_FILE` is ignored by this script.

The one thing to install is a WebP encoder: `cwebp` from libwebp, or
ImageMagick. Everything else is the standard library.

Captures are taken at twice the size the page draws them, so the browser's own
type is rendered at two device pixels per logical one rather than resampled
down to nine. That costs bytes, and the full captures are encoded lossless
anyway: they are the file a reader opens to read the type in. The thumbnails
carry the saving instead -- lossy, and a quarter of what lossless wants for a
picture nobody reads.

Themes are read from `/usr/share/omarchy/themes` and `~/.config/omarchy/themes`,
the user directory winning, and from any directory named with `--themes`, which
wins over both: a checkout of the Omarchy repository's `themes/` serves on a
machine that has no Omarchy installed. A theme found in none of them is skipped
with a word about it. Nothing is written outside this repository.

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
import struct
import subprocess
import sys
import tempfile
import tomllib
import zlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
TEMPLATE = ROOT / "integrations" / "omarchy" / "omaweb.json.tpl"
APPLICATION_ICON = ROOT / "assets" / "icons" / "omaweb.svg"
WORDMARK = ROOT / "website" / "assets" / "icons" / "omaweb.svg"
GENERATED = "<!-- Generated by scripts/build_website_themes.py. Do not edit. -->"
WEBSITE = ROOT / "website"
SHOTS = WEBSITE / "assets" / "shots"
ICONS = WEBSITE / "assets" / "icons"
STYLESHEET = WEBSITE / "themes.css"
CAPTURE_FONT_FAMILY = os.environ.get("OMAWEB_CAPTURE_FONT_FAMILY", "").strip() or "JetBrains Mono"

THEME_DIRECTORIES = [
    pathlib.Path.home() / ".config" / "omarchy" / "themes",
    pathlib.Path("/usr/share/omarchy/themes"),
]

# The themes the site offers, in the order it offers them. The first is the
# default: the one the page comes up in and the one its favicon is.
THEMES = [
    ("hackerman", "Hackerman"),
    ("oligarchy", "Oligarchy"),
    ("tokyo-night", "Tokyo Night"),
    ("catppuccin", "Catppuccin"),
    ("nord", "Nord"),
    ("everforest", "Everforest"),
    ("gruvbox", "Gruvbox"),
]

# The interface states worth a picture, and the lab arguments that reach each.
# `--tabs` seeds every one of them: a Space at rest draws neither the Pinned
# section nor the tab list, and the vertical strip with Pinned tabs above it is
# what distinguishes this browser at a glance.
STATES = [
    ("space", ["--tabs"]),
    ("settings", ["--tabs", "--show", "settings:tabs"]),
    ("blocking", ["--tabs", "--show", "settings:content-blocking"]),
    ("history", ["--tabs", "--show", "history"]),
    ("collapsed", ["--tabs", "--show", "collapsed"]),
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
# flat, and the translucency a theme names is already resolved into the
# screenshots by the compositing step.
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

# The desktop the window is shown standing on, as a fraction of each capture
# axis. The wider sides leave room for the wallpaper to read as a desktop;
# the tighter top and bottom keep the browser itself large in the frame.
HORIZONTAL_MARGIN = 0.1
VERTICAL_MARGIN = 0.055

# The opacity the captures give the window's surfaces, in place of the
# template's own. The template ships the page sheet at 0.92 and the sidebar at
# 0.95, which over a dark wallpaper is a window that reads as opaque: the page
# ground under dense and under empty desktop probes two levels apart. These
# values let the blurred desktop through as a haze while the type stays on a
# ground dark enough to read.
CAPTURE_OPACITY = {"sheet": 0.8, "sidebar": 0.86}

# Captured at twice the size Qt would lay the window out at, so the type is
# rendered at two device pixels per logical one rather than resampled down to
# them. The page draws the lead shot at around 1200 CSS pixels; a capture at
# the same width lands the browser's 12px body type at nine, which is what
# made the shots read as soft. Qt scales the whole layout, so the shot is the
# same window at the same proportions and only the pixel count changes.
SCALE = 2

# What the grid under the lead shot draws. A whole window at 600 CSS pixels is
# unreadable however many pixels it holds, so those are thumbnails that open
# the full capture rather than shrunken copies pretending to be legible.
THUMBNAIL_WIDTH = 720


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


# ------------------------------------------------------------------- png


def read_png(path: pathlib.Path) -> tuple[int, int, bytearray]:
    """Width, height and RGBA bytes of an 8-bit non-interlaced PNG.

    Written out rather than taken from a library because this script has no
    dependencies to install: the captures are what Qt writes, which is 8-bit
    RGBA, and the wallpapers are what this script writes.
    """
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise SystemExit(f"{path} is not a PNG")
    width = height = depth = kind = 0
    compressed = bytearray()
    offset = 8
    while offset < len(data):
        (length,) = struct.unpack_from(">I", data, offset)
        name = data[offset + 4 : offset + 8]
        body = data[offset + 8 : offset + 8 + length]
        offset += 12 + length
        if name == b"IHDR":
            width, height, depth, kind, _, _, interlace = struct.unpack(">IIBBBBB", body)
            if depth != 8 or kind not in (2, 6) or interlace != 0:
                raise SystemExit(f"{path} is not an 8-bit non-interlaced RGB(A) PNG")
        elif name == b"IDAT":
            compressed += body
        elif name == b"IEND":
            break

    source = zlib.decompress(bytes(compressed))
    stride = width * (4 if kind == 6 else 3)
    step = 4 if kind == 6 else 3
    pixels = bytearray(width * height * 4)
    previous = bytearray(stride)
    at = 0
    for row in range(height):
        filter_kind = source[at]
        line = bytearray(source[at + 1 : at + 1 + stride])
        at += 1 + stride
        unfilter(filter_kind, line, previous, step)
        if kind == 6:
            pixels[row * width * 4 : (row + 1) * width * 4] = line
        else:
            base = row * width * 4
            for column in range(width):
                pixels[base + column * 4 : base + column * 4 + 3] = line[
                    column * 3 : column * 3 + 3
                ]
                pixels[base + column * 4 + 3] = 255
        previous = line
    return width, height, pixels


def unfilter(kind: int, line: bytearray, previous: bytearray, step: int) -> None:
    """PNG's five per-scanline filters, undone in place."""
    if kind == 0:
        return
    if kind == 1:
        for index in range(step, len(line)):
            line[index] = (line[index] + line[index - step]) & 0xFF
    elif kind == 2:
        for index in range(len(line)):
            line[index] = (line[index] + previous[index]) & 0xFF
    elif kind == 3:
        for index in range(len(line)):
            left = line[index - step] if index >= step else 0
            line[index] = (line[index] + ((left + previous[index]) >> 1)) & 0xFF
    elif kind == 4:
        for index in range(len(line)):
            left = line[index - step] if index >= step else 0
            up = previous[index]
            corner = previous[index - step] if index >= step else 0
            estimate = left + up - corner
            if abs(estimate - left) <= abs(estimate - up) and abs(estimate - left) <= abs(
                estimate - corner
            ):
                nearest = left
            elif abs(estimate - up) <= abs(estimate - corner):
                nearest = up
            else:
                nearest = corner
            line[index] = (line[index] + nearest) & 0xFF
    else:
        raise SystemExit(f"unknown PNG filter {kind}")


def png_bytes(width: int, height: int, pixels: bytes) -> bytes:
    """RGB bytes as an 8-bit PNG. Composites are opaque: the window's own
    translucency has already been resolved against the desktop behind it.

    Returned rather than written, because what is written is WebP and the
    encoder takes a PNG on its standard input. Compression level 1: this is a
    handoff to another process, not the file that ships.
    """
    raw = bytearray()
    stride = width * 3
    for row in range(height):
        raw.append(0)
        raw += pixels[row * stride : (row + 1) * stride]

    def chunk(name: bytes, body: bytes) -> bytes:
        return (
            struct.pack(">I", len(body))
            + name
            + body
            + struct.pack(">I", zlib.crc32(name + body) & 0xFFFFFFFF)
        )

    return (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(bytes(raw), 1))
        + chunk(b"IEND", b"")
    )


# ------------------------------------------------------------- wallpaper

# The wallpaper's grid, as fractions of the canvas width: one cell pitch per
# hundredth of the width, and a square filling a little over half of each
# pitch so the gaps read as a grid rather than a texture.
CELL_PITCH = 1 / 100
CELL_FILL = 0.58

# How much of the column count falls as an unbroken streak, and how far down
# the canvas those streaks reach before they break up like the rest.
STREAK_SHARE = 0.25
STREAK_SOLID_DEPTH = 0.75

# How far each cell rises from the ground it sits on. The website draws its
# hero canvas at a third of full opacity so the rain stays a backdrop to the
# type over it; painted at full tint, the same cells were the brightest thing
# on the page. Half keeps the columns legible through the blur behind the
# window while the window stays the brightest thing in the frame.
CELL_OPACITY = 0.5


def wallpaper(palette: dict, width: int, height: int) -> bytearray:
    """Square cells raining from the top edge, in this palette.

    Cells sit on a coarse grid and fall in columns: solid near the top, then
    breaking into dashes and thinning out with depth, each column reaching its
    own distance down. A quarter of the columns are streaks that hold together
    for most of their length, so long lines cut through the shorter speckle.
    Brightness is quantised to four accent tints rather than faded smoothly,
    which is what keeps the cells reading as blocks. The website draws the
    same picture behind its hero in `script.js`; a change here wants the same
    change there.

    Everything random is a hash of the cell's position, so a re-run gives the
    same picture and only the palette changes between themes.
    """
    background = channels(palette["windowOpaque"])
    foreground = channels(palette["text"])
    accent = channels(palette["accent"])

    # The theme's window ground pulled toward black, a touch lighter at the top.
    top = mix(background, (0, 0, 0), 0.45)
    bottom = mix(background, (0, 0, 0), 0.7)
    pixels = bytearray(width * height * 3)
    stride = width * 3
    for row in range(height):
        color = bytes(clamp(value) for value in mix(top, bottom, row / max(height - 1, 1)))
        pixels[row * stride : (row + 1) * stride] = color * width

    pitch = max(8, round(width * CELL_PITCH))
    cell = max(3, round(pitch * CELL_FILL))
    inset = (pitch - cell) // 2
    columns = width // pitch + 1
    rows = height // pitch + 1

    # A whisper of accent up to the accent itself, and a fifth tint, the
    # accent lifted toward the foreground, for a few leading cells on the top
    # row.
    tints = [
        mix(background, accent, 0.22),
        mix(background, accent, 0.42),
        mix(background, accent, 0.68),
        accent,
        mix(accent, foreground, 0.55),
    ]

    def paint(column: int, row: int, tint: tuple[float, float, float]) -> None:
        x = column * pitch + inset
        y = row * pitch + inset
        ground = mix(top, bottom, (y + cell / 2) / max(height - 1, 1))
        fill_rect(pixels, width, height, x, y, x + cell, y + cell, mix(ground, tint, CELL_OPACITY))

    # Each column's fall, weighted toward its neighbours so long and short
    # runs cluster a little, and biased short so the deep ones stand out.
    raw = [hash01(1, index) for index in range(columns + 4)]
    lengths = []
    for index in range(columns):
        noise = 0.55 * (raw[index + 1] + raw[index + 2] + raw[index + 3]) / 3 + 0.45 * raw[index + 2]
        lengths.append(0.06 + 0.8 * noise**1.8)
    for index in range(columns):
        if hash01(7, index) < STREAK_SHARE:
            lengths[index] = max(lengths[index], 0.55 + 0.45 * hash01(9, index))

    for column in range(columns):
        if hash01(2, column) < 0.12:
            continue
        fall = max(lengths[column] * rows, 1)
        streak = hash01(7, column) < STREAK_SHARE
        # Dropout is decided per run of a few cells rather than per cell, so
        # the breaks come as dashes.
        run = 1 + int(hash01(8, column) * 3)
        for row in range(rows):
            depth = row / fall
            if depth > 1.15:
                break
            depth = min(1.0, depth)
            survive = 1.0 - depth**0.9 * 0.9
            if streak and depth < STREAK_SOLID_DEPTH:
                survive = 1.0
            if hash01(3, column, row // run) > survive:
                continue
            brightness = (1 - depth) ** 1.1 * (0.65 + 0.5 * hash01(4, column, row))
            level = min(3, int(brightness * 4))
            if brightness < 0.08:
                level = 0
            if row == 0 and hash01(5, column, row) < 0.2:
                level = 4
            paint(column, row, tints[level])

    # Stray cells drifting in the dark below the fall.
    for column in range(columns):
        for row in range(rows):
            if hash01(6, column, row) < 0.012:
                paint(column, row, tints[0])
    return pixels


def hash01(*values: int) -> float:
    """A value in [0, 1) from a few integers, the same for the same integers.

    A splitmix-style mixer rather than `zlib.crc32`: a CRC is linear, and the
    cells it placed lined up in rows the eye found at once.
    """
    state = 0x9E3779B97F4A7C15
    for value in values:
        state = (state ^ (value & MASK64)) * 0xBF58476D1CE4E5B9 & MASK64
        state ^= state >> 31
        state = state * 0x94D049BB133111EB & MASK64
        state ^= state >> 29
    return (state >> 11) / (1 << 53)


MASK64 = (1 << 64) - 1


def mix(
    first: tuple[float, float, float], second: tuple[float, float, float], amount: float
) -> tuple[float, float, float]:
    return tuple(first[index] + (second[index] - first[index]) * amount for index in range(3))


def fill_rect(
    pixels: bytearray,
    width: int,
    height: int,
    left: int,
    top: int,
    right: int,
    bottom: int,
    color: tuple[float, float, float],
) -> None:
    """Fill one clipped rectangle of an RGB canvas, a row slice at a time."""
    left, top = max(0, left), max(0, top)
    right, bottom = min(width, right), min(height, bottom)
    if right <= left or bottom <= top:
        return
    line = bytes(clamp(channel) for channel in color) * (right - left)
    for row in range(top, bottom):
        at = (row * width + left) * 3
        pixels[at : at + len(line)] = line


def active_border_stops(
    value: str | None, fallback: tuple[int, int, int]
) -> list[tuple[tuple[int, int, int], float]]:
    """RGB stops and opacity from an Omarchy active-border declaration."""
    stops = []
    for rgb, opacity in re.findall(r"(?:#|rgba\()?([0-9a-fA-F]{6})([0-9a-fA-F]{2})?", value or ""):
        stops.append((channels(rgb), int(opacity, 16) / 255 if opacity else 1.0))
    return stops or [(fallback, 1.0)]


def draw_active_border(
    pixels: bytearray,
    canvas_width: int,
    canvas_height: int,
    left: int,
    top: int,
    window_width: int,
    window_height: int,
    stops: list[tuple[tuple[int, int, int], float]],
) -> None:
    """Draw the theme's active-window cue immediately outside a capture."""
    thickness = max(3, round(canvas_width / 900))
    right, bottom = left + window_width, top + window_height

    def paint(box: tuple[int, int, int, int]) -> None:
        box_left, box_top, box_right, box_bottom = box
        for row in range(max(0, box_top), min(canvas_height, box_bottom)):
            for column in range(max(0, box_left), min(canvas_width, box_right)):
                position = ((column - left) + (row - top)) / (window_width + window_height)
                position = max(0.0, min(1.0, position))
                scaled = position * (len(stops) - 1)
                first = min(int(scaled), len(stops) - 1)
                second = min(first + 1, len(stops) - 1)
                amount = scaled - first
                color = tuple(
                    round(stops[first][0][channel] * (1.0 - amount) + stops[second][0][channel] * amount)
                    for channel in range(3)
                )
                alpha = stops[first][1] * (1.0 - amount) + stops[second][1] * amount
                at = (row * canvas_width + column) * 3
                for channel in range(3):
                    pixels[at + channel] = round(
                        pixels[at + channel] * (1.0 - alpha) + color[channel] * alpha
                    )

    paint((left - thickness, top - thickness, right + thickness, top))
    paint((left - thickness, bottom, right + thickness, bottom + thickness))
    paint((left - thickness, top, left, bottom))
    paint((right, top, right + thickness, bottom))


def clamp(value: float) -> int:
    return 0 if value < 0 else (255 if value > 255 else int(value + 0.5))


def blur(pixels: bytearray, width: int, height: int, radius: int) -> bytearray:
    """A separable box blur, twice, which is close enough to a gaussian.

    Applied under the window unless `--no-blur` asks otherwise. A blur under
    the translucent surfaces is what a compositor with `decoration:blur` on
    shows, and the shots are meant to look like the desktop the browser was
    designed for rather than the flattest install of it: a sharp field of cells
    behind the sidebar reads as noise, a blurred one as a glow.
    """
    for _ in range(2):
        pixels = box_pass(pixels, width, height, radius)
        pixels = transpose(pixels, width, height)
        pixels = box_pass(pixels, height, width, radius)
        pixels = transpose(pixels, height, width)
    return pixels


def box_pass(pixels: bytearray, width: int, height: int, radius: int) -> bytearray:
    out = bytearray(len(pixels))
    window = radius * 2 + 1
    for row in range(height):
        base = row * width * 3
        for channel in range(3):
            line = pixels[base + channel : base + width * 3 : 3]
            total = line[0] * radius + sum(line[: radius + 1])
            for column in range(width):
                out[base + column * 3 + channel] = total // window
                leaving = line[max(column - radius, 0)]
                entering = line[min(column + radius + 1, width - 1)]
                total += entering - leaving
    return out


def transpose(pixels: bytearray, width: int, height: int) -> bytearray:
    out = bytearray(len(pixels))
    for row in range(height):
        source = row * width * 3
        for column in range(width):
            target = (column * height + row) * 3
            out[target : target + 3] = pixels[source + column * 3 : source + column * 3 + 3]
    return out


# ------------------------------------------------------- scale and encode


def find_encoder() -> str | None:
    """The tool that turns a PNG on stdin into WebP on stdout.

    WebP because the captures are taken at twice the size the page draws them,
    and a thumbnail of one has to be cheap. `cwebp` is libwebp's own tool;
    ImageMagick is the fallback because a machine that renders this site tends
    to have it already.
    """
    for name in ("cwebp", "magick", "convert"):
        if shutil.which(name):
            return name
    return None


def encoding(encoder: str, width: int | None, lossless: bool) -> list[str]:
    """The command line for one encode, scaling on the way through if asked.

    The full capture is encoded losslessly: it is the file a reader opens to
    read the type in. A thumbnail is the opposite case. Nobody reads one, and
    lossless spends four times what it needs to.
    """
    if encoder == "cwebp":
        # -z 9 is the slowest and smallest of the lossless presets.
        quality = ["-lossless", "-z", "9"] if lossless else ["-q", "82"]
        resize = ["-resize", str(width), "0"] if width else []
        return ["cwebp", "-quiet", *quality, *resize, "-o", "-", "--", "-"]
    # ImageMagick reads its operators between the input and the output.
    quality = ["-define", "webp:lossless=true"] if lossless else ["-quality", "82"]
    resize = ["-filter", "Lanczos", "-resize", f"{width}x"] if width else []
    return [encoder, "png:-", *resize, *quality, "webp:-"]


def write_webp(
    path: pathlib.Path,
    encoder: str,
    png: bytes,
    width: int | None = None,
    lossless: bool = True,
) -> None:
    """Encode one composite, scaled to `width` on the way through if given.

    The encoder does the scaling because it is decoding the image anyway, and
    a box average written in Python over a canvas this size costs more than
    every other step here put together.
    """
    command = encoding(encoder, width, lossless)
    result = subprocess.run(command, input=png, capture_output=True)
    if result.returncode != 0 or not result.stdout:
        sys.exit(f"{encoder} could not encode {path.name}: {result.stderr.decode().strip()}")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(result.stdout)


# ------------------------------------------------------------- composite


def composite(
    capture: pathlib.Path, ground: bytearray, canvas: tuple[int, int], offset: tuple[int, int]
) -> bytearray:
    """The window's own pixels over the desktop, at the alpha it was captured at.

    The capture keeps the opacity the theme gave each surface, so the desktop
    shows through the sidebar and the empty page ground exactly as far as a
    compositor would let it.

    Done a row at a time over channel slices rather than a pixel at a time:
    a capture at this size is several million pixels, and the interpreter is
    the whole cost. A row that turns out to be one alpha throughout -- an
    opaque band of page, a translucent band of sidebar -- skips the blend for
    a slice assignment or a single constant.
    """
    width, _ = canvas
    left, top = offset
    shot_width, shot_height, shot = read_png(capture)
    stride = shot_width * 3
    pixels = bytearray(ground)
    row_bytes = bytearray(stride)
    for row in range(shot_height):
        line = shot[row * shot_width * 4 : (row + 1) * shot_width * 4]
        alpha = line[3::4]
        target = ((top + row) * width + left) * 3
        low, high = min(alpha), max(alpha)
        if low == 255:
            row_bytes[0::3] = line[0::4]
            row_bytes[1::3] = line[1::4]
            row_bytes[2::3] = line[2::4]
        else:
            under = pixels[target : target + stride]
            if low == high:
                weight = low
                rest = 255 - weight
                for channel in range(3):
                    row_bytes[channel::3] = bytes(
                        (over * weight + beneath * rest) // 255
                        for over, beneath in zip(line[channel::4], under[channel::3])
                    )
            else:
                for channel in range(3):
                    row_bytes[channel::3] = bytes(
                        (over * transmit + beneath * (255 - transmit)) // 255
                        for over, beneath, transmit in zip(
                            line[channel::4], under[channel::3], alpha
                        )
                    )
        pixels[target : target + stride] = row_bytes
    return pixels


# ------------------------------------------------------------------ assets


def favicon(palette: dict) -> str:
    """The application icon in one theme's ground and foreground.

    The two colours are replaced by the literals the shipped icon draws them
    with, so a redesign that renames them fails here instead of quietly
    shipping six copies of one palette.
    """
    source = APPLICATION_ICON.read_text(encoding="utf-8")
    ground, mark = "#06060c", "#82fb9c"
    for color in (ground, mark):
        if color not in source:
            raise SystemExit(f"{APPLICATION_ICON} no longer draws {color}")
    return stamp(
        source.replace(ground, hex_color(palette["windowOpaque"])).replace(
            mark, hex_color(palette["text"])
        )
    )


def wordmark(palette: dict) -> str:
    """The wordmark in one theme's foreground, on the page's own ground.

    Drawn black by the editor that made it, and the lettering carries no fill
    of its own, so the colour is set once on the root and the mark's own black
    is turned into an inheritance. The page used to invert the whole image to
    get white out of it, which no theme could reach.
    """
    source = WORDMARK.read_text(encoding="utf-8")
    black = "fill:#000000"
    if black not in source:
        raise SystemExit(f"{WORDMARK} no longer draws its mark in {black}")
    source = source.replace(black, "fill:inherit", 1)
    opened = source.index("<svg") + len("<svg")
    color = hex_color(palette["text"])
    return stamp(source[:opened] + f'\n   fill="{color}"' + source[opened:])


def stamp(svg: str) -> str:
    """Mark the file generated, after any XML declaration rather than before.

    A comment ahead of `<?xml ...?>` is not well-formed, and the wordmark was
    saved by an editor that writes one.
    """
    end = svg.find("?>")
    at = 0 if end < 0 else svg.index("\n", end) + 1
    return svg[:at] + GENERATED + "\n" + svg[at:]


def stylesheet(palettes: dict[str, tuple[dict, dict]]) -> str:
    lines = [
        "/* Generated by scripts/build_website_themes.py. Do not edit.",
        " *",
        " * The roles Omaweb resolved from each theme's own colors.toml, not an",
        " * approximation of them: the browser derives every role it draws from the",
        " * handful of colours a theme names, and floors the quiet ones against the",
        " * ground they sit on. Re-run the script to pick up an upstream change.",
        " */",
        "",
    ]
    for name, _ in THEMES:
        if name not in palettes:
            continue
        palette, named = palettes[name]
        lines.append(f'body[data-theme="{name}"] {{')
        for token, role in RESOLVED_ROLES.items():
            lines.append(f"  {token}: {hex_color(palette[role])};")
        for token, role in NAMED_ROLES.items():
            lines.append(f"  {token}: {hex_color(named[role])};")
        lines.append("}")
        lines.append("")
    return "\n".join(lines)


# -------------------------------------------------------------------- main


def build(
    theme: str,
    lab: pathlib.Path,
    scratch: pathlib.Path,
    encoder: str,
    blurred: bool,
    opacity: dict[str, float] | None,
) -> tuple[dict, dict] | None:
    colors = theme_colors(theme)
    if colors is None:
        print(f"  {theme}: not installed, skipped")
        return None

    theme_file = scratch / f"{theme}.json"
    named = render_theme_file(colors, theme_file, opacity)
    palette = resolved_palette(lab, theme_file)

    (ICONS / f"favicon-{theme}.svg").write_text(favicon(palette), encoding="utf-8")
    (ICONS / f"wordmark-{theme}.svg").write_text(wordmark(palette), encoding="utf-8")

    captures = {}
    for state, arguments in STATES:
        target = scratch / f"{theme}-{state}.png"
        run_lab(lab, theme_file, [*arguments, "--capture", str(target)])
        captures[state] = target

    width, height, _ = read_png(captures["space"])
    horizontal_margin = round(width * HORIZONTAL_MARGIN)
    vertical_margin = round(height * VERTICAL_MARGIN)
    canvas = (width + horizontal_margin * 2, height + vertical_margin * 2)
    ground = wallpaper(palette, *canvas)
    if blurred:
        softened = blur(bytearray(ground), *canvas, radius=max(2, round(canvas[0] / 90)))
        for row in range(vertical_margin, vertical_margin + height):
            start = (row * canvas[0] + horizontal_margin) * 3
            end = start + width * 3
            ground[start:end] = softened[start:end]
    draw_active_border(
        ground,
        *canvas,
        horizontal_margin,
        vertical_margin,
        width,
        height,
        active_border_stops(colors.get("hyprland_active_border"), channels(palette["accent"])),
    )

    for state, capture in captures.items():
        pixels = composite(capture, ground, canvas, (horizontal_margin, vertical_margin))
        png = png_bytes(*canvas, pixels)
        write_webp(SHOTS / theme / f"{state}.webp", encoder, png)
        write_webp(
            SHOTS / theme / f"{state}-thumb.webp",
            encoder,
            png,
            width=THUMBNAIL_WIDTH,
            lossless=False,
        )
        print(f"  {theme}/{state}.webp")
    return palette, named


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--lab",
        type=pathlib.Path,
        default=ROOT / "build" / "dev" / "omaweb-ui-lab",
        help="the omaweb-ui-lab binary to capture with",
    )
    parser.add_argument(
        "--theme", action="append", help="build only this theme; repeatable"
    )
    parser.add_argument(
        "--no-blur",
        dest="blur",
        action="store_false",
        help="leave the desktop behind the window sharp, as a compositor without blur shows it",
    )
    parser.add_argument(
        "--template-opacity",
        action="store_true",
        help="capture the window at the opacity the template ships, not the frosted capture one",
    )
    parser.add_argument(
        "--themes",
        action="append",
        type=pathlib.Path,
        default=[],
        help="a directory of Omarchy themes to read before the installed ones; repeatable",
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

    palettes = {}
    with tempfile.TemporaryDirectory() as directory:
        scratch = pathlib.Path(directory)
        for theme, _ in wanted:
            print(f"{theme}:")
            built = build(
                theme,
                arguments.lab,
                scratch,
                encoder,
                arguments.blur,
                None if arguments.template_opacity else CAPTURE_OPACITY,
            )
            if built is not None:
                palettes[theme] = built

    if not palettes:
        return fail("none of the requested themes are installed")

    STYLESHEET.write_text(stylesheet(palettes), encoding="utf-8")
    print(f"  {STYLESHEET.relative_to(ROOT)}")

    default = THEMES[0][0]
    if default in palettes:
        shutil.copyfile(ICONS / f"favicon-{default}.svg", WEBSITE / "favicon.svg")
        print(f"  {(WEBSITE / 'favicon.svg').relative_to(ROOT)}")
    return 0


def fail(message: str) -> int:
    print(message, file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
