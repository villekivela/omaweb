#!/usr/bin/env python3
"""Records the website's introductory film from the real browser.

The website opens on a film of Omaweb in use: the Omnibar, a Space switch, split view with the
sidebar hidden, link hints, blocking, a theme change, an Agent at work and the install line. It
has to show the real browser on its real engine and stay true to each release, so it is recorded
by this script rather than by hand.
"""

from __future__ import annotations

import argparse
import dataclasses
import html
import http.server
import json
import os
import re
import shutil
import signal
import socket
import statistics
import subprocess
import sys
import tempfile
import threading
import time
import tomllib
import urllib.parse
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))

import benchmark_runtime as runtime  # noqa: E402
import build_website_themes as themes  # noqa: E402

# The invented sites the film browses, one directory each under `film/sites/`, named for the host
# that serves it. `.test` is reserved, so none of these names can be a real site, and Omaweb counts
# it as a Local-development site's, so the pages load over plain HTTP without a certificate.
SITE_ROOT = ROOT / "film" / "sites"
SITES = (
    "fernwood.test",  # an outdoor store: the Omnibar and the reader's tabs
    "quillstack.test",  # a developer docs site: the Work Space's split
    "halyard.test",  # a magazine carrying ads: blocking
    "tallyhaus.test",  # an invoicing dashboard: the Agent's work
    "kestrel.test",  # the search engine, and its Engine suggestions
    "adsprout.test",  # the magazine's ad network, which blocking keeps out
    "pixelmint.test",  # the magazine's tracker, likewise
)

# The film's last page, which every site serves: the logo and the install line.
END_CARD_PATH = "/film/end/"

# Paths the server answers itself on every site rather than from a file: what a page reports, the
# search engine's two endpoints, the invoice form's target, the script that does the reporting and
# the end card.
SERVED_PATHS = {"/beacon", "/search", "/suggest", "/invoices", "/film/report.js", END_CARD_PATH}


def site_directory(host: str) -> Path:
    return SITE_ROOT / host


CONTENT_TYPES = {
    ".html": "text/html; charset=utf-8",
    ".css": "text/css; charset=utf-8",
    ".js": "text/javascript; charset=utf-8",
    ".svg": "image/svg+xml",
    ".gif": "image/gif",
    ".webp": "image/webp",
    ".json": "application/json",
}


class FixtureServer:
    """Every fixture site, each answered under its own name, and a record of what was asked.

    The browser reaches the sites by name, so the server answers by the `Host` a request names and
    refuses any other: a name nobody served is a page the film would have shown broken, and the
    recording is told about it as a stray. What a beat check reads back is here too: each request
    by the host it named, what the pages reported, and the invoices the Agent sent.
    """

    def __init__(self, address: tuple[str, int]) -> None:
        self._lock = threading.Lock()
        self._requests: list[tuple[str, str]] = []
        self._strays: list[str] = []
        self._reports: list[dict] = []
        self._invoices: list[dict] = []
        self._server = http.server.ThreadingHTTPServer(address, self._handler())
        self._server.daemon_threads = True
        self._thread: threading.Thread | None = None

    @property
    def port(self) -> int:
        return self._server.server_address[1]

    def start(self) -> None:
        self._thread = threading.Thread(target=self._server.serve_forever, daemon=True)
        self._thread.start()

    def stop(self) -> None:
        self._server.shutdown()
        self._server.server_close()

    def paths(self, host: str) -> list[str]:
        with self._lock:
            return [path for named, path in self._requests if named == host]

    def strays(self) -> list[str]:
        with self._lock:
            return list(self._strays)

    def reports(self) -> list[dict]:
        with self._lock:
            return list(self._reports)

    def invoices(self) -> list[dict]:
        with self._lock:
            return list(self._invoices)

    def _handler(self) -> type[http.server.BaseHTTPRequestHandler]:
        fixtures = self

        class Handler(http.server.BaseHTTPRequestHandler):
            def log_message(self, format: str, *arguments) -> None:  # noqa: A002
                pass

            def answer(self, status: int, body: bytes = b"", kind: str = "") -> None:
                self.send_response(status)
                if kind:
                    self.send_header("Content-Type", kind)
                self.send_header("Content-Length", str(len(body)))
                self.send_header("Cache-Control", "no-store")
                self.end_headers()
                self.wfile.write(body)

            def host(self) -> str:
                return (self.headers.get("Host") or "").split(":")[0].lower()

            def admitted(self) -> str | None:
                host = self.host()
                with fixtures._lock:
                    if host not in SITES:
                        fixtures._strays.append(host + self.path)
                        return None
                    fixtures._requests.append((host, self.path))
                return host

            def do_GET(self) -> None:  # noqa: N802
                host = self.admitted()
                if host is None:
                    self.answer(404)
                    return
                address = urllib.parse.urlsplit(self.path)
                query = urllib.parse.parse_qs(address.query).get("q", [""])[0]
                if address.path == "/film/report.js":
                    self.file(SITE_ROOT / "_film" / "report.js")
                elif address.path == END_CARD_PATH:
                    self.file(SITE_ROOT / "_film" / "end.html")
                elif host == "kestrel.test" and address.path == "/suggest":
                    self.answer(200, json.dumps(suggestions(query)).encode(),
                                "application/x-suggestions+json")
                elif host == "kestrel.test" and address.path == "/search":
                    self.page(site_directory(host) / "results.html", {"query": query})
                else:
                    self.file(site_directory(host) / address.path.lstrip("/"))

            def do_POST(self) -> None:  # noqa: N802
                host = self.admitted()
                if host is None:
                    self.answer(404)
                    return
                length = int(self.headers.get("Content-Length") or 0)
                body = self.rfile.read(length).decode("utf-8", errors="replace")
                path = urllib.parse.urlsplit(self.path).path
                if path == "/beacon":
                    with fixtures._lock:
                        fixtures._reports.append(json.loads(body or "{}"))
                    self.answer(204)
                elif host == "tallyhaus.test" and path == "/invoices":
                    form = urllib.parse.parse_qs(body)
                    invoice = {key: values[0] for key, values in form.items()}
                    with fixtures._lock:
                        fixtures._invoices.append(invoice)
                    self.page(site_directory(host) / "invoices" / "sent.html", invoice)
                else:
                    self.answer(404)

            def file(self, path: Path) -> None:
                if path.is_dir():
                    path = path / "index.html"
                resolved = path.resolve()
                if not resolved.is_relative_to(SITE_ROOT.resolve()) or not resolved.is_file():
                    self.answer(404)
                    return
                kind = CONTENT_TYPES.get(resolved.suffix, "application/octet-stream")
                self.answer(200, resolved.read_bytes(), kind)

            def page(self, template: Path, values: dict[str, str]) -> None:
                text = template.read_text(encoding="utf-8")
                text = re.sub(r"\{\{(\w+)\}\}",
                              lambda match: html.escape(values.get(match.group(1), "")), text)
                self.answer(200, text.encode(), CONTENT_TYPES[".html"])

        return Handler


