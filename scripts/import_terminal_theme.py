#!/usr/bin/env python3
"""Derive a Tanto theme from the colours of the terminal this script runs in.

A terminal gives up a background, a foreground and sixteen ANSI colours. Tanto's
schema wants a lightness ladder of chrome surfaces on top of that, so most of
this file is the derivation rather than the reading. Run it from the terminal
whose colours you want; TERM_PROGRAM then identifies that terminal exactly,
which nothing about a browser launched from the Dock ever could.
"""

import argparse
import json
import os
import plistlib
import re
import shutil
import subprocess
import sys
from pathlib import Path

REPOSITORY_ROOT = Path(__file__).resolve().parent.parent
TEMPLATE = REPOSITORY_ROOT / "assets" / "themes" / "default.json"

# Only the ANSI slots that read as decoration rather than as status. Red, green
# and yellow carry "error", "success" and "warning" in an interface, so an
# accent picked from them would fight the meaning it already has.
ACCENT_SLOTS = (4, 5, 6, 12, 13, 14)
# A private window has to be unmistakable at a glance, so its accent comes from
# the magenta and red families — and never from whatever the accent already took.
PRIVATE_SLOTS = (13, 5, 9, 1)

# Where each derived surface sits between the background and the foreground,
# measured off the shipped default theme so an imported theme has the same
# spacing between its chrome layers.
SURFACE_STEPS = {
    "sidebar": 0.04,
    "overlay": 0.10,
    "surface": 0.14,
    "surfaceHover": 0.21,
    "border": 0.27,
}
MUTED_TEXT_STEP = 0.70

# The private-window ladder is the same climb taken towards the private accent
# instead of the foreground. A straight mix lands too grey to read as a warning
# at a glance, so the result keeps more of its chroma than the mix produces.
PRIVATE_STEPS = {
    "privateWindow": 0.24,
    "privateSidebar": 0.32,
    "privateSurface": 0.36,
    "privateSurfaceHover": 0.50,
}
PRIVATE_CHROMA_GAIN = 1.5

# xterm's palette, for a terminal that leaves some slots at their built-in value
# and so never writes them to a config file.
FALLBACK_PALETTE = [
    "#000000", "#cd0000", "#00cd00", "#cdcd00", "#0000ee", "#cd00cd", "#00cdcd", "#e5e5e5",
    "#7f7f7f", "#ff0000", "#00ff00", "#ffff00", "#5c5cff", "#ff00ff", "#00ffff", "#ffffff",
]


class ImportError_(Exception):
    """A terminal that cannot be read, reported without a traceback."""


# --- colour ----------------------------------------------------------------


def to_linear(channel):
    channel /= 255.0
    return channel / 12.92 if channel <= 0.04045 else ((channel + 0.055) / 1.055) ** 2.4


def from_linear(channel):
    channel = 12.92 * channel if channel <= 0.0031308 else 1.055 * channel ** (1 / 2.4) - 0.055
    return round(min(1.0, max(0.0, channel)) * 255)


def parse_hex(text):
    value = text.strip().strip('"').lstrip("#")
    if len(value) == 3:
        value = "".join(character * 2 for character in value)
    if len(value) != 6 or not re.fullmatch(r"[0-9a-fA-F]{6}", value):
        raise ImportError_(f"not a colour: {text!r}")
    return tuple(int(value[index:index + 2], 16) for index in (0, 2, 4))


def to_hex(rgb):
    return "#%02x%02x%02x" % rgb


def to_oklab(rgb):
    red, green, blue = (to_linear(float(channel)) for channel in rgb)
    long_ = (0.4122214708 * red + 0.5363325363 * green + 0.0514459929 * blue) ** (1 / 3)
    medium = (0.2119034982 * red + 0.6806995451 * green + 0.1073969566 * blue) ** (1 / 3)
    short = (0.0883024619 * red + 0.2817188376 * green + 0.6299787005 * blue) ** (1 / 3)
    return (
        0.2104542553 * long_ + 0.7936177850 * medium - 0.0040720468 * short,
        1.9779984951 * long_ - 2.4285922050 * medium + 0.4505937099 * short,
        0.0259040371 * long_ + 0.7827717662 * medium - 0.8086757660 * short,
    )


def from_oklab(lab):
    lightness, green_red, blue_yellow = lab
    long_ = (lightness + 0.3963377774 * green_red + 0.2158037573 * blue_yellow) ** 3
    medium = (lightness - 0.1055613458 * green_red - 0.0638541728 * blue_yellow) ** 3
    short = (lightness - 0.0894841775 * green_red - 1.2914855480 * blue_yellow) ** 3
    return (
        from_linear(4.0767416621 * long_ - 3.3077115913 * medium + 0.2309699292 * short),
        from_linear(-1.2684380046 * long_ + 2.6097574011 * medium - 0.3413193965 * short),
        from_linear(-0.0041960863 * long_ - 0.7034186147 * medium + 1.7076147010 * short),
    )