class BeatMissed(RuntimeError):
    """A beat did not happen, so the film would show it broken. Nothing is encoded after one."""

    def __init__(self, beat: str, why: str) -> None:
        super().__init__(f"{beat}: {why}")


def expect_space_on_show(beat: str, answer: dict, name: str) -> None:
    """`answer` is `omaweb spaces --json`; the Space called `name` has to be the one on show."""
    on_show = [space["name"] for space in answer.get("spaces", []) if space.get("onShow")]
    if on_show != [name]:
        raise BeatMissed(beat, f"{name} is not on show; {', '.join(on_show) or 'nothing'} is")


def expect_current_tab(beat: str, answer: dict, address: str) -> None:
    """`answer` is `omaweb tabs --json`; the current tab has to be at `address`."""
    current = [tab["url"] for tab in answer.get("tabs", []) if tab.get("current")]
    if current != [address]:
        raise BeatMissed(beat, f"the current tab is {', '.join(current) or 'none'}, not {address}")


def expect_asked(beat: str, server: FixtureServer, host: str, path: str) -> None:
    if path not in server.paths(host):
        raise BeatMissed(beat, f"{host} was never asked for {path}")


def expect_wider(beat: str, before: list[dict], after: list[dict], host: str) -> None:
    """Every page of `host` that reported before the beat reported a wider viewport after it.

    A page reports its width when it loads and whenever its viewport changes, so `after` is
    `before` with whatever the beat added. Each half of a split is its own page.
    """

    def widths(reports: list[dict]) -> dict[str, int]:
        return {report["path"]: report["width"] for report in reports if report["host"] == host}

    was, now = widths(before), widths(after)
    if not was:
        raise BeatMissed(beat, f"no page of {host} was on show to measure")
    narrow = [path for path, width in was.items() if now.get(path, 0) <= width]
    if narrow:
        raise BeatMissed(beat, f"{', '.join(narrow)} on {host} did not widen")


def expect_blocked(beat: str, server: FixtureServer, reports: list[dict], page_host: str,
                   blocked: tuple[str, ...]) -> None:
    """The page on `page_host` loaded and drew no ad slot, and no `blocked` host was asked."""
    reached = [host for host in blocked if server.paths(host)]
    if reached:
        raise BeatMissed(beat, f"the page reached {', '.join(reached)}")
    loaded = [report for report in reports if report["host"] == page_host]
    if not loaded:
        raise BeatMissed(beat, f"{page_host} never reported that it loaded")
    last = loaded[-1]
    if last.get("adsShown") or last.get("adCreatives"):
        raise BeatMissed(beat, f"{page_host} still drew {last.get('adsShown')} ad slots")


def expect_hints(beat: str, reports: list[dict], host: str) -> None:
    """A page of `host` still has its link hints on show at the end of the beat.

    `report.js` reports the count each time the hints come up or go, so the last report is what
    is on screen. Hints that came up and were taken down at once would pass a check of any report
    and still show nothing on camera.
    """
    counts = [report.get("hints", 0) for report in reports if report["host"] == host]
    if not counts or not counts[-1]:
        raise BeatMissed(beat, f"no page of {host} kept its link hints on show; it reported "
                               f"{', '.join(map(str, counts)) or 'nothing'}")


def hover_point(beat: str, held: dict) -> tuple[float, float]:
    """Where a pointer resting on the Space the window held would be, as a fraction of the window.

    `held` is the window's answer to `film-hover`: the footer button's rectangle and the window's
    size, in the same logical pixels.
    """
    if not held.get("ok"):
        raise BeatMissed(beat, f"the window held no Space's name: {held.get('error', held)}")
    return ((held["x"] + held["width"] / 2) / held["windowWidth"],
            (held["y"] + held["height"] / 2) / held["windowHeight"])


def expect_repainted(beat: str, before: tuple[int, int, int], after: tuple[int, int, int],
                     old: tuple[int, int, int], new: tuple[int, int, int]) -> None:
    """A sample of the chrome was nearer the old theme's colour and is now nearer the new one's.

    Nearer rather than equal: the window is drawn at its theme's opacity over the compositor's
    ground, so what reaches the frame is a blend of the colour the theme names.
    """

    def distance(a: tuple[int, int, int], b: tuple[int, int, int]) -> int:
        return sum((x - y) ** 2 for x, y in zip(a, b))

    if distance(before, old) >= distance(before, new):
        raise BeatMissed(beat, f"the chrome was {before} before the change, not the old theme")
    if distance(after, new) >= distance(after, old):
        raise BeatMissed(beat, f"the chrome is {after} after the change, not the new theme")


def pixel(frame: bytes, x: int, y: int) -> tuple[int, int, int]:
    """One pixel of a binary PPM, which is what `grim -t ppm` writes."""
    fields = frame.split(maxsplit=4)
    if fields[0] != b"P6" or fields[3] != b"255":
        raise ValueError("not an 8-bit binary PPM")
    width = int(fields[1])
    start = len(frame) - len(fields[4]) + (y * width + x) * 3
    return tuple(frame[start:start + 3])


def target(beat: str, look: dict, name: str) -> str:
    """The label `look` gave the target called `name`, or one whose name starts with it."""
    targets = look.get("targets", [])
    for matches in (lambda found: found == name, lambda found: found.startswith(name)):
        for candidate in targets:
            if matches(candidate.get("name", "")):
                return candidate["label"]
    raise BeatMissed(beat, f"{look.get('url', 'the page')} has no target called {name}")


def expect_ran(beat: str, result: subprocess.CompletedProcess) -> None:
    if result.returncode != 0:
        command = " ".join(result.args[:2])
        said = (result.stderr or result.stdout or "").strip()
        raise BeatMissed(beat, f"{command} exited {result.returncode}: {said}")


def expect_invoice(beat: str, server: FixtureServer, wanted: dict[str, str]) -> None:
    sent = server.invoices()
    if not sent:
        raise BeatMissed(beat, "no invoice was sent")
    differing = {key: sent[-1].get(key) for key, value in wanted.items()
                 if sent[-1].get(key) != value}
    if differing:
        raise BeatMissed(beat, f"the invoice sent had {differing}, not {wanted}")


def expect_sites_on_loopback(beat: str, resolve=socket.gethostbyname) -> None:
    """Every fixture site resolves to the server on loopback, before the browser is started.

    `record_film.sh` names them in the hosts file, which Docker writes afresh when a container
    starts, so a recording rerun in a restarted container would load every page as an error.
    """
    lost = []
    for host in SITES:
        try:
            address = resolve(host)
        except OSError:
            address = None
        if address != "127.0.0.1":
            lost.append(f"{host} ({address or 'unresolved'})")
    if lost:
        raise BeatMissed(beat, f"/etc/hosts does not send {', '.join(lost)} to 127.0.0.1")


def expect_within(beat: str, paths: list[Path], limit: int) -> None:
    total = sum(path.stat().st_size for path in paths)
    if total > limit:
        raise BeatMissed(beat, f"the film is {total} bytes, over its {limit}")


def suggestions(query: str) -> list:
    """An OpenSearch suggestions answer: the terms, then what the engine proposes for them."""
    with open(site_directory("kestrel.test") / "suggestions.json", encoding="utf-8") as handle:
        proposals = json.load(handle)
    return [query, proposals.get(query.strip().lower(), [])]


# How long each beat holds still under its caption before its action starts, so the caption is read
# first and the eye then goes to what it names. The caption comes in 0.3 s into a beat.
CAPTION_LEAD = 1.5
# How long the camera takes to move from one place to the next.
MOVE = 0.7
# The Theme beat holds the old theme longer, so the change it names is the thing watched.
THEME_HOLD = 2.5
# How long a command the Agent runs stays on screen before the next one is run, at the least.
COMMAND_READ = 0.7

# The film's beats, in the order they play: what the caption says, empty for none, and, for each
# moment the camera moves, where it looks. A place is a fraction of the window, (left, top, width,
# height), so it holds at any output size; `None` is the whole window. Times are seconds into the
# beat.
@dataclasses.dataclass
class Beat:
    name: str
    caption: str
    focus: list[tuple[float, tuple[float, float, float, float] | None]]


# Where the parts the camera follows sit in the window, with the sidebar at its default width: the
# Omnibar a little below the middle of the page, the search results at the top left of the page,
# the Space squares at the foot of the sidebar with the page beside them, the blocked count at the
# end of the address, and the Agent's form.
OMNIBAR = (0.29, 0.28, 0.6, 0.6)
RESULTS = (0.2, 0.17, 0.4, 0.4)
SPACES = (0.0, 0.4, 0.6, 0.6)
SHIELD = (0.0, 0.0, 0.22, 0.22)
FORM = (0.32, 0.03, 0.66, 0.66)

BEATS = [
    Beat("Omnibar", "One Omnibar for tabs, Spaces and search",
         [(0.0, None), (CAPTION_LEAD + 0.2, OMNIBAR), (CAPTION_LEAD + 4.0, None)]),
    # On the search results, which are links at full width: a split's narrow docs fold their links
    # away. The camera closes in, since a hint is a small label.
    Beat("Hints", "Keyboard first: f puts a label on every link",
         [(0.0, None), (CAPTION_LEAD - MOVE, RESULTS)]),
    # The footer stays on camera through the switch, so the Space's colour is seen to change.
    Beat("Space switch", "Each Space keeps its own logins and tabs", [(0.0, SPACES)]),
    Beat("Sidebar", "Split view, sidebar out of the way", [(0.0, None)]),
    # Close on the count before the reload, so it is seen counting what it stops.
    Beat("Blocking", "Ads and trackers stopped before they load",
         [(0.0, None), (CAPTION_LEAD, SHIELD)]),
    Beat("Theme", "Your Omarchy theme reaches the whole browser", [(0.0, None)]),
    # Each command the Agent runs takes the caption's place as it runs.
    Beat("Agent", "An Agent works in a Space of its own",
         [(0.0, None), (CAPTION_LEAD + 1.4, FORM)]),
    Beat("End", "", [(0.0, None)]),
]
BEAT_NAMED = {beat.name: beat for beat in BEATS}

# The frame the page shows before the film plays: the Omnibar with its rows open.
POSTER = ("Omnibar", CAPTION_LEAD + 2.8)

# What the reader's two Spaces hold when the film starts. The Omnibar beat types "tra", which finds
# the trail shoe open here, the tracing guide open in Work, and the engine's suggestions.
PERSONAL_TABS = [
    "http://fernwood.test/products/canvas-daypack/",
    "http://halyard.test/2026/the-last-lighthouse-keepers/",
    "http://fernwood.test/products/trail-runner/",
]
WORK_TABS = ["http://quillstack.test/docs/tracing/", "http://quillstack.test/docs/api/"]
TYPED = "tra"
SUGGESTION = "trail running shoes"
# Steps down from the first row to the suggestion the beat picks: the rows are Work's tracing guide,
# then the trail shoe's tab, then the engine's suggestions. A row missing moves the pick onto
# another, and the tab's address then says so.
SUGGESTION_ROW = 2
# The tab's address as the browser reports it, which is decoded.
SEARCH = "http://kestrel.test/search?q=" + SUGGESTION
MAGAZINE = "http://halyard.test/2026/the-last-lighthouse-keepers/"
END_CARD = "http://fernwood.test" + END_CARD_PATH
INVOICE_PAGE = "http://tallyhaus.test/invoices/new/"
INVOICE = {"customer": "Kiln Coffee", "amount": "640.00", "due": "31 Oct 2026",
           "note": "Thanks for the beans"}
AGENT = "scout"
AD_HOSTS = ("adsprout.test", "pixelmint.test")
THEMES = ROOT / "film" / "themes"

# The frame the recording is taken at, and the film's own. The window is drawn at one and a half
# device pixels to the logical one, so the browser lays out at 1600 by 900 and a zoom-in has
# pixels to spare before it reaches the film's 1920 by 1080.
OUTPUT_MODE = (2400, 1350)
OUTPUT_SCALE = 1.5
FILM_SIZE = (1920, 1080)
FPS = 30
FADE = 0.4
# A frame whose mean luma, on 0-255, is over the ceiling or more than the jump above the frames
# around it is a flash: a white frame on a dark site, which the reader sees as a blink.
FLASH_CEILING = 120
FLASH_JUMP = 18
FLASH_WINDOW = 6