def mix(first, second, amount):
    """Blend perceptually, so an even step in `amount` looks like an even step."""
    left, right = to_oklab(first), to_oklab(second)
    return from_oklab(tuple(a + (b - a) * amount for a, b in zip(left, right)))


def chroma(rgb):
    _, green_red, blue_yellow = to_oklab(rgb)
    return (green_red ** 2 + blue_yellow ** 2) ** 0.5


def scale_chroma(rgb, gain):
    lightness, green_red, blue_yellow = to_oklab(rgb)
    return from_oklab((lightness, green_red * gain, blue_yellow * gain))


def relative_luminance(rgb):
    red, green, blue = (to_linear(float(channel)) for channel in rgb)
    return 0.2126 * red + 0.7152 * green + 0.0722 * blue


def contrast(first, second):
    lighter, darker = sorted(
        (relative_luminance(first), relative_luminance(second)), reverse=True)
    return (lighter + 0.05) / (darker + 0.05)


# --- terminals -------------------------------------------------------------


def run(command):
    try:
        result = subprocess.run(command, capture_output=True, text=True, timeout=20)
    except (OSError, subprocess.SubprocessError) as error:
        raise ImportError_(f"could not run {command[0]}: {error}") from error
    if result.returncode != 0:
        raise ImportError_(f"{command[0]} failed: {result.stderr.strip()}")
    return result.stdout


def read_ghostty():
    if not shutil.which("ghostty"):
        raise ImportError_("ghostty is not on PATH")
    # show-config resolves `theme = TokyoNight Night` down to a background, a
    # foreground and sixteen palette lines, so no theme file has to be found.
    settings = {}
    palette = {}
    for line in run(["ghostty", "+show-config"]).splitlines():
        key, separator, value = line.partition("=")
        if not separator:
            continue
        key, value = key.strip(), value.strip()
        if key == "palette":
            index, _, colour = value.partition("=")
            if index.strip().isdigit():
                palette[int(index)] = colour
        else:
            settings.setdefault(key, value)
    return {
        "background": settings.get("background"),
        "foreground": settings.get("foreground"),
        "palette": palette,
        "font": settings.get("font-family"),
    }


def read_preferences(domain):
    """Read a preferences domain through cfprefsd, which the plist file lags."""
    return plistlib.loads(run(["defaults", "export", domain, "-"]).encode())


def component_colour(entry):
    """An AppKit colour dictionary, in whichever space the app recorded it."""
    red = float(entry.get("Red Component", 0.0))
    green = float(entry.get("Green Component", 0.0))
    blue = float(entry.get("Blue Component", 0.0))
    if entry.get("Color Space") == "P3":
        # Display P3 primaries into sRGB's, before the transfer function.
        red, green, blue = (to_linear(red * 255), to_linear(green * 255), to_linear(blue * 255))
        red, green, blue = (
            1.2249 * red - 0.2247 * green + 0.0000 * blue,
            -0.0420 * red + 1.0419 * green + 0.0000 * blue,
            -0.0197 * red - 0.0786 * green + 1.0979 * blue,
        )
        return (from_linear(red), from_linear(green), from_linear(blue))
    return tuple(round(min(1.0, max(0.0, channel)) * 255) for channel in (red, green, blue))


def read_iterm2():
    preferences = read_preferences("com.googlecode.iterm2")
    profiles = preferences.get("New Bookmarks") or []
    if not profiles:
        raise ImportError_("iTerm2 has no profiles recorded")
    wanted = os.environ.get("ITERM_PROFILE")
    default_guid = preferences.get("Default Bookmark Guid")
    profile = next((p for p in profiles if wanted and p.get("Name") == wanted), None)
    profile = profile or next((p for p in profiles if p.get("Guid") == default_guid), profiles[0])

    def colour(key):
        entry = profile.get(key)
        return to_hex(component_colour(entry)) if isinstance(entry, dict) else None

    return {
        "background": colour("Background Color"),
        "foreground": colour("Foreground Color"),
        "palette": {index: colour(f"Ansi {index} Color") for index in range(16)},
        "font": (profile.get("Normal Font") or "").rsplit(" ", 1)[0] or None,
    }