# What the film may weigh, per file, which together stay under the 5 MB the page can afford.
BUDGET = {"omaweb.webm": 2_000_000, "omaweb.mp4": 2_600_000, "poster.webp": 250_000}

# What the recording's pixels are. wf-recorder converts the screen to limited-range BT.709 but
# flags the stream full range, and a browser that believes the flag shows black as grey, so the
# whole film looks washed out. The values pass through the edit as they are, so the finished edit is
# labelled with what it holds, and the files it is cut into carry the label on.
COLOURS = "setparams=range=tv:colorspace=bt709:color_primaries=bt709:color_trc=bt709"

SETTLE = 0.8
LOAD_SETTLE = 2.5


def log(message: str) -> None:
    print(message, flush=True)


def omaweb(browser: Path, *arguments: str, name: str = "film") -> subprocess.CompletedProcess:
    """One run of the command the browser was started from, as a client of that browser."""
    return subprocess.run([str(browser), *arguments, "--name", name],
                          capture_output=True, text=True, timeout=90, check=False)


def answer(beat: str, browser: Path, *arguments: str, name: str = "film") -> dict:
    result = omaweb(browser, *arguments, "--json", name=name)
    expect_ran(beat, result)
    return json.loads(result.stdout)


MODIFIERS = {"Primary": "ctrl", "Shift": "shift", "Alt": "alt"}


def keys(*sequence: str, settle: float = SETTLE) -> None:
    """Presses each binding in turn, through the compositor's virtual keyboard."""
    for binding in sequence:
        *modifiers, key = binding.split("+")
        command = ["wtype"]
        for modifier in modifiers:
            command += ["-M", MODIFIERS[modifier]]
        command += ["-k", key]
        for modifier in reversed(modifiers):
            command += ["-m", MODIFIERS[modifier]]
        subprocess.run(command, check=True)
        time.sleep(settle)


def type_text(text: str, delay: int = 160) -> None:
    """Types as a person would, a key at a time, so the Omnibar's rows arrive on camera."""
    subprocess.run(["wtype", "-d", str(delay), text], check=True)


def frame() -> bytes:
    return subprocess.run(["grim", "-t", "ppm", "-"], capture_output=True, check=True).stdout


def theme_colour(name: str, role: str) -> tuple[int, int, int]:
    with open(THEMES / f"{name}.toml", "rb") as handle:
        value = tomllib.load(handle)[role]
    return tuple(int(value[index:index + 2], 16) for index in (1, 3, 5))


def write_theme(name: str, config: Path) -> None:
    """Renders the shipped Omarchy template against a committed palette, as Omarchy itself would."""
    with open(THEMES / f"{name}.toml", "rb") as handle:
        colors = {key: value for key, value in tomllib.load(handle).items() if key != "mode"}
    staged = config / "theme.json.new"
    themes.render_theme_file(colors, staged, None)
    # A rename, so the browser's watcher never reads half a theme.
    os.replace(staged, config / "theme.json")


def seed(root: Path) -> tuple[Path, Path]:
    """The browser's data and configuration roots, set up as the film needs them.

    The search engine is Kestrel, asked for Engine suggestions; Agents are allowed; blocking has
    the shipped lists as a first run leaves them and rules for the magazine's ad network; and the
    theme is Retro 82, which the website wears. None of it reaches anything outside the film.
    """
    data, config = root / "data", root / "config"
    config.mkdir(parents=True)
    (config / "settings.json").write_text(
        json.dumps({"version": 1, "engine-suggestions": True}), encoding="utf-8")
    (config / "agents.json").write_text(json.dumps({"allow-agents": True}), encoding="utf-8")
    (config / "search-engines.json").write_text(json.dumps({
        "version": 3,
        "default": "kestrel",
        "engines": [{
            "id": "kestrel",
            "name": "Kestrel",
            "queryUrl": "http://kestrel.test/search?q={query}",
            "keyword": "k",
            "suggestUrl": "http://kestrel.test/suggest?q={query}",
        }],
    }), encoding="utf-8")
    keybindings = json.loads((ROOT / "assets" / "keybindings" / "default.json").read_text())
    keybindings["browser"][runtime.NEW_SPACE_BINDING] = "new-space"
    (config / "keybindings.json").write_text(json.dumps(keybindings), encoding="utf-8")
    data.mkdir()
    runtime.seed_content_blocking(
        str(data), user_rules=[f"||{host}^" for host in AD_HOSTS] + ["halyard.test##.ad"],
        disabled_sites=[])
    write_theme("retro-82", config)
    return data, config


class Recorder:
    """The compositor's output, recorded from before the first beat to after the last.

    `wf-recorder` writes at a constant rate, so a time on this script's clock is a time in the
    file once the recorder's own start is known. That is taken from the file afterwards: whatever
    the file is shorter than the time the recorder ran was lost at its start, before a first frame.
    """

    def __init__(self, path: Path) -> None:
        self.path = path
        # Its own log, beside the recording.
        self.log_path = path.with_suffix(".log")
        self.process: subprocess.Popen | None = None
        self.started = 0.0
        self.stopped = 0.0

    def start(self) -> None:
        self.started = time.monotonic()
        # Its log goes to a file: it writes a line a frame, and a pipe nobody reads fills and stops
        # it mid-film.
        with open(self.log_path, "w") as written:
            self.process = subprocess.Popen(
                ["wf-recorder", "-y", "-r", str(FPS), "-c", "libx264", "-p", "preset=ultrafast",
                 "-p", "crf=12", "-x", "yuv420p", "-f", str(self.path)],
                stdin=subprocess.DEVNULL, stdout=written, stderr=written)
        time.sleep(1.5)
        if self.process.poll() is not None:
            raise BeatMissed("Recording", self.log_path.read_text().strip())

    def now(self) -> float:
        return time.monotonic() - self.started

    def stop(self) -> float:
        """Stops the recording and returns how late its first frame came."""
        assert self.process is not None
        self.stopped = time.monotonic()
        self.process.send_signal(signal.SIGINT)
        # It can finish the file and then wait on the compositor for a frame that a still screen
        # never sends. Once the encoder has written its closing summary, the file is whole.
        deadline = time.monotonic() + 60
        while self.process.poll() is None:
            if "kb/s:" in self.log_path.read_text(errors="replace"):
                time.sleep(1.0)
                self.process.kill()
                self.process.wait()
                break
            if time.monotonic() > deadline:
                self.process.kill()
                raise BeatMissed("Recording", "the recorder did not finish its file")
            time.sleep(0.5)
        duration = float(subprocess.run(
            ["ffprobe", "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0",
             str(self.path)], capture_output=True, text=True, check=True).stdout)
        return max(0.0, (self.stopped - self.started) - duration)


def wait_for(beat: str, what: str, ready, timeout: float = 20.0):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        found = ready()
        if found:
            return found
        time.sleep(0.2)
    raise BeatMissed(beat, f"{what} never happened")


def reported(server: FixtureServer, host: str, path: str, since: int = 0):
    return lambda: [report for report in server.reports()[since:]
                    if report["host"] == host and report["path"] == path]


def drive(browser: Path, server: FixtureServer, recorder: Recorder, config: Path) -> list[dict]:
    """Plays the beats in front of the recorder and checks each one. Returns when each played."""
    marks: list[dict] = []

    # A beat ends where its last moment on camera does, and its checks run after that, so a slow
    # answer is cut out of the film rather than held on screen. What the edit draws over a beat,
    # the pointer and the Agent's commands, comes with it.
    def mark(beat: Beat, start: float, end: float, **drawn) -> None:
        marks.append({"beat": beat.name, "start": start, "end": end, **drawn})
        log(f"{beat.name}: {start:.1f}s to {end:.1f}s")

    beats = BEAT_NAMED

    # The stage, which the film cuts: two Spaces and their tabs, the Work Space's two pages split.
    # Each Space's first page is typed into the blank tab it opened on, as a reader fills one, and
    # the rest are opened in new tabs.
    def type_into_this_tab(address: str) -> None:
        keys("Primary+l", settle=0.6)
        type_text(address, delay=10)
        keys("Return", settle=LOAD_SETTLE)

    type_into_this_tab(PERSONAL_TABS[0])
    # A Space is made by its key, as a reader makes one, since a Space the command line makes is an
    # Agent's. The keyboard is put in the sidebar first, because the Start page's Omnibar keeps the
    # key for itself, and the key is pressed again until the Space is there.
    def has_work() -> bool:
        return any(space["name"] == "Work"
                   for space in answer("Stage", browser, "spaces")["spaces"])

    for _ in range(3):
        keys("Primary+e", runtime.NEW_SPACE_BINDING)
        type_text("Work")
        keys("Return", settle=LOAD_SETTLE)
        if has_work():
            break
        keys("Escape")
    wait_for("Stage", "the Work Space", has_work)
    type_into_this_tab(WORK_TABS[0])
    for space, addresses in (("Work", WORK_TABS), ("Personal", PERSONAL_TABS)):
        for address in addresses[1:]:
            expect_ran("Stage", omaweb(browser, "open", address, "--space", space, "--new"))
    # A tab loads when it is first shown, so each is shown once.
    for address in WORK_TABS + PERSONAL_TABS:
        parts = urllib.parse.urlsplit(address)
        expect_ran("Stage", omaweb(browser, "focus", address))
        wait_for("Stage", f"{address} to load", reported(server, parts.netloc, parts.path))
    expect_ran("Stage", omaweb(browser, "focus", WORK_TABS[0]))
    expect_ran("Stage", omaweb(browser, "run", "add-split"))
    # The picker lists a new blank tab first, then the Space's other tabs. A key pressed before it
    # has opened goes to the page.
    time.sleep(1.5)
    keys("Down", "Return", settle=LOAD_SETTLE)
    expect_ran("Stage", omaweb(browser, "space", "Personal"))
    expect_ran("Stage", omaweb(browser, "focus", PERSONAL_TABS[-1]))
    time.sleep(LOAD_SETTLE)

    beat = beats["Omnibar"]
    start = recorder.now()
    time.sleep(CAPTION_LEAD)
    keys("Primary+l", settle=0.6)
    type_text(TYPED)
    time.sleep(1.8)
    keys(*["Down"] * SUGGESTION_ROW, settle=0.5)
    # The results, on screen for the hints that follow.
    keys("Return", settle=LOAD_SETTLE)
    end = recorder.now()
    expect_asked(beat.name, server, "kestrel.test", f"/suggest?q={TYPED}")
    expect_current_tab(beat.name, answer(beat.name, browser, "tabs", "--space", "Personal"),
                       SEARCH)
    mark(beat, start, end)

    # The keyboard is in the results page the Omnibar left on show, which is where `f` is a key.
    beat = beats["Hints"]
    since = len(server.reports())
    start = recorder.now()
    time.sleep(CAPTION_LEAD)
    keys("f", settle=2.6)
    end = recorder.now()
    shown = server.reports()[since:]
    keys("Escape")
    expect_hints(beat.name, shown, "kestrel.test")
    mark(beat, start, end)

    # The pointer rests on each Space's square in turn, and the square names its Space, then the
    # reader switches to the one it rests on. The recording has no pointer, so the window holds
    # each name on show and the edit draws the pointer where the window says the square is.
    beat = beats["Space switch"]

    def hold(space: str) -> tuple[float, float]:
        """Holds `space`'s name on show and returns where its square is."""
        return hover_point(beat.name, answer(beat.name, browser, "film-hover", space))

    def let_go() -> None:
        expect_ran(beat.name, omaweb(browser, "film-hover"))

    # Off camera: where each square is. The squares do not move while the reader stays.
    personal, work = hold("Personal"), hold("Work")
    let_go()
    moves = []

    def glide(to: tuple[float, float]) -> None:
        moves.append({"at": round(recorder.now() - start, 3), "x": to[0], "y": to[1]})

    start = recorder.now()
    time.sleep(CAPTION_LEAD)
    # In from the page above Personal's square, then onto it.
    glide((personal[0] + 0.06, personal[1] - 0.14))
    glide(personal)
    time.sleep(MOVE)
    # Long enough for the name to come up, after its 400 ms delay, and be read.
    hold("Personal")
    time.sleep(1.3)
    let_go()
    glide(work)
    time.sleep(MOVE)
    hold("Work")
    time.sleep(0.9)
    keys("Primary+2", settle=1.5)
    let_go()
    end = recorder.now()
    expect_space_on_show(beat.name, answer(beat.name, browser, "spaces"), "Work")
    mark(beat, start, end, pointer=moves)

    beat = beats["Sidebar"]
    before = server.reports()
    start = recorder.now()
    time.sleep(CAPTION_LEAD)
    keys("Primary+b", settle=3.0)
    end = recorder.now()
    expect_wider(beat.name, before, server.reports(), "quillstack.test")
    mark(beat, start, end)

    # Off camera: the sidebar back, and the magazine on show in Personal.
    keys("Primary+b", "Primary+1")
    expect_ran("Stage", omaweb(browser, "focus", MAGAZINE))
    time.sleep(LOAD_SETTLE)

    # The magazine as it stands, its ad slots empty, then the camera on the blocked count, and the
    # page loaded again under it so the count is seen to climb.
    beat = beats["Blocking"]
    since = len(server.reports())
    start = recorder.now()
    time.sleep(CAPTION_LEAD + MOVE + 0.2)
    expect_ran(beat.name, omaweb(browser, "run", "reload"))
    path = urllib.parse.urlsplit(MAGAZINE).path
    wait_for(beat.name, "the magazine to load", reported(server, "halyard.test", path, since))
    time.sleep(2.4)
    end = recorder.now()
    expect_blocked(beat.name, server, server.reports()[since:], "halyard.test", AD_HOSTS)
    mark(beat, start, end)

    # Off camera: a new tab, whose Start page is the theme's road and sun, so the change shows.
    keys("Primary+t", settle=LOAD_SETTLE)

    beat = beats["Theme"]
    # A point in the sidebar, which is the theme's darkest background.
    sample = (int(OUTPUT_MODE[0] * 0.01), int(OUTPUT_MODE[1] * 0.5))
    was = pixel(frame(), *sample)
    start = recorder.now()
    # Long enough to read the caption over the old theme, so the change it names comes after it,
    # on the same frame.
    time.sleep(THEME_HOLD)
    write_theme("tokyo-night", config)
    time.sleep(3.0)
    end = recorder.now()
    expect_repainted(beat.name, was, pixel(frame(), *sample),
                     theme_colour("retro-82", "dark_background"),
                     theme_colour("tokyo-night", "dark_background"))
    mark(beat, start, end)

    # Off camera: the new tab put away unopened.
    keys("Escape")

    # Each command the Agent runs is put on screen as it runs it, in place of the caption.
    beat = beats["Agent"]
    lines = []

    def show_command(*command: str) -> None:
        lines.append({"at": round(recorder.now() - start, 3), "text": "$ " + " ".join(command)})

    start = recorder.now()
    time.sleep(CAPTION_LEAD)
    show_command("omaweb", "space", "new", "Invoices")
    created = omaweb(browser, "space", "new", "Invoices", name=AGENT)
    expect_ran(beat.name, created)
    space = created.stdout.strip()
    expect_ran(beat.name, omaweb(browser, "space", space))
    expect_space_on_show(beat.name, answer(beat.name, browser, "spaces"), "Invoices")
    time.sleep(COMMAND_READ)
    show_command("omaweb", "open", INVOICE_PAGE)
    opened = omaweb(browser, "open", INVOICE_PAGE, "--space", space, name=AGENT)
    expect_ran(beat.name, opened)
    tab = opened.stdout.strip()
    # An Agent's tab never takes the reader's focus, so the reader selects it to watch, and rests
    # the keyboard in the sidebar: an Agent waits while the reader's keyboard is in its page.
    expect_ran(beat.name, omaweb(browser, "focus", tab))
    keys("Primary+e", settle=0)
    wait_for(beat.name, "the invoice form to load",
             reported(server, "tallyhaus.test", urllib.parse.urlsplit(INVOICE_PAGE).path))
    time.sleep(COMMAND_READ)
    show_command("omaweb", "look")
    look = answer(beat.name, browser, "look", "--tab", tab, name=AGENT)["look"]
    time.sleep(COMMAND_READ)
    # One batch, as the skill teaches, sent with Enter from the last field as a person would: a
    # click waits while the reader is in Omaweb's own controls, and here the reader is in the
    # sidebar.
    steps = [
        f'select {target(beat.name, look, "Customer")} "{INVOICE["customer"]}"',
        f'fill {target(beat.name, look, "Amount")} {INVOICE["amount"]}',
        f'fill {target(beat.name, look, "Note to customer")} "{INVOICE["note"]}"',
        f'fill {target(beat.name, look, "Due date")} "{INVOICE["due"]}"',
        "press Enter",
    ]
    show_command("omaweb", "do", *(f"'{step}'" for step in steps[:2]), "…")
    expect_ran(beat.name, omaweb(browser, "do", *steps, "--tab", tab, name=AGENT))
    # The invoice it sent, long enough to read before the end card.
    time.sleep(1.7)
    end = recorder.now()
    expect_invoice(beat.name, server, INVOICE)
    mark(beat, start, end, lines=lines)

    # Off camera: the end card in a tab of the reader's own, with the sidebar out of the way.
    expect_ran("Stage", omaweb(browser, "open", END_CARD, "--space", "Personal", "--new"))
    expect_ran("Stage", omaweb(browser, "space", "Personal"))
    expect_ran("Stage", omaweb(browser, "focus", END_CARD))
    keys("Primary+b")
    wait_for("Stage", "the end card to load", reported(server, "fernwood.test", END_CARD_PATH))
    time.sleep(LOAD_SETTLE)

    beat = beats["End"]
    start = recorder.now()
    time.sleep(2.6)
    end = recorder.now()
    mark(beat, start, end)
    return marks