def read_apple_terminal():
    preferences = read_preferences("com.apple.Terminal")
    name = preferences.get("Default Window Settings") or "Basic"
    profile = (preferences.get("Window Settings") or {}).get(name) or {}
    # Terminal records a built-in profile by name alone and keeps its colours
    # inside the app, so an untouched profile has nothing on disk to read.
    if not any("Color" in key for key in profile):
        raise ImportError_(
            f"Terminal profile {name!r} has no saved colours: it is an unmodified built-in, "
            "and Terminal keeps those inside the application")

    def colour(key):
        blob = profile.get(key)
        if not isinstance(blob, bytes):
            return None
        # Terminal archives an NSColor rather than storing components. The
        # archive carries them as an ASCII float triple, which is far cheaper to
        # pull out than to stand up a keyed unarchiver for.
        found = re.search(rb"(\d\.?\d*(?:e-?\d+)? \d\.?\d*(?:e-?\d+)? \d\.?\d*(?:e-?\d+)?)", blob)
        if not found:
            return None
        parts = [float(part) for part in found.group(1).split()]
        return to_hex(tuple(round(min(1.0, max(0.0, part)) * 255) for part in parts))

    slots = ["Black", "Red", "Green", "Yellow", "Blue", "Magenta", "Cyan", "White"]
    palette = {}
    for index, slot in enumerate(slots):
        palette[index] = colour(f"ANSI{slot}Color")
        palette[index + 8] = colour(f"ANSIBright{slot}Color")
    return {
        "background": colour("BackgroundColor"),
        "foreground": colour("TextColor"),
        "palette": palette,
        "font": None,
    }


def read_key_value_config(path, keys, depth=0):
    """Read `key value` lines, following `include` the way kitty does.

    Themes arrive as an included file rather than as edits to the main config,
    so the colours are usually not in the file the user points at.
    """
    if not path.exists():
        raise ImportError_(f"no config at {path}")
    settings = {}
    included = []
    for line in path.read_text(errors="replace").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        key, _, value = line.partition(" ")
        if key in ("include", "globinclude") and depth < 4:
            included.append(value.strip())
        elif key in keys:
            settings.setdefault(key, value.strip())
    for name in included:
        for candidate in sorted(path.parent.glob(name)):
            for key, value in read_key_value_config(candidate, keys, depth + 1).items():
                settings.setdefault(key, value)
    return settings


def read_kitty():
    base = Path(os.environ.get("XDG_CONFIG_HOME", Path.home() / ".config")) / "kitty"
    keys = {"background", "foreground", "font_family"} | {f"color{i}" for i in range(16)}
    settings = read_key_value_config(base / "kitty.conf", keys)
    return {
        "background": settings.get("background"),
        "foreground": settings.get("foreground"),
        "palette": {index: settings.get(f"color{index}") for index in range(16)},
        "font": settings.get("font_family"),
    }


def read_alacritty():
    try:
        import tomllib
    except ModuleNotFoundError as error:  # Python older than 3.11.
        raise ImportError_("reading Alacritty's TOML needs Python 3.11 or newer") from error
    base = Path(os.environ.get("XDG_CONFIG_HOME", Path.home() / ".config")) / "alacritty"
    path = base / "alacritty.toml"
    if not path.exists():
        raise ImportError_(f"no config at {path}")
    settings = tomllib.loads(path.read_text(errors="replace"))
    colours = settings.get("colors", {})
    primary = colours.get("primary", {})
    normal, bright = colours.get("normal", {}), colours.get("bright", {})
    slots = ["black", "red", "green", "yellow", "blue", "magenta", "cyan", "white"]
    palette = {index: normal.get(slot) for index, slot in enumerate(slots)}
    palette.update({index + 8: bright.get(slot) for index, slot in enumerate(slots)})
    return {
        "background": primary.get("background"),
        "foreground": primary.get("foreground"),
        "palette": palette,
        "font": (settings.get("font", {}).get("normal", {}) or {}).get("family"),
    }


READERS = {
    "ghostty": read_ghostty,
    "iterm2": read_iterm2,
    "apple-terminal": read_apple_terminal,
    "kitty": read_kitty,
    "alacritty": read_alacritty,
}

# TERM_PROGRAM as each terminal sets it.
TERM_PROGRAMS = {
    "ghostty": "ghostty",
    "iTerm.app": "iterm2",
    "Apple_Terminal": "apple-terminal",
    "kitty": "kitty",
    "alacritty": "alacritty",
    "WezTerm": "wezterm",
}


def detect_terminal():
    program = os.environ.get("TERM_PROGRAM", "")
    name = TERM_PROGRAMS.get(program)
    if name == "wezterm":
        raise ImportError_(
            "WezTerm's config is a Lua program, so its colours cannot be read without running "
            "it; pass --terminal with another terminal, or write the theme by hand")
    if not name:
        raise ImportError_(
            f"could not tell which terminal this is (TERM_PROGRAM={program!r}). Run this from "
            f"the terminal you want, or pass --terminal {{{','.join(READERS)}}}")
    return name