def record(browser: Path, out: Path) -> None:
    """Runs under the compositor: the browser, the beats and the raw recording, and the marks."""
    expect_sites_on_loopback("Sites")
    root = Path(tempfile.mkdtemp(prefix="omaweb-film-"))
    server = FixtureServer(("127.0.0.1", 80))
    server.start()
    data, config = seed(root)
    subprocess.run(["wlr-randr", "--output", "HEADLESS-1", "--custom-mode",
                    "{}x{}".format(*OUTPUT_MODE), "--scale", str(OUTPUT_SCALE)], check=True)
    environment = dict(os.environ, OMAWEB_DATA_ROOT=str(data), OMAWEB_CONFIG_ROOT=str(config),
                       OMAWEB_NO_OMARCHY_TEMPLATE="1", QT_QPA_PLATFORM="wayland")
    with open(out / "browser.log", "w") as browser_log:
        process = subprocess.Popen(["dbus-run-session", "--", str(browser)], env=environment,
                                   stdout=subprocess.DEVNULL, stderr=browser_log,
                                   start_new_session=True)
    recorder = Recorder(out / "raw.mkv")
    try:
        wait_for("Stage", "the browser to answer",
                 lambda: omaweb(browser, "spaces").returncode == 0, timeout=60)
        time.sleep(LOAD_SETTLE)
        recorder.start()
        marks = drive(browser, server, recorder, config)
        # Off camera: a new tab, whose road moves. The recorder only writes a frame when the screen
        # changes, so a still last page would end the file early, and the start it is measured
        # from would come out late.
        keys("Primary+t", settle=2.5)
        late = recorder.stop()
        strays = server.strays()
        if strays:
            raise BeatMissed("Sites", f"the browser asked for names no site serves: {strays}")
        for entry in marks:
            entry["start"] -= late
            entry["end"] -= late
        (out / "marks.json").write_text(json.dumps(marks, indent=2) + "\n", encoding="utf-8")
    finally:
        # Stopped rather than killed, so the raw recording of a run that failed can be watched.
        if recorder.process and recorder.process.poll() is None:
            recorder.process.send_signal(signal.SIGINT)
            try:
                recorder.process.wait(timeout=30)
            except subprocess.TimeoutExpired:
                recorder.process.kill()
        os.killpg(process.pid, signal.SIGTERM)
        server.stop()
        shutil.rmtree(root, ignore_errors=True)




def eased(keyframes: list[tuple[float, float]], variable: str) -> str:
    """An ffmpeg expression that holds each value, then eases to the next over `MOVE` seconds."""
    expression = f"{keyframes[-1][1]:.5f}"
    for (_, value), (at, following) in reversed(list(zip(keyframes, keyframes[1:]))):
        progress = f"clip(({variable}-{at:.3f})/{MOVE},0,1)"
        smooth = f"{progress}*{progress}*(3-2*{progress})"
        expression = (f"if(lt({variable},{at:.3f}),{value:.5f},"
                      f"if(lt({variable},{at + MOVE:.3f}),"
                      f"{value:.5f}+({following - value:.5f})*{smooth},{expression}))")
    return expression


def camera(beat: Beat) -> str:
    """The zoompan filter that follows a beat's focus, each place framed at the film's aspect.

    The window and the film are both 16:9, so a place keeps its shape when it is as wide, as a
    fraction of the window, as it is high: the shorter side is grown about the place's centre.
    """
    keyframes = []
    for at, place in beat.focus:
        left, top, wide, high = place or (0.0, 0.0, 1.0, 1.0)
        side = max(wide, high)
        left = min(max(left + (wide - side) / 2, 0.0), 1.0 - side)
        top = min(max(top + (high - side) / 2, 0.0), 1.0 - side)
        keyframes.append((at, side, left, top))
    side = eased([(at, value) for at, value, _, _ in keyframes], "it")
    x = eased([(at, value) for at, _, value, _ in keyframes], "it")
    y = eased([(at, value) for at, _, _, value in keyframes], "it")
    return (f"zoompan=z='1/({side})':x='({x})*iw':y='({y})*ih':d=1:fps={FPS}"
            f":s={FILM_SIZE[0]}x{FILM_SIZE[1]}")


# The pointer drawn over the Space switch, whose tip is its top left corner. The recording has no
# pointer of its own, so the window holds a Space's name on show and this is drawn where the
# window says the Space's square is. It is sized for the recording's pixels, before the camera.
POINTER = ROOT / "film" / "pointer.png"


def pointer(entry: dict) -> str | None:
    """The overlay filter that draws the pointer over a beat, or `None` for a beat without one.

    `entry["pointer"]` lists where it moves, each a time into the beat and a fraction of the
    window. It comes in at the first and glides to each next one, and stays to the beat's end.
    """
    moves = entry.get("pointer")
    if not moves:
        return None
    x = eased([(move["at"], move["x"] * OUTPUT_MODE[0]) for move in moves], "t")
    y = eased([(move["at"], move["y"] * OUTPUT_MODE[1]) for move in moves], "t")
    length = entry["end"] - entry["start"]
    return (f"overlay=x='{x}':y='{y}'"
            f":enable='between(t,{moves[0]['at']:.3f},{length:.3f})'")


@dataclasses.dataclass
class Cue:
    """A line of text burned into the film: a beat's caption, or a command an Agent ran."""
    start: float
    end: float
    text: str
    mono: bool


def positions(marks: list[dict]) -> list[float]:
    """Where each beat starts in the film. Each overlaps the one before it by the fade."""
    starts, position = [], 0.0
    for entry in marks:
        starts.append(position)
        position += entry["end"] - entry["start"] - FADE
    return starts


def cues(marks: list[dict]) -> list[Cue]:
    """Every line the film shows, in order, timed in the film.

    A beat's caption comes in 0.3 s after the beat does. A command the beat lists under `lines`
    takes the caption's place at the time it was run, and the last line stays until the beat
    fades. A beat without a caption shows nothing.
    """
    shown = []
    for entry, position in zip(marks, positions(marks)):
        caption = BEAT_NAMED[entry["beat"]].caption
        if not caption:
            continue
        end = position + entry["end"] - entry["start"] - FADE
        lines = [(position + 0.3, caption, False)] + [
            (position + line["at"], line["text"], True) for line in entry.get("lines", [])]
        for (start, text, mono), following in zip(lines, lines[1:] + [(end, "", False)]):
            shown.append(Cue(round(start, 3), round(following[0], 3), text, mono))
    return shown


def timestamp(seconds: float) -> str:
    minutes, seconds = divmod(seconds, 60)
    return f"{int(minutes):02d}:{seconds:06.3f}"