# --- derivation ------------------------------------------------------------


def derive(source):
    if not source.get("background") or not source.get("foreground"):
        raise ImportError_("the terminal reports no background or foreground colour")
    window = parse_hex(source["background"])
    text = parse_hex(source["foreground"])
    palette = []
    for index in range(16):
        entry = (source.get("palette") or {}).get(index)
        palette.append(parse_hex(entry) if entry else parse_hex(FALLBACK_PALETTE[index]))

    # An accent has to survive being drawn as a hairline on the window, so
    # contrast gates the choice and chroma only ranks what is left. A palette
    # too muted to clear the gate falls back to its most contrasting entry.
    candidates = [palette[index] for index in ACCENT_SLOTS]
    legible = [colour for colour in candidates if contrast(colour, window) >= 4.5]
    accent = max(legible or candidates,
                 key=lambda colour: chroma(colour) if legible else contrast(colour, window))

    private_options = [palette[index] for index in PRIVATE_SLOTS if palette[index] != accent]
    private_accent = (max(private_options, key=chroma) if private_options
                      else scale_chroma(accent, 1.4))

    derived = {
        "window": to_hex(window),
        "text": to_hex(text),
        "mutedText": to_hex(mix(window, text, MUTED_TEXT_STEP)),
        "accent": to_hex(accent),
        "privateAccent": to_hex(private_accent),
    }
    for key, step in SURFACE_STEPS.items():
        derived[key] = to_hex(mix(window, text, step))
    for key, step in PRIVATE_STEPS.items():
        derived[key] = to_hex(scale_chroma(mix(window, private_accent, step), PRIVATE_CHROMA_GAIN))
    return derived


def build_theme(source):
    """Fill the shipped theme's colour keys, leaving its other sections alone.

    Type sizes, tint and the semantic opacities are Tanto's own layout
    decisions. A terminal's background-opacity in particular means something
    different here, where opacity is per-surface rather than per-window.
    """
    theme = json.loads(TEMPLATE.read_text())
    theme.update(derive(source))
    font = (source.get("font") or "").strip().strip('"')
    if font:
        families = theme.get("font", {}).get("families", [])
        theme["font"]["families"] = [font] + [f for f in families if f != font]
    return theme


OMARCHY_THEME = Path.home() / ".local/state/omarchy/current/theme/tanto.json"


def desktop_manages_theme():
    """Whether a desktop theme manager is already supplying Tanto's palette.

    On Omarchy the desktop is the source of truth: it renders Tanto's template
    on every theme switch. A theme written to the configuration directory ranks
    above that, so importing one here would silently freeze the palette at
    whatever the terminal looked like on the day the import ran.
    """
    return sys.platform.startswith("linux") and OMARCHY_THEME.exists()


def default_output():
    override = os.environ.get("TANTO_CONFIG_ROOT")
    if override:
        return Path(override) / "theme.json"
    base = os.environ.get("XDG_CONFIG_HOME") or Path.home() / ".config"
    return Path(base) / "tanto" / "theme.json"


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--terminal", choices=sorted(READERS),
                        help="read this terminal instead of the one running the script")
    parser.add_argument("--output", type=Path, default=None,
                        help="write here instead of the Tanto configuration directory")
    parser.add_argument("--print", dest="print_only", action="store_true",
                        help="write nothing; print the theme")
    parser.add_argument("--force", action="store_true", help="overwrite an existing theme")
    arguments = parser.parse_args()

    try:
        name = arguments.terminal or detect_terminal()
        theme = build_theme(READERS[name]())
    except ImportError_ as error:
        print(f"import_terminal_theme: {error}", file=sys.stderr)
        return 1

    document = json.dumps(theme, indent=2) + "\n"
    if arguments.print_only:
        sys.stdout.write(document)
        return 0

    output = arguments.output or default_output()
    if arguments.output is None and desktop_manages_theme() and not arguments.force:
        print(f"import_terminal_theme: Omarchy is already theming Tanto through "
              f"{OMARCHY_THEME}, and a theme in the configuration directory would override it "
              f"for good — the palette would stop following theme switches. Pass --force to "
              f"import anyway, or --print to see the result without writing.", file=sys.stderr)
        return 1
    if output.exists() and not arguments.force:
        print(f"import_terminal_theme: {output} exists; pass --force to replace it",
              file=sys.stderr)
        return 1
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(document)
    print(f"Wrote {name} colours to {output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