def compose(out: Path) -> None:
    """Cuts the raw recording to its beats, follows each beat's focus, and captions it."""
    marks = json.loads((out / "marks.json").read_text(encoding="utf-8"))
    fonts = ROOT / "website" / "assets" / "fonts"
    font = {False: fonts / "Tomorrow-SemiBold.woff2", True: fonts / "IoskeleyMono-Medium.woff2"}
    starts = positions(marks)
    pointed = [entry for entry in marks if pointer(entry)]
    chains = []
    # One pointer image, split for each beat that draws it.
    if pointed:
        chains.append(f"[1:v]split={len(pointed)}"
                      + "".join(f"[p{index}]" for index in range(len(pointed))))
    for index, entry in enumerate(marks):
        beat = BEAT_NAMED[entry["beat"]]
        trimmed = (f"[0:v]trim=start={entry['start']:.3f}:end={entry['end']:.3f},"
                   "setpts=PTS-STARTPTS")
        overlay = pointer(entry)
        if overlay:
            chains.append(f"{trimmed}[r{index}]")
            trimmed = f"[r{index}][p{pointed.index(entry)}]{overlay}"
        chains.append(f"{trimmed},{camera(beat)},setsar=1,format=yuv420p[b{index}]")
    joined = "[b0]"
    for index in range(1, len(marks)):
        chains.append(f"{joined}[b{index}]xfade=transition=fade:duration={FADE}"
                      f":offset={starts[index]:.3f}[j{index}]")
        joined = f"[j{index}]"
    # Each line is read from a file, so a command's quotes and colons reach the film as typed.
    lines = out / "lines"
    shutil.rmtree(lines, ignore_errors=True)
    lines.mkdir()
    shown = cues(marks)
    texts = []
    for index, cue in enumerate(shown):
        (lines / f"{index}.txt").write_text(cue.text, encoding="utf-8")
        alpha = (f"if(lt(t,{cue.start + 0.3:.3f}),(t-{cue.start:.3f})/0.3,"
                 f"if(gt(t,{cue.end - 0.3:.3f}),({cue.end:.3f}-t)/0.3,1))")
        size = 40 if cue.mono else 46
        texts.append(
            f"drawtext=fontfile='{font[cue.mono]}':textfile='{lines / f'{index}.txt'}'"
            f":expansion=none:fontsize={size}:fontcolor=white"
            f":box=1:boxcolor=0x05182e@0.82:boxborderw=22|34:x=(w-text_w)/2:y=h-text_h-84"
            f":alpha='{alpha}':enable='between(t,{cue.start:.3f},{cue.end:.3f})'")
    chains.append(f"{joined}{','.join(texts + [COLOURS])}[film]")
    master = out / "master.mkv"
    pointer_input = ["-i", str(POINTER)] if pointed else []
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", str(out / "raw.mkv"), *pointer_input,
                    "-filter_complex", ";".join(chains), "-map", "[film]", "-an",
                    "-c:v", "libx264", "-preset", "veryfast", "-crf", "8", str(master)],
                   check=True)
    encode(out, master)
    expect_no_flash("Film", out / "omaweb.mp4")
    poster_beat, poster_at = POSTER
    poster = next(start for entry, start in zip(marks, starts)
                  if entry["beat"] == poster_beat) + poster_at
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-ss", f"{poster:.3f}", "-i", str(master),
                    "-frames:v", "1", "-c:v", "libwebp", "-quality", "80",
                    str(out / "poster.webp")], check=True)
    vtt = ["WEBVTT", ""]
    for cue in shown:
        vtt += [f"{timestamp(cue.start)} --> {timestamp(cue.end)}", cue.text, ""]
    (out / "captions.vtt").write_text("\n".join(vtt), encoding="utf-8")
    expect_within("Film", [out / name for name in BUDGET], sum(BUDGET.values()))
    for name, limit in BUDGET.items():
        expect_within(name, [out / name], limit)


def frame_brightness(film: Path) -> list[float]:
    """The mean luma of every frame of `film`, on a 0-255 scale, from ffmpeg's `signalstats`."""
    filters = "signalstats,metadata=print:key=lavfi.signalstats.YAVG:file=-"
    report = subprocess.run(["ffmpeg", "-v", "error", "-i", str(film), "-vf", filters,
                             "-f", "null", "-"], capture_output=True, text=True, check=True).stdout
    return [float(level) for level in re.findall(r"YAVG=([0-9.]+)", report)]


def flashed_frames(levels: list[float]) -> list[tuple[int, float]]:
    """The frames that are light outright or jump above the frames around them, as (index, level).

    A white frame on a dark site is both. Around a frame is the median of the six before it and of
    the six after it, so a bright scene that fades in is not a flash and a single bright frame is.
    """
    flashed = []
    for index, level in enumerate(levels):
        before = levels[max(0, index - FLASH_WINDOW):index]
        after = levels[index + 1:index + 1 + FLASH_WINDOW]
        around = [statistics.median(side) for side in (before, after) if side]
        if level > FLASH_CEILING or (around and level - max(around) > FLASH_JUMP):
            flashed.append((index, level))
    return flashed


def expect_no_flash(beat: str, film: Path) -> None:
    flashed = flashed_frames(frame_brightness(film))
    if flashed:
        frames = ", ".join(f"{index / FPS:.2f}s (mean {level:.0f})" for index, level in flashed[:5])
        raise BeatMissed(beat, f"{film.name} flashes a light frame at {frames}")


def encode(out: Path, master: Path) -> None:
    """The two codecs the page offers, each at the best quality that fits its share of the budget.

    Screen recordings vary a great deal in how well they compress, so rather than one setting that
    is too generous one release and too mean the next, each starts high and steps down until it
    fits. The last step that still does not fit is left on disk and the size check refuses it.
    """
    codecs = {
        # VP9 rather than AV1: every Arch ffmpeg has libvpx, and the aarch64 build has no
        # software AV1 encoder. Every browser that plays AV1 in WebM plays VP9 too.
        "omaweb.webm": (["-c:v", "libvpx-vp9", "-b:v", "0", "-row-mt", "1", "-deadline", "good",
                         "-cpu-used", "2", "-g", "300"], range(36, 60, 4)),
        "omaweb.mp4": (["-c:v", "libx264", "-preset", "slower", "-profile:v", "high",
                        "-movflags", "+faststart"], range(24, 40, 2)),
    }
    for name, (arguments, ladder) in codecs.items():
        for quality in ladder:
            subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", str(master), "-an",
                            "-pix_fmt", "yuv420p", *arguments, "-crf", str(quality),
                            str(out / name)], check=True)
            size = (out / name).stat().st_size
            log(f"{name}: {size} bytes at crf {quality}")
            if size <= BUDGET[name]:
                break


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("step", choices=("record", "compose"))
    parser.add_argument("--browser", type=Path, default=ROOT / "build" / "ci" / "omaweb-browser")
    parser.add_argument("--out", type=Path, default=ROOT / "build" / "film")
    arguments = parser.parse_args()
    arguments.out.mkdir(parents=True, exist_ok=True)
    try:
        if arguments.step == "record":
            record(arguments.browser.resolve(), arguments.out)
        else:
            compose(arguments.out)
    except BeatMissed as missed:
        print(f"the film was not made: {missed}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
