#!/usr/bin/env python3
"""Records the website's introductory film from the real browser.

The website opens on a film of Omaweb in use: the Omnibar, a Space switch, split view with the
sidebar hidden, link hints, blocking, a theme change, an Agent at work and the install line. It
has to show the real browser on its real engine and stay true to each release, so it is recorded
by this script rather than by hand.
"""

from __future__ import annotations

import argparse
import contextlib
import dataclasses
import html
import http.server
import json
import math
import os
import re
import shlex
import shutil
import signal
import socket
import statistics
import struct
import subprocess
import sys
import tempfile
import threading
import time
import tomllib
import typing
import urllib.parse
import zlib
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

# Where the desktop's browser asks Kestrel for its suggestions. Pages reach the sites through
# Chromium's host resolver rules, but suggestions are asked for by the browser's own Qt network
# stack, which resolves through the system, and on the desktop no hosts file names a `.test` site.
# So the profile names Kestrel's suggestions there by the server's address and port, and the server
# answers a suggestion asked of that address as Kestrel's.
KESTREL_ADDRESS = "127.0.0.1"

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

    The end card wears `opening`, the palette of the theme the film opens and ends in: Retro 82
    unless another is given.
    """

    def __init__(self, address: tuple[str, int], opening: dict[str, str] | None = None) -> None:
        self.opening = opening or film_palette("retro-82")
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
                named = (self.headers.get("Host") or "").split(":")[0].lower()
                suggestion = urllib.parse.urlsplit(self.path).path == "/suggest"
                return "kestrel.test" if named == KESTREL_ADDRESS and suggestion else named

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
                    self.page(SITE_ROOT / "_film" / "end.html", fixtures.opening)
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


def step_to_suggestion(beat: str, selected, down, title: str) -> None:
    """Presses Down until the Omnibar's selected row is the engine's suggestion `title`.

    `selected` answers the row the Omnibar has selected, as `film-omnibar` gives it, or None.
    The rows are not counted, since the row the beat starts from is not fixed: a new list
    selects none, and a pointer that crosses the rows selects the one under it.
    """
    wanted = {"kind": "suggestion", "title": title}
    for _ in range(SUGGESTION_STEPS):
        if selected() == wanted:
            return
        down()
    if selected() != wanted:
        raise BeatMissed(beat, f"Down never selected the suggestion {title!r}")


def expect_asked(beat: str, server: FixtureServer, host: str, path: str,
                 timeout: float = 0.0) -> None:
    """`host` was asked for `path`, or is within `timeout` seconds."""
    deadline = time.monotonic() + timeout
    while path not in server.paths(host):
        if time.monotonic() >= deadline:
            raise BeatMissed(beat, f"{host} was never asked for {path}")
        time.sleep(0.2)


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


def expect_blocked_list(beat: str, site: dict, hosts: tuple[str, ...]) -> None:
    """`site` is the window's answer to `film-site`: Site information has to be open at its
    blocked list, and the list has to name a request to each of `hosts`."""
    if not site.get("open"):
        raise BeatMissed(beat, "Site information is not open")
    if site.get("detail") != "blocked":
        raise BeatMissed(beat, f"Site information is at {site.get('detail') or 'its top'}, "
                               "not the blocked list")
    listed = site.get("listed", [])
    missing = [host for host in hosts
               if not any(label.startswith(host + "/") or label == host for label in listed)]
    if missing:
        raise BeatMissed(beat, f"the blocked list names no request to {', '.join(missing)}; "
                               f"it lists {', '.join(listed) or 'nothing'}")


# How far the camera keeps from what it frames, as a fraction of the window.
FRAME_MARGIN = 0.03


def framing(site: dict) -> tuple[float, float, float, float]:
    """The place that frames Site information's button and card from `film-site`'s answer,
    as a fraction of the window: (left, top, width, height)."""
    width, height = site["windowWidth"], site["windowHeight"]
    rectangles = [site["button"], site["card"]]
    left = max(0.0, min(rect["x"] for rect in rectangles) / width - FRAME_MARGIN)
    top = max(0.0, min(rect["y"] for rect in rectangles) / height - FRAME_MARGIN)
    right = min(1.0, max(rect["x"] + rect["width"] for rect in rectangles) / width + FRAME_MARGIN)
    bottom = min(1.0, max(rect["y"] + rect["height"] for rect in rectangles) / height
                 + FRAME_MARGIN)
    return (left, top, right - left, bottom - top)


def hover_point(beat: str, held: dict) -> tuple[float, float]:
    """Where a pointer resting on what the window held would be, as a fraction of the window.

    `held` is the window's answer to `film-hover` or `film-row`: the rectangle of the Space's
    footer button or of the row's Stop, and the window's size, in the same logical pixels.
    """
    if not held.get("ok"):
        raise BeatMissed(beat, f"the window held nothing to point at: {held.get('error', held)}")
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


# Where the Theme beat samples the chrome, as a fraction of the frame: in the sidebar, which is the
# theme's darkest background.
CHROME_SAMPLE = (0.01, 0.5)


def chrome_sample(frame: bytes) -> tuple[int, int, int]:
    """The chrome's colour in a frame, at the same place whatever size the frame was taken at."""
    width, height = (int(field) for field in frame.split(maxsplit=3)[1:3])
    return pixel(frame, int(width * CHROME_SAMPLE[0]), int(height * CHROME_SAMPLE[1]))


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


def expect_acts_on_the_row(beat: str, lines: list[str]) -> None:
    """The Agent tab's row named the Agent's last act after each command, and a new one each time.

    `lines` is the row's line under its title, from the window's answer to `film-row --where`,
    read after each command in turn.
    """
    for line in lines:
        if not line.startswith(f"{AGENT} · "):
            raise BeatMissed(beat, f"the Agent's row read {line!r}, not {AGENT}'s last act")
    if len(set(lines)) != len(lines):
        raise BeatMissed(beat, f"the Agent's row did not change between commands: {lines}")


def expect_handed_back(beat: str, result: subprocess.CompletedProcess) -> None:
    """After Stop, the window has no Agent's row for the tab: it is the reader's again.

    `result` is `film-row` asked for the tab once more, which the browser refuses for a tab no
    Agent is attached to.
    """
    if result.returncode == 0:
        raise BeatMissed(beat, "the Agent was still attached to its tab after Stop")
    said = (result.stderr or result.stdout or "").strip()
    if "No Agent is attached" not in said:
        raise BeatMissed(beat, f"film-row after Stop said {said!r}")


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


def expect_shared_memory(beat: str, statvfs=os.statvfs) -> None:
    """`/dev/shm` has room for what the browser's processes share, before the browser is started.

    Docker gives a container 64 MB of it unless `docker run` says otherwise, and in 64 MB the
    magazine's renderer was killed with exit code 9 at its first load (#672). The film was seen to
    use up to 163 MB, so `record_film.sh` asks for a gigabyte. A machine with no `/dev/shm` keeps
    its shared memory elsewhere.
    """
    try:
        shared = statvfs("/dev/shm")
    except OSError:
        return
    free = shared.f_bavail * shared.f_frsize // (1024 * 1024)
    if free < SHARED_MEMORY_MB:
        raise BeatMissed(beat, f"/dev/shm has {free} MB free, and the film wants {SHARED_MEMORY_MB} "
                               "MB: give the container more with --shm-size")


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

# Where the camera looks: a fraction of the window, (left, top, width, height), so it holds at any
# output size; `None` is the whole window, and `DESKTOP` the whole monitor the desktop film records.
DESKTOP = "desktop"
Place = tuple[float, float, float, float] | None | str


# The film's beats, in the order they play: what the caption says, empty for none, and, for each
# moment the camera moves, where it looks. Times are seconds into the beat.
@dataclasses.dataclass
class Beat:
    name: str
    caption: str
    focus: list[tuple[float, Place]]


# Where the parts the camera follows sit in the window, with the sidebar at its default width: the
# Omnibar a little below the middle of the page, the search results at the top left of the page,
# the Space squares at the foot of the sidebar with the page beside them, the address at the top
# of the sidebar, and the Agent's form.
OMNIBAR = (0.29, 0.28, 0.6, 0.6)
RESULTS = (0.2, 0.17, 0.4, 0.4)
SPACES = (0.0, 0.4, 0.6, 0.6)
ADDRESS = (0.0, 0.0, 0.42, 0.42)
FORM = (0.32, 0.03, 0.66, 0.66)
# The Agent tab's row near the top of the sidebar, with the start of its page beside it.
ROW = (0.0, 0.0, 0.42, 0.42)

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
    # The magazine reloads with its ad slots empty, then the camera closes on Site information's
    # button and card, at moments the beat times as it plays (`play`).
    Beat("Blocking", "Ads and trackers stopped before they load", [(0.0, None)]),
    Beat("Theme", "Your Omarchy theme reaches the whole browser", [(0.0, None)]),
    # Each command the Agent runs takes the caption's place as it runs, and its row in the sidebar
    # says what it did last. The camera then closes on the row for Stop, at a moment the beat
    # times as it plays (`play`).
    Beat("Agent", "An Agent works in a Space of its own",
         [(0.0, None), (CAPTION_LEAD + 1.4, FORM)]),
    Beat("End", "", [(0.0, None)]),
]
BEAT_NAMED = {beat.name: beat for beat in BEATS}


def theme_focus(back: float) -> list[tuple[float, Place]]:
    """The Theme beat's camera on the desktop, where the switch changes the wallpaper, the bar and
    the borders as well as the browser: once the caption is read it draws back to the whole
    monitor, which is on camera before the first switch, and at `back` it closes on the window
    again."""
    return [(0.0, None), (CAPTION_LEAD, DESKTOP), (back, None)]


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
# The most times the beat presses Down looking for the suggestion: twice round the six places Down
# steps through after "tra", Work's tracing guide, the trail shoe's tab, the engine's three
# suggestions and the typed text, since a list that arrives on the way selects none again.
SUGGESTION_STEPS = 12
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
# Where Omarchy's themes are installed, the reader's own first, as Omarchy looks for them.
OMARCHY_THEMES = (Path.home() / ".config" / "omarchy" / "themes",
                  Path("/usr/share/omarchy/themes"))
# The theme Omarchy has the desktop wearing, which the desktop film opens in. It is only read.
# Where Omarchy keeps the theme the desktop is wearing: the theme in `theme/`, its name in
# `theme.name`. `omarchy-theme-set` replaces both, and the desktop film's browser follows them.
DESKTOP_CURRENT = Path.home() / ".local" / "state" / "omarchy" / "current"
# How far apart, in 8-bit RGB, the chrome's colour in two themes has to be for the Theme beat's
# repaint check to tell them apart through the window's translucency.
THEMES_APART = 12
# The installed Omarchy theme the desktop's Theme beat changes to where none is named: the
# maintainer's choice, a grey-blue with its own wallpaper, well apart from their dark navy.
DEFAULT_CHANGE = "lumon"

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

# The free `/dev/shm` a recording starts with, in megabytes: `expect_shared_memory` says why.
SHARED_MEMORY_MB = 256

# What the film may weigh, per file, which together stay under the 5 MB the page can afford.
BUDGET = {"omaweb.webm": 2_000_000, "omaweb.mp4": 2_600_000, "poster.webp": 250_000}

# The card on the website's drive that plays each beat, as a clip of its own: the beat whole, with
# no caption and no fade, since the card's title says what the caption did and the drive moves
# between cards itself. The end card plays on none; the drive ends on the install.
CLIP_OF = {
    "Omnibar": "omnibar",
    "Hints": "keyboard",
    "Space switch": "spaces",
    "Sidebar": "pros",
    "Blocking": "blocking",
    "Theme": "theme",
    "Agent": "agents",
}
# The clips' width, about the widest a card on the drive draws one, and what they may weigh
# together: every one of them, each in two codecs with its poster.
CLIP_WIDTH = 1280
CLIPS_BUDGET = 4_000_000

# What the recording's pixels are. wf-recorder converts the screen to limited-range BT.709 but
# flags the stream full range, and a browser that believes the flag shows black as grey, so the
# whole film looks washed out. The values pass through the edit as they are, so the finished edit is
# labelled with what it holds, and the files it is cut into carry the label on.
COLOURS = "setparams=range=tv:colorspace=bt709:color_primaries=bt709:color_trc=bt709"

SETTLE = 0.8
LOAD_SETTLE = 2.5


def log(message: str) -> None:
    print(message, flush=True)


def would_run(command) -> None:
    """What a dry run prints in place of running `command`."""
    log("would run: " + shlex.join(command))


# What the script's commands to the browser run in: the session's own under cage, and on the
# desktop the film browser's, so they reach its socket rather than the reader's Omaweb.
CLIENT_ENVIRONMENT: dict | None = None


def omaweb(browser: Path, *arguments: str, name: str = "film") -> subprocess.CompletedProcess:
    """One run of the command the browser was started from, as a client of that browser."""
    return subprocess.run([str(browser), *arguments, "--name", name], env=CLIENT_ENVIRONMENT,
                          capture_output=True, text=True, timeout=90, check=False)


def answer(beat: str, browser: Path, *arguments: str, name: str = "film") -> dict:
    result = omaweb(browser, *arguments, "--json", name=name)
    expect_ran(beat, result)
    return json.loads(result.stdout)


MODIFIERS = {"Primary": "ctrl", "Shift": "shift", "Alt": "alt"}

# What is asked before each key is sent: nothing under cage, where the browser is the only window,
# and on the desktop whether the film's window has the keyboard, since a key sent anywhere else
# lands in the reader's own windows.
BEFORE_KEYS = None


def keys(*sequence: str, settle: float = SETTLE) -> None:
    """Presses each binding in turn, through the compositor's virtual keyboard."""
    for binding in sequence:
        if BEFORE_KEYS:
            BEFORE_KEYS()
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
    if BEFORE_KEYS:
        BEFORE_KEYS()
    subprocess.run(["wtype", "-d", str(delay), text], check=True)


def region(geometry: tuple[int, int, int, int]) -> str:
    """A rectangle of the layout as wf-recorder and grim take it: `x,y wxh`."""
    return "{},{} {}x{}".format(*geometry)


def frame_command(geometry: tuple[int, int, int, int] | None = None) -> list[str]:
    """What takes a frame: the whole output under cage, and the film's window on the desktop."""
    return ["grim", *(["-g", region(geometry)] if geometry else []), "-t", "ppm", "-"]


def frame(geometry: tuple[int, int, int, int] | None = None) -> bytes:
    return subprocess.run(frame_command(geometry), capture_output=True, check=True).stdout


def palette(path: Path) -> dict[str, str]:
    """An Omarchy theme's `colors.toml`: its colours, without its mode."""
    with open(path, "rb") as handle:
        return {key: value for key, value in tomllib.load(handle).items()
                if isinstance(value, str) and key != "mode"}


class Theme(typing.NamedTuple):
    """An Omarchy theme the film wears: its name, as `omarchy-theme-set` takes it, and its
    palette."""

    name: str
    colours: dict[str, str]


def film_palette(name: str) -> dict[str, str]:
    """One of the palettes committed for the film."""
    return palette(THEMES / f"{name}.toml")


def desktop_theme(current: Path = DESKTOP_CURRENT) -> Theme:
    """The theme the desktop is wearing, which the desktop film opens in and comes back to. A
    desktop with none current stops the run before it changes anything, since the Theme beat
    switches the desktop's theme and has to know which to put back."""
    colours, name = current / "theme" / "colors.toml", current / "theme.name"
    if not colours.is_file() or not name.is_file():
        raise BeatMissed("Theme", f"Omarchy has no current theme in {current}, so the run could "
                                  "not put the desktop's back after the Theme beat")
    theme = Theme(name.read_text(encoding="utf-8").strip(), palette(colours))
    log(f"the film opens in the desktop's theme, {theme.name}")
    return theme


def theme_changes(opening: Theme | None = None, change: Theme | None = None) -> list[Theme]:
    """The themes the film wears in turn: Retro 82 and then Tokyo Night in the container, and on
    the desktop its own theme, the one the Theme beat `change`s to, and its own again, so most of
    the film is in it."""
    if opening is None:
        return [Theme(name, film_palette(name)) for name in ("retro-82", "tokyo-night")]
    return [opening, change, opening]


def themes_apart(one: dict[str, str], other: dict[str, str]) -> float:
    """How far apart two themes' sidebars are, in 8-bit RGB."""
    return sum((a - b) ** 2 for a, b in zip(theme_colour(one, "dark_background"),
                                            theme_colour(other, "dark_background"))) ** 0.5


def installed_themes(directories: tuple[Path, ...] = OMARCHY_THEMES) -> dict[str, Path]:
    """Every Omarchy theme installed, by name, the reader's own ahead of the system's."""
    found: dict[str, Path] = {}
    for directory in directories:
        for colours in sorted(directory.glob("*/colors.toml")):
            found.setdefault(colours.parent.name, colours)
    return found


def change_theme(desktop: dict[str, str], name: str = DEFAULT_CHANGE,
                 directories: tuple[Path, ...] = OMARCHY_THEMES) -> Theme:
    """The installed Omarchy theme the Theme beat switches the desktop to, Lumon unless another
    is named. One too near the desktop's for the repaint check to tell the change stops the run
    before it changes anything, and names one that is far enough."""
    installed = installed_themes(directories)
    if name not in installed:
        raise BeatMissed("Theme", f"no Omarchy theme called {name} is installed in "
                                  + " or ".join(str(directory) for directory in directories))
    change = palette(installed[name])
    apart = themes_apart(desktop, change)
    if apart < THEMES_APART:
        candidates = {other: palette(colours) for other, colours in installed.items()}
        far_enough = sorted((themes_apart(desktop, theme), other)
                            for other, theme in candidates.items()
                            if other != name and themes_apart(desktop, theme) >= THEMES_APART)
        # The default where it will do, and otherwise the nearest theme the check can still tell
        # from the desktop's, so the film changes least.
        suggestion = (DEFAULT_CHANGE if any(other == DEFAULT_CHANGE for _, other in far_enough)
                      else far_enough[0][1] if far_enough else None)
        raise BeatMissed("Theme", f"{name}'s sidebar is {apart:.0f} from the desktop theme's, too "
                                  f"near for the Theme beat to show the change"
                                  + (f"; try --change-to {suggestion}" if suggestion else ""))
    log(f"the Theme beat changes to {name} and back, {apart:.0f} apart from the desktop's theme")
    return Theme(name, change)


def reader_sidebar_opacity(session: dict = os.environ) -> float | None:
    """The sidebar opacity the reader set over the theme's in their own Omaweb, or None where they
    set none. Their settings are read where Omaweb keeps them, and never written."""
    if session.get("OMAWEB_CONFIG_ROOT"):
        root = Path(session["OMAWEB_CONFIG_ROOT"])
    else:
        base = session.get("XDG_CONFIG_HOME") or Path(session.get("HOME", Path.home())) / ".config"
        root = Path(base) / "omaweb"
    try:
        value = json.loads((root / "settings.json").read_text(encoding="utf-8"))["sidebar-opacity"]
    except (OSError, ValueError, KeyError, TypeError):
        return None
    # Omaweb takes any number the file holds, as it does here, and nothing else.
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        return None
    return float(value)


def theme_colour(colours: dict[str, str], role: str) -> tuple[int, int, int]:
    value = colours[role]
    return tuple(int(value[index:index + 2], 16) for index in (1, 3, 5))


def write_theme(colours: dict[str, str], config: Path) -> None:
    """Renders the shipped Omarchy template against a palette, as Omarchy itself would."""
    staged = config / "theme.json.new"
    themes.render_theme_file(colours, staged, None)
    # A rename, so the browser's watcher never reads half a theme.
    os.replace(staged, config / "theme.json")


def seed(root: Path, opening: dict[str, str] | None, suggestions: str = "http://kestrel.test",
         sidebar_opacity: float | None = None) -> tuple[Path, Path]:
    """The browser's data and configuration roots, set up as the film needs them.

    The search engine is Kestrel, asked for Engine suggestions at the `suggestions` origin; Agents
    are allowed; blocking has the shipped lists as a first run leaves them and rules for the
    magazine's ad network; the theme is the `opening` palette, or the desktop's where there is
    none; and the sidebar is the theme's own opacity unless `sidebar_opacity` is set over it. None
    of it reaches anything outside the film.
    """
    data, config = root / "data", root / "config"
    config.mkdir(parents=True)
    settings = {"version": 1, "engine-suggestions": True}
    if sidebar_opacity is not None:
        settings["sidebar-opacity"] = sidebar_opacity
    (config / "settings.json").write_text(json.dumps(settings), encoding="utf-8")
    (config / "agents.json").write_text(json.dumps({"allow-agents": True}), encoding="utf-8")
    (config / "search-engines.json").write_text(json.dumps({
        "version": 3,
        "default": "kestrel",
        "engines": [{
            "id": "kestrel",
            "name": "Kestrel",
            "queryUrl": "http://kestrel.test/search?q={query}",
            "keyword": "k",
            "suggestUrl": suggestions + "/suggest?q={query}",
        }],
    }), encoding="utf-8")
    keybindings = json.loads((ROOT / "assets" / "keybindings" / "default.json").read_text())
    keybindings["browser"][runtime.NEW_SPACE_BINDING] = "new-space"
    (config / "keybindings.json").write_text(json.dumps(keybindings), encoding="utf-8")
    data.mkdir()
    runtime.seed_content_blocking(
        str(data), user_rules=[f"||{host}^" for host in AD_HOSTS] + ["halyard.test##.ad"],
        disabled_sites=[])
    if opening is not None:
        write_theme(opening, config)
    return data, config


def follow_desktop_theme(home: Path, current: Path = DESKTOP_CURRENT) -> None:
    """Lets the desktop film's browser, whose home is its own, see the desktop's current theme
    where Omaweb looks for it, so it follows `omarchy-theme-set` as the reader's Omaweb does. It
    reads the theme and writes nothing there."""
    state = home / ".local" / "state" / "omarchy"
    state.mkdir(parents=True, exist_ok=True)
    (state / "current").symlink_to(current, target_is_directory=True)


def recorder_command(path: Path, output: str | None = None) -> list[str]:
    """What records: the whole output, cage's only one, or on the desktop the monitor named
    `output`, so the Theme beat can show the desktop around the window."""
    return ["wf-recorder", "-y", *(["-o", output] if output else []), "-r", str(FPS),
            "-c", "libx264", "-p", "preset=ultrafast", "-p", "crf=12", "-x", "yuv420p",
            "-f", str(path)]


class Recorder:
    """The compositor's output, recorded from before the first beat to after the last.

    `wf-recorder` writes at a constant rate, so a time on this script's clock is a time in the
    file once the recorder's own start is known. That is taken from the file afterwards: whatever
    the file is shorter than the time the recorder ran was lost at its start, before a first frame.
    """

    def __init__(self, path: Path, output: str | None = None) -> None:
        self.path = path
        # The monitor recorded on the desktop, and `None` for cage's only output.
        self.output = output
        # Where the film's window is on the desktop, which `chrome_sample` reads, and `None`
        # under cage, whose output is the window.
        self.geometry: tuple[int, int, int, int] | None = None
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
                recorder_command(self.path, self.output),
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


def nearer(sample: tuple[int, int, int], one: tuple[int, int, int],
           other: tuple[int, int, int]) -> bool:
    """Whether a sample of the chrome is nearer the colour `one` than `other`."""
    return (sum((x - y) ** 2 for x, y in zip(sample, one))
            < sum((x - y) ** 2 for x, y in zip(sample, other)))


# How long a theme has to show in the chrome once it is put on, and how often the beat looks.
REPAINT_WITHIN = 3.0
REPAINT_LOOK = 0.05


def play_theme(beat: Beat, recorder: Recorder, worn: list[Theme], wear, mark) -> None:
    """The Theme beat: each of the `worn` themes after the first put on with `wear`, and the chrome
    checked to have repainted after each.

    Each theme is marked with the moment, in seconds into the beat, that the recorder saw its chrome
    show it. That is later than the recording shows it, so `cut_clips` replaces it with the time
    read from the recording (`theme_onsets`), which is what the website's page wears it at. On the desktop,
    where the whole monitor is recorded, the camera draws back to it for the switches and closes on
    the window again before the beat fades into the next.
    """
    was = chrome_sample(frame(recorder.geometry))
    start = recorder.now()
    themes = [[0.0, worn[0].name]]
    # Long enough to read the caption over the old theme, so the change it names comes after it,
    # on the same frame.
    time.sleep(THEME_HOLD)
    for old, new in zip(worn, worn[1:]):
        old_colour = theme_colour(old.colours, "dark_background")
        new_colour = theme_colour(new.colours, "dark_background")
        put_on = recorder.now()
        wear(new)
        while True:
            # The moment a frame is asked for is the moment it shows.
            at = recorder.now()
            now = chrome_sample(frame(recorder.geometry))
            if nearer(now, new_colour, old_colour) or at - put_on >= REPAINT_WITHIN:
                break
            time.sleep(REPAINT_LOOK)
        expect_repainted(beat.name, was, now, old_colour, new_colour)
        themes.append([round(at - start, 3), new.name])
        time.sleep(max(0.0, REPAINT_WITHIN - (recorder.now() - put_on)))
        was = now
    if recorder.output is None:
        mark(beat, start, recorder.now(), themes=themes)
        return
    focus = theme_focus(recorder.now() - start)
    time.sleep(MOVE + FADE + 0.3)
    mark(beat, start, recorder.now(), focus=focus, themes=themes)


def drive(browser: Path, server: FixtureServer, recorder: Recorder, worn: list[Theme],
          wear) -> list[dict]:
    """Plays the beats in front of the recorder and checks each one. Returns when each played.

    The film opens in the first of the `worn` themes, and the Theme beat has `wear` each after it.
    """
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
    # Down is pressed until the window says the suggestion is selected, which it can only be once
    # Kestrel has been asked for it.
    expect_asked(beat.name, server, "kestrel.test", f"/suggest?q={TYPED}", timeout=5.0)
    time.sleep(1.8)
    def selected() -> dict | None:
        return answer(beat.name, browser, "film-omnibar").get("row")

    def down() -> None:
        keys("Down", settle=0.5)

    step_to_suggestion(beat.name, selected, down, SUGGESTION)
    # Asked again just before Return, in case a list came in after the last look.
    step_to_suggestion(beat.name, selected, down, SUGGESTION)
    # The results, on screen for the hints that follow.
    keys("Return", settle=LOAD_SETTLE)
    end = recorder.now()
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

    # The magazine loaded again, its ad slots empty. Then the pointer clicks the Site information
    # button, the card opens, and the pointer opens its blocked tile, whose list of refused
    # requests names the ad network and the tracker and is held long enough to read. The
    # recording has no pointer, so the window says where the button and the tile are, presses each
    # as a click would, and the edit draws the pointer there.
    beat = beats["Blocking"]

    def site(*arguments: str) -> dict:
        return answer(beat.name, browser, "film-site", *arguments)

    moves = []

    def glide(to: tuple[float, float]) -> None:
        moves.append({"at": round(recorder.now() - start, 3), "x": to[0], "y": to[1]})

    since = len(server.reports())
    start = recorder.now()
    time.sleep(CAPTION_LEAD)
    expect_ran(beat.name, omaweb(browser, "run", "reload"))
    path = urllib.parse.urlsplit(MAGAZINE).path
    wait_for(beat.name, "the magazine to load", reported(server, "halyard.test", path, since))
    time.sleep(1.6)
    focus = [(0.0, None), (round(recorder.now() - start, 3), ADDRESS)]
    button = hover_point(beat.name, site("button"))
    # In from the page below and to the right of the button, then onto it.
    glide((button[0] + 0.08, button[1] + 0.1))
    glide(button)
    time.sleep(MOVE + 0.2)
    site("button", "--press")
    time.sleep(1.0)
    focus.append((round(recorder.now() - start, 3), framing(site())))
    glide(hover_point(beat.name, site("blocked")))
    time.sleep(MOVE + 0.2)
    site("blocked", "--press")
    time.sleep(0.4)
    shown = site()
    if framing(shown) != focus[-1][1]:
        focus.append((round(recorder.now() - start, 3), framing(shown)))
    # Long enough to read each host the list names.
    time.sleep(3.6)
    end = recorder.now()
    expect_blocked(beat.name, server, server.reports()[since:], "halyard.test", AD_HOSTS)
    expect_blocked_list(beat.name, shown, AD_HOSTS)
    mark(beat, start, end, pointer=moves, focus=focus)

    # Off camera: Site information put away as it was opened.
    site("button", "--press")

    # Off camera: a new tab, whose Start page is the theme's road and sun, so the change shows.
    keys("Primary+t", settle=LOAD_SETTLE)

    play_theme(beats["Theme"], recorder, worn, wear, mark)

    # Off camera: the new tab put away unopened.
    keys("Escape")

    # Each command the Agent runs is put on screen as it runs it, in place of the caption.
    beat = beats["Agent"]
    lines = []

    def show_command(*command: str) -> None:
        lines.append({"at": round(recorder.now() - start, 3), "text": "$ " + " ".join(command)})

    def row_where() -> dict:
        """Where the Agent tab's row has Stop, and its line, without holding it."""
        return answer(beat.name, browser, "film-row", tab, "--where")

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
    acts = [row_where()["line"]]
    time.sleep(COMMAND_READ)
    show_command("omaweb", "look")
    look = answer(beat.name, browser, "look", "--tab", tab, name=AGENT)["look"]
    acts.append(row_where()["line"])
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
    acts.append(row_where()["line"])
    # The invoice it sent, long enough to read.
    time.sleep(1.7)
    # Then the reader stops it from its row: the camera closes on the row, the pointer comes to
    # rest on it, Stop shows beside Close, and a click on Stop hands the tab back. The recording
    # has no pointer, so the window holds the row as hovered and the edit draws the pointer where
    # the window says Stop is.
    focus = [(0.0, None), (CAPTION_LEAD + 1.4, FORM), (round(recorder.now() - start, 3), ROW)]
    stop = hover_point(beat.name, row_where())
    time.sleep(MOVE)
    moves = []

    def glide(to: tuple[float, float]) -> None:
        moves.append({"at": round(recorder.now() - start, 3), "x": to[0], "y": to[1]})

    glide((stop[0] + 0.08, stop[1] + 0.1))
    glide(stop)
    time.sleep(MOVE)
    expect_ran(beat.name, omaweb(browser, "film-row", tab))
    time.sleep(1.2)
    expect_ran(beat.name, omaweb(browser, "film-row", tab, "--stop"))
    # The row plain again, the reader's tab.
    time.sleep(1.2)
    end = recorder.now()
    expect_handed_back(beat.name, omaweb(browser, "film-row", tab, "--where"))
    expect_ran(beat.name, omaweb(browser, "film-row"))
    expect_invoice(beat.name, server, INVOICE)
    expect_acts_on_the_row(beat.name, acts)
    mark(beat, start, end, lines=lines, pointer=moves, focus=focus)

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


def record(browser: Path, out: Path, desktop: Desktop | None = None) -> None:
    """Runs under the compositor: the browser, the beats and the raw recording, and the marks.

    Under cage the output is the film's frame. On the `desktop`, which the caller has taken, the
    film's window is put in it and the whole monitor is recorded, and `stage.json` says where the
    window is in the recording, for the edit.
    """
    global CLIENT_ENVIRONMENT, BEFORE_KEYS
    if desktop is None:
        expect_sites_on_loopback("Sites")
    expect_shared_memory("Stage")
    # Where the window is in the recording, for the edit, which takes the whole recording as the
    # window where this is missing, as under cage.
    placed = out / STAGE
    placed.unlink(missing_ok=True)
    root = Path(tempfile.mkdtemp(prefix="omaweb-film-"))
    # Port 80 needs the container's root; on the desktop the browser is told the port instead.
    worn = theme_changes() if desktop is None else theme_changes(desktop.theme, desktop.change)
    server = FixtureServer(("127.0.0.1", 80 if desktop is None else 0), worn[0].colours)
    server.start()
    if desktop is None:
        data, config = seed(root, worn[0].colours)

        def wear(theme: Theme) -> None:
            write_theme(theme.colours, config)
    else:
        # The browser follows the desktop's theme, which the Theme beat switches.
        data, config = seed(root, None, f"http://{KESTREL_ADDRESS}:{server.port}",
                            desktop.sidebar_opacity)
        follow_desktop_theme(root / "home")
        wear = desktop.set_theme
    if desktop is None:
        subprocess.run(["wlr-randr", "--output", "HEADLESS-1", "--custom-mode",
                        "{}x{}".format(*OUTPUT_MODE), "--scale", str(OUTPUT_SCALE)], check=True)
        environment = dict(os.environ, OMAWEB_DATA_ROOT=str(data), OMAWEB_CONFIG_ROOT=str(config),
                           OMAWEB_NO_OMARCHY_TEMPLATE="1", QT_QPA_PLATFORM="wayland")
    else:
        environment = desktop_environment(os.environ, root, server.port)
        CLIENT_ENVIRONMENT = environment
    with open(out / "browser.log", "w") as browser_log:
        process = subprocess.Popen(["dbus-run-session", "--", str(browser)], env=environment,
                                   stdout=subprocess.DEVNULL, stderr=browser_log,
                                   start_new_session=True)
    if desktop is not None:
        desktop.note(browser_session=process.pid, browser=str(browser))
    recorder = Recorder(out / "raw.mkv", desktop.monitor["name"] if desktop else None)
    try:
        wait_for("Stage", "the browser to answer",
                 lambda: omaweb(browser, "spaces").returncode == 0, timeout=60)
        if desktop is not None:
            recorder.geometry = desktop.place(process.pid)
            placed.write_text(json.dumps(
                {"window": recorded_window(desktop.monitor, recorder.geometry)}) + "\n",
                encoding="utf-8")
            BEFORE_KEYS = desktop.expect_keyboard
        time.sleep(LOAD_SETTLE)
        recorder.start()
        if desktop is not None:
            desktop.note(recorder=recorder.process.pid)
        marks = drive(browser, server, recorder, worn, wear)
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
        # A second Ctrl+C would otherwise leave the browser running in its own session.
        with held_signals():
            # Stopped rather than killed, so the raw recording of a run that failed can be watched.
            if recorder.process and recorder.process.poll() is None:
                recorder.process.send_signal(signal.SIGINT)
                try:
                    recorder.process.wait(timeout=30)
                except subprocess.TimeoutExpired:
                    recorder.process.kill()
            try:
                os.killpg(process.pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
            server.stop()
            shutil.rmtree(root, ignore_errors=True)
            CLIENT_ENVIRONMENT = BEFORE_KEYS = None


# The window the desktop recording takes, in the desktop's logical pixels: the browser's layout
# under cage, so every place a beat looks at is where it is there.
WINDOW = (round(OUTPUT_MODE[0] / OUTPUT_SCALE), round(OUTPUT_MODE[1] / OUTPUT_SCALE))


def monitor_area(monitor: dict) -> tuple[int, int, int, int]:
    """Where a monitor from `hyprctl monitors -j` is in the layout, as it stands, in logical pixels.

    Hyprland gives its mode in the display's own pixels, unturned, and its place and what its bar
    keeps (`reserved`, left, top, right, bottom) in logical ones.
    """
    wide, high = monitor["width"], monitor["height"]
    if monitor.get("transform", 0) % 2:
        wide, high = high, wide
    return (monitor["x"], monitor["y"], int(wide / monitor["scale"]),
            int(high / monitor["scale"]))


def window_frame(monitor: dict, size: tuple[int, int] = WINDOW) -> tuple[int, int, int, int]:
    """The film's window in the middle of what the monitor's bar leaves free: (x, y, w, h)."""
    x, y, wide, high = monitor_area(monitor)
    left, top, right, bottom = monitor.get("reserved", [0, 0, 0, 0])
    free = (wide - left - right, high - top - bottom)
    if size[0] > free[0] or size[1] > free[1]:
        raise BeatMissed("Desktop", f"the film's window is {size[0]} by {size[1]}, and "
                                    f"{monitor['name']} has {free[0]} by {free[1]} free")
    return (x + left + (free[0] - size[0]) // 2, y + top + (free[1] - size[1]) // 2, *size)


def recorded_window(monitor: dict,
                    frame: tuple[int, int, int, int]) -> tuple[int, int, int, int]:
    """Where the window at `frame`, in the layout's logical pixels, is in a recording of the
    monitor, in the recording's own pixels: (x, y, w, h)."""
    x, y, wide, high = frame
    scale = monitor["scale"]
    return (round((x - monitor["x"]) * scale), round((y - monitor["y"]) * scale),
            round(wide * scale), round(high * scale))


def parking(monitor: dict, frame: tuple[int, int, int, int]) -> tuple[int, int]:
    """A corner of the monitor off the window, where the pointer hovers nothing the film shows.

    A bottom corner first, since the top ones are on Omarchy's bar, whose buttons a resting
    pointer would open.
    """
    x, y, wide, high = monitor_area(monitor)
    for corner in ((x + wide - 1, y + high - 1), (x, y + high - 1), (x + wide - 1, y), (x, y)):
        inside = (frame[0] <= corner[0] < frame[0] + frame[2]
                  and frame[1] <= corner[1] < frame[1] + frame[3])
        if not inside:
            return corner
    raise BeatMissed("Desktop", f"the film's window covers all of {monitor['name']}")


def desktop_environment(session: dict, root: Path, port: int | str) -> dict:
    """The film's browser on the maintainer's desktop, as cut off from the reader's as under cage.

    It draws on the session's Wayland, so it keeps the runtime directory, and everything else it
    could read or write is under `root`: its home, so no XDG directory of the reader's is found;
    its Agent socket, so neither it nor the script's commands reach the reader's Omaweb; and its
    D-Bus, which `record` starts. The fixture sites resolve to the server on `port`, which needs no
    hosts file, and any other name resolves to nothing, as no name outside the container does.
    The display's scale is Hyprland's, so a Qt scale from the session is dropped.
    """
    home = root / "home"
    environment = {name: value for name, value in session.items()
                   if not name.startswith(("XDG_", "QT_", "QTWEBENGINE_"))}
    environment.update(
        HOME=str(home), XDG_CONFIG_HOME=str(home / ".config"),
        XDG_DATA_HOME=str(home / ".local" / "share"), XDG_STATE_HOME=str(home / ".local" / "state"),
        XDG_CACHE_HOME=str(home / ".cache"),
        OMAWEB_DATA_ROOT=str(root / "data"), OMAWEB_CONFIG_ROOT=str(root / "config"),
        OMAWEB_CONTROL_SOCKET=str(root / "control.sock"), OMAWEB_NO_OMARCHY_TEMPLATE="1",
        QT_QPA_PLATFORM="wayland",
        # A session's Qt logs to the journal when stderr is not a terminal, and `browser.log`
        # is where the film keeps the browser's side.
        QT_FORCE_STDERR_LOGGING="1",
        QTWEBENGINE_CHROMIUM_FLAGS=f'"--host-resolver-rules=MAP *.test 127.0.0.1:{port}, '
                                   'MAP * ~NOTFOUND"')
    if "XDG_RUNTIME_DIR" in session:
        environment["XDG_RUNTIME_DIR"] = session["XDG_RUNTIME_DIR"]
    return environment


# The signals that stop a desktop recording. Each puts the desktop back on its way out.
STOPS = (signal.SIGINT, signal.SIGTERM, signal.SIGHUP)


@contextlib.contextmanager
def held_signals():
    """Nothing that stops a run interrupts what is inside, which is cleaning up after one."""
    handlers = {stop: signal.getsignal(stop) for stop in STOPS}
    for stop in STOPS:
        signal.signal(stop, signal.SIG_IGN)
    try:
        yield
    finally:
        for stop, handler in handlers.items():
            signal.signal(stop, handler)


def stop_recording(signum, _frame) -> None:
    """Stops the run the first time, and lets nothing stop the desktop being put back after it."""
    for stop in STOPS:
        signal.signal(stop, signal.SIG_IGN)
    raise KeyboardInterrupt


class Desktop:
    """The maintainer's Hyprland session, which a desktop recording borrows and gives back.

    What it reads it reads at once; what it changes, it changes only through `act`, which a dry
    run prints instead. Hyprland 0.56 takes its dispatchers as Lua, as Omarchy 4's own scripts send
    them.
    """

    # The workspace a run makes for the film, where it is not given one.
    WORKSPACE = "omaweb-film"

    def __init__(self, run=subprocess.run, which=shutil.which, dry_run: bool = False,
                 ledger: Path | None = None, proc: Path = Path("/proc"), kill=os.kill,
                 killpg=os.killpg, current: Path = DESKTOP_CURRENT,
                 workspace: str = WORKSPACE, session: dict = os.environ,
                 change_to: str = DEFAULT_CHANGE, themes: tuple[Path, ...] = OMARCHY_THEMES) -> None:
        self._run = run
        self._which = which
        self.dry_run = dry_run
        # What the run must put back and what it started, for the next run if this one is killed
        # before it can clean up.
        self.ledger = ledger
        self._proc = proc
        self._kill = kill
        self._killpg = killpg
        # The commands that failed to put the desktop back, for the run's last word.
        self.not_put_back: list[str] = []
        # The film's window, once it is placed, by Hyprland's address for it.
        self.window: str | None = None
        # Where Omarchy keeps the theme the desktop wears, and its palette once the run has read it.
        self._current = current
        self.theme: Theme | None = None
        # The environment the run was started in, whose Omaweb settings it reads, and the sidebar
        # opacity the reader set there once the run has read it.
        self._session = session
        self.sidebar_opacity: float | None = None
        # The theme the Theme beat changes to, by name, where Omarchy's themes are installed, and
        # the theme once the run has read it.
        self._change_to = change_to
        self._themes = themes
        self.change: Theme | None = None
        # The desktop's theme as the run last set it, and `None` until the run first sets one.
        self.theme_on_show: str | None = None
        # The workspace the film runs in, by its name, as `hyprctl workspaces` gives it, and as
        # Hyprland is told it: by number, or by name.
        self.film_workspace = workspace.removeprefix("name:")
        self.film_selector = (self.film_workspace if self.film_workspace.isdigit()
                              else f"name:{self.film_workspace}")

    def query(self, *command: str):
        return json.loads(self.read(*command))

    def read(self, *command: str) -> str:
        if self.dry_run:
            log("reads: " + shlex.join(command))
        try:
            return self._run(list(command), capture_output=True, text=True, check=True).stdout
        except subprocess.CalledProcessError as failed:
            said = (failed.stderr or failed.stdout or "").strip()
            raise BeatMissed("Desktop", f"{shlex.join(command)} exited {failed.returncode}: "
                                        f"{said}") from None
        except OSError as failed:
            raise BeatMissed("Desktop", f"{shlex.join(command)} could not run: "
                                        f"{failed.strerror}") from None

    def act(self, *command: str) -> None:
        """Runs a command that changes the desktop. Hyprland answers a dispatch or an eval it could
        not do with the error on stdout, at times with exit 0, so it has to say "ok"."""
        if self.dry_run:
            would_run(command)
            return
        try:
            done = self._run(list(command), capture_output=True, text=True, check=True)
        except subprocess.CalledProcessError as failed:
            said = (failed.stdout or failed.stderr or "").strip()
            raise BeatMissed("Desktop", f"{shlex.join(command)} exited {failed.returncode}: "
                                        f"{said}") from None
        except OSError as failed:
            raise BeatMissed("Desktop", f"{shlex.join(command)} could not run: "
                                        f"{failed.strerror}") from None
        said = (done.stdout or "").strip()
        if command[0] == "hyprctl" and said != "ok":
            raise BeatMissed("Desktop", f"Hyprland refused {shlex.join(command)}: {said}")

    def note(self, **fields) -> None:
        """Adds to the run's ledger, renamed into place so it is never read half written."""
        if self.ledger is None or self.dry_run:
            return
        noted = json.loads(self.ledger.read_text()) if self.ledger.exists() else {}
        noted.update(fields)
        staged = self.ledger.with_suffix(".new")
        staged.write_text(json.dumps(noted), encoding="utf-8")
        os.replace(staged, self.ledger)

    def command_of(self, pid: int) -> str:
        """The command line of a running process, or nothing for one that has gone."""
        try:
            return (self._proc / str(pid) / "cmdline").read_bytes().replace(b"\0", b" ").decode(
                errors="replace").strip()
        except OSError:
            return ""

    def group(self, session: int) -> list[int]:
        """The processes still running in a process group."""
        members = []
        for entry in self._proc.iterdir():
            if not entry.name.isdigit():
                continue
            try:
                # The command's name is in parentheses and may hold spaces; the group is the third
                # field after it.
                group = int((entry / "stat").read_text().rsplit(")", 1)[1].split()[2])
            except (OSError, ValueError, IndexError):
                continue
            if group == session:
                members.append(int(entry.name))
        return members

    def recover(self) -> None:
        """Cleans up after a run that could not: one killed outright, or whose terminal closed
        before it ended. Its browser and recorder are stopped, if those process numbers are still
        theirs, the desktop's theme and notifications are restored as that run found them, and the
        workspace it started on is brought back if its own is still on show."""
        if self.ledger is None or not self.ledger.exists():
            return
        left = json.loads(self.ledger.read_text())
        log(f"The run before this one did not finish, so what it left is put back first "
            f"({self.ledger}).")
        session, browser = left.get("browser_session"), left.get("browser") or "omaweb-browser"
        if session and any(browser in self.command_of(pid) for pid in self.group(session)):
            self.stop(["kill", "-TERM", "--", f"-{session}"],
                      lambda: self._killpg(session, signal.SIGTERM))
        recorder = left.get("recorder")
        if recorder and Path(self.command_of(recorder).split(" ")[0]).name == "wf-recorder":
            # Interrupted, as Ctrl+C would, so it closes the file it was filling.
            self.stop(["kill", "-INT", str(recorder)], lambda: self._kill(recorder, signal.SIGINT))
        commands = left.get("release", [])
        named = self._current / "theme.name"
        on_show = named.read_text(encoding="utf-8").strip() if named.is_file() else None
        if left.get("theme") and on_show != left["theme"]:
            commands = [("omarchy-theme-set", left["theme"]), *commands]
        if left.get("return_to") and self.active_workspace() == left.get("workspace"):
            commands = [*commands, self.focus_workspace(left["return_to"])]
        for command in commands:
            try:
                self.act(*command)
            except BeatMissed as failed:
                log(f"could not put back {shlex.join(command)}: {failed}")
                self.not_put_back.append(shlex.join(command))
        if not self.dry_run and not self.not_put_back:
            self.ledger.unlink(missing_ok=True)

    def stop(self, command: list[str], stopping) -> None:
        if self.dry_run:
            would_run(command)
            return
        try:
            stopping()
            log("stopped: " + shlex.join(command))
        except ProcessLookupError:
            pass

    @staticmethod
    def dispatch(lua: str) -> tuple[str, ...]:
        return ("hyprctl", "dispatch", lua)

    @classmethod
    def focus_workspace(cls, selector: str) -> tuple[str, ...]:
        return cls.dispatch(f'hl.dsp.focus({{ workspace = "{selector}" }})')

    @staticmethod
    def move_pointer(x: int, y: int) -> tuple[str, ...]:
        return ("hyprctl", "eval", f"hl.dispatch(hl.dsp.cursor.move({{ x = {x}, y = {y} }}))")

    def survey(self) -> None:
        """Reads what the run changes and where its window goes, and stops before changing any."""
        self.monitor = next((monitor for monitor in self.query("hyprctl", "monitors", "-j")
                             if monitor.get("focused")), None)
        if self.monitor is None:
            raise BeatMissed("Desktop", "Hyprland says no monitor has focus")
        self.frame = window_frame(self.monitor)
        self.parked = parking(self.monitor, self.frame)
        # The film must neither cover nor move a window of the reader's.
        held = [space for space in self.query("hyprctl", "workspaces", "-j")
                if space["name"] == self.film_workspace and space["windows"]]
        if held:
            raise BeatMissed("Desktop", f"the workspace {self.film_workspace} already holds "
                                        f"{held[0]['windows']} windows; close them first")
        # Nor may the whole monitor, which the Theme beat shows, hold one: a pinned window is on
        # every workspace.
        pinned = [client for client in self.query("hyprctl", "clients", "-j")
                  if client.get("pinned") and client.get("monitor") == self.monitor["id"]]
        if pinned:
            raise BeatMissed("Desktop", f"{pinned[0].get('class') or 'a window'} "
                                        f"({pinned[0].get('title', '')}) is pinned on "
                                        f"{self.monitor['name']}, so it would be in the film; "
                                        "unpin it first")
        self.workspace = self.active_workspace()
        cursor = self.query("hyprctl", "cursorpos", "-j")
        self.pointer = (cursor["x"], cursor["y"])
        if not self._which("omarchy-theme-set"):
            raise BeatMissed("Theme", "omarchy-theme-set is not here to switch the desktop's "
                                      "theme for the Theme beat")
        self.theme = desktop_theme(self._current)
        self.change = change_theme(self.theme.colours, self._change_to, self._themes)
        self.sidebar_opacity = reader_sidebar_opacity(self._session)
        log("the film's sidebar is " + (f"at the reader's opacity, {self.sidebar_opacity}"
                                        if self.sidebar_opacity is not None
                                        else "at the theme's opacity, as the reader set none"))
        # The terminal the run was started from, which has the keyboard until the film's window.
        self.started_from = self.query("hyprctl", "activewindow", "-j").get("address")
        if self._which("omarchy-shell"):
            # Omarchy 4's shell is its notification daemon.
            state = self.read("omarchy-shell", "notifications", "dndState").strip()
            if state not in ("on", "off"):
                raise BeatMissed("Desktop", f"omarchy-shell said do-not-disturb is {state!r}")
            refresh = ("omarchy-shell", "-q", "omarchy.indicators", "refresh")
            self.hold = [("omarchy-shell", "notifications", "setDnd", "on"), refresh]
            self.release = [("omarchy-shell", "notifications", "setDnd", state), refresh]
        elif self._which("makoctl"):
            modes = self.read("makoctl", "mode").split()
            self.hold = [("makoctl", "mode", "-a", "do-not-disturb")]
            self.release = [("makoctl", "mode", "-s", *modes)]
        else:
            raise BeatMissed("Desktop", "neither omarchy-shell nor makoctl is here to hold "
                                        "notifications for the run")

    def active_workspace(self) -> str:
        """The workspace on show, as Hyprland is told it."""
        active = self.query("hyprctl", "activeworkspace", "-j")
        return str(active["id"]) if active["id"] > 0 else f"name:{active['name']}"

    def set_theme(self, theme: Theme) -> None:
        """Switches the desktop's Omarchy theme, as a reader does. The theme the run started with
        is in the ledger before the first switch, so a killed run's recovery puts it back too."""
        if self.theme_on_show is None:
            self.note(theme=self.theme.name)
        self.theme_on_show = theme.name
        self.act("omarchy-theme-set", theme.name)

    def preflight(self) -> None:
        """Two things Hyprland is asked that change nothing, an eval and a dispatch in the form
        the run sends: the pointer to where it is, and the keyboard to the window that has it.
        A Hyprland that refuses either stops the run before the desktop has changed."""
        self.act(*self.move_pointer(*self.pointer))
        if self.started_from:
            self.act(*self.dispatch(f'hl.dsp.focus({{ window = "address:{self.started_from}" }})'))

    def expect_keyboard(self) -> None:
        """The film's window has the keyboard, so a key sent now reaches it and nothing else."""
        active = self.query("hyprctl", "activewindow", "-j")
        if self.window is None or active.get("address") != self.window:
            holder = (f"{active.get('class') or 'a window'} ({active.get('title', '')})"
                      if active.get("address") else "nothing")
            raise BeatMissed("Desktop", f"{holder} has the keyboard, not the film's window, so "
                                        "the run stops before typing anywhere else")

    def place(self, session: int, getpgid=os.getpgid,
              timeout: float = 20.0) -> tuple[int, int, int, int]:
        """Puts the film's window on its workspace in the film's frame, and answers where it is.

        The window is the one whose process is in the browser's `session`. Its class is no help,
        since the reader's own Omaweb has the same one.
        """

        def in_session(client: dict) -> bool:
            try:
                return getpgid(client["pid"]) == session
            except (OSError, KeyError):
                return False

        def films_window() -> dict | None:
            return next((client for client in self.query("hyprctl", "clients", "-j")
                         if in_session(client)), None)

        if self.dry_run:
            window, floating = "address:<film window>", False
        else:
            found = wait_for("Desktop", "the film's window", films_window, timeout)
            window, floating = f"address:{found['address']}", found.get("floating")
            self.window = found["address"]
        x, y, wide, high = self.frame
        self.act(*self.dispatch(f'hl.dsp.window.move({{ window = "{window}", '
                                f'workspace = "{self.film_selector}", follow = false }})'))
        if not floating:
            self.act(*self.dispatch(f'hl.dsp.window.float({{ window = "{window}", '
                                    'action = "toggle" })'))
        self.act(*self.dispatch(f'hl.dsp.window.resize({{ window = "{window}", '
                                f'x = {wide}, y = {high} }})'))
        self.act(*self.dispatch(f'hl.dsp.window.move({{ window = "{window}", x = {x}, y = {y} }})'))
        self.act(*self.dispatch(f'hl.dsp.focus({{ window = "{window}" }})'))
        if self.dry_run:
            return self.frame

        # Hyprland eases a window to its place, so where it is is read once it has its size and
        # has stopped moving: the same place twice running.
        last: list = [None]

        def arrived() -> dict | None:
            client = films_window()
            if not client or client["size"] != [wide, high]:
                return None
            settled = client["at"] == last[0]
            last[0] = client["at"]
            return client if settled else None

        try:
            client = wait_for("Desktop", "the film's window to take its size", arrived,
                              min(timeout, 5.0))
        except BeatMissed:
            client = films_window() or {"size": ["no", "window"]}
            raise BeatMissed("Desktop", "Hyprland made the film's window {} by {}, not {} by {}"
                             .format(*client["size"], wide, high)) from None
        return (*client["at"], *client["size"])

    @contextlib.contextmanager
    def taken(self):
        """Notifications held, the pointer off the window and a workspace for the film, then each
        put back, whether the run ends, fails, or is stopped with Ctrl+C or a signal.

        What puts a change back is noted before the change is made, so a run stopped halfway
        through taking the desktop puts back what it had taken.
        """
        handlers = {stop: signal.getsignal(stop) for stop in STOPS}
        for stop in STOPS:
            signal.signal(stop, stop_recording)
        undo: list[list[tuple[str, ...]]] = []
        try:
            self.note(release=[list(command) for command in self.release],
                      workspace=self.film_selector, return_to=self.workspace)
            undo.append(self.release)
            for command in self.hold:
                self.act(*command)
            undo.append([self.move_pointer(*self.pointer)])
            self.act(*self.move_pointer(*self.parked))
            undo.append([self.focus_workspace(self.workspace)])
            self.act(*self.focus_workspace(self.film_selector))
            yield
        finally:
            for stop in STOPS:
                signal.signal(stop, signal.SIG_IGN)
            # The desktop's theme first, while notifications are still held.
            if self.theme_on_show not in (None, self.theme.name):
                undo.append([("omarchy-theme-set", self.theme.name)])
            for commands in reversed(undo):
                for command in commands:
                    try:
                        self.act(*command)
                    except BeatMissed as failed:
                        log(f"could not put back {shlex.join(command)}: {failed}")
                        self.not_put_back.append(shlex.join(command))
            if self.ledger is not None and not self.dry_run and not self.not_put_back:
                self.ledger.unlink(missing_ok=True)
            for stop, handler in handlers.items():
                signal.signal(stop, handler)


# Where the desktop's film build is, apart from the reader's own builds.
DESKTOP_BUILD = ROOT / "build" / "film-desktop"
ENGINE = Path("/usr/lib/omaweb")
# What the desktop needs beyond a development machine's tools: the recorder, the keyboard and the
# frame grabber cage has, and the fonts the container installs for the chrome and the sites.
DESKTOP_PACKAGES = ("wf-recorder", "wtype", "grim", "ffmpeg", "ttf-jetbrains-mono", "inter-font",
                    "ttf-ibm-plex", "noto-fonts", "gnu-free-fonts")


def expect_desktop_packages(run=subprocess.run) -> None:
    """Everything the desktop recording runs or draws with is installed. `pacman -T` names what
    is not."""
    missing = run(["pacman", "-T", *DESKTOP_PACKAGES], capture_output=True, text=True,
                  check=False).stdout.split()
    if missing:
        raise BeatMissed("Desktop", "the film needs " + ", ".join(missing) + "; install them with "
                         "sudo pacman -S --needed " + " ".join(missing))


def build_for_desktop(run=subprocess.run, engine: Path = ENGINE / "lib" / "cmake",
                      dry_run: bool = False) -> Path:
    """Builds the film's browser from the commit checked out, as the container does, and answers
    where it is.

    The film is of a commit, so a checkout with changes is refused. A machine with a stock
    QtWebEngine beside Omaweb's finds the stock one first, so each engine package is named, and
    `check_omaweb_engine.sh` says the build found Omaweb's. The build takes two jobs, which is
    what the maintainer's machine has memory for.
    """
    changes = run(["git", "status", "--porcelain"], cwd=ROOT, capture_output=True, text=True,
                  check=True).stdout
    if changes.strip():
        refusal = BeatMissed("Build", "the checkout has changes, and the film is of a commit: "
                                      "commit or put them away first")
        if not dry_run:
            raise refusal
        log(f"a real run would stop here: {refusal}")
    environment = dict(os.environ)
    environment["CMAKE_BUILD_PARALLEL_LEVEL"] = "2"
    # bootstrap_content_blocker.sh builds with cargo, which takes every core it is not told about.
    environment["CARGO_BUILD_JOBS"] = "2"
    commands = [
        ["scripts/bootstrap_content_blocker.sh"],
        ["cmake", "--preset", "ci", "-B", str(DESKTOP_BUILD), "-DOMAWEB_FILM_HOOKS=ON",
         f"-DQT_ADDITIONAL_PACKAGES_PREFIX_PATH={ENGINE}",
         f"-DCMAKE_CXX_FLAGS=-I{ENGINE}/include/qt6",
         *(f"-D{package.name}_DIR:PATH={package}"
           for package in sorted(engine.glob("Qt6WebEngine*")))],
        ["scripts/check_omaweb_engine.sh", str(DESKTOP_BUILD)],
        # The film's verbs are tested here, in the one build that has them, as in the container.
        ["cmake", "--build", str(DESKTOP_BUILD), "--target", "omaweb-browser",
         "omaweb-translations", "omaweb-agent-command-tests", "omaweb-agent-control-tests"],
        ["ctest", "--test-dir", str(DESKTOP_BUILD), "-R", "^omaweb-agent-(command|control)$"],
    ]
    for command in commands:
        if dry_run:
            would_run(command)
        else:
            run(command, cwd=ROOT, env=environment, check=True)
    return DESKTOP_BUILD / "omaweb-browser"


TAKES_THE_KEYBOARD = """\
The film is recorded on this desktop now, in about a minute and a half.

It opens a film build of Omaweb, with a profile of its own, on the empty workspace
{workspace}, and records only its window. Your own Omaweb is not started, and of its profile
only the sidebar opacity is read.
For the run it takes the keyboard and types into that window, holds notifications, and moves
the pointer to a corner of the screen. Leave the keyboard and the pointer alone until it ends.
Ctrl+C stops it at any time, and the workspace, the pointer and notifications are put back.

Press Enter to begin, or Ctrl+C to leave everything as it is."""


def record_on_desktop(desktop: Desktop, out: Path) -> None:
    """The film recorded on the maintainer's own Hyprland session, and cut as the container's is.

    Nothing on the desktop changes before the reader has been told and pressed a key. A dry run
    builds nothing and changes nothing, and prints each command it would have run instead.
    """
    try:
        expect_desktop_packages()
    except BeatMissed as missing:
        if not desktop.dry_run:
            raise
        log(f"a real run would stop here: {missing}")
    browser = build_for_desktop(dry_run=desktop.dry_run)
    if desktop.dry_run:
        desktop.recover()
        desktop.survey()
        desktop.preflight()
        with desktop.taken():
            root = Path(tempfile.gettempdir()) / "omaweb-film-<run>"
            environment = desktop_environment(os.environ, root, "<port>")
            settings = {name: environment[name] for name in
                        ("HOME", "OMAWEB_DATA_ROOT", "OMAWEB_CONFIG_ROOT", "OMAWEB_CONTROL_SOCKET",
                         "QTWEBENGINE_CHROMIUM_FLAGS")}
            log("would start: " + " ".join(f"{name}={shlex.quote(value)}"
                                           for name, value in settings.items())
                + " " + shlex.join(["dbus-run-session", "--", str(browser)]))
            frame = desktop.place(0)
            would_run(recorder_command(out / "raw.mkv", desktop.monitor["name"]))
            log(f"and types into the window with wtype, each key after hyprctl activewindow -j "
                f"says the window has the keyboard, and samples it with "
                f"{shlex.join(frame_command(frame))}")
            log(f"the edit crops the recording to the window, at "
                f"{recorded_window(desktop.monitor, frame)} in it, and the Theme beat draws back "
                f"to the whole monitor")
            log(f"its browser follows the desktop's theme through {DESKTOP_CURRENT}, and the Theme "
                f"beat switches the desktop's theme there and back:")
            desktop.set_theme(desktop.change)
            desktop.set_theme(desktop.theme)
        cursor = desktop_cursor(os.environ, Path.home())
        log(f"and the edit draws the desktop's pointer, {cursor.resolve()}" if cursor
            else f"and the edit draws {POINTER}, as no desktop cursor was found")
        return
    print(TAKES_THE_KEYBOARD.format(workspace=desktop.film_workspace), flush=True)
    input()
    # After the key: what a run killed before this one left is put back first, and the desktop is
    # read after that, so what this run puts back is the desktop as it found it.
    desktop.recover()
    desktop.survey()
    desktop.preflight()
    with desktop.taken():
        record(browser, out, desktop)
    compose(out, pointer_for_desktop(os.environ, Path.home(), out))
    log("The film is in " + ", ".join(str(out / name) for name in (*BUDGET, "captions.vtt"))
        + ", and its clips in " + ", ".join(f"{out}/clip-{card}.*" for card in CLIP_OF.values())
        + f" with {out / 'clips.json'}")


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


def camera(focus: list[tuple[float, Place]],
           window: tuple[float, float, float] = (0.0, 0.0, 1.0)) -> str:
    """The zoompan filter that follows a beat's focus, each place framed at the film's aspect.

    The window and the film are both 16:9, so a place keeps its shape when it is as wide, as a
    fraction of the window, as it is high: the shorter side is grown about the place's centre,
    and kept in the window. The window is where `Stage.window` says it is on the stage.
    """
    keyframes = []
    for at, place in focus:
        if place == DESKTOP:
            keyframes.append((at, 1.0, 0.0, 0.0))
            continue
        left, top, wide, high = place or (0.0, 0.0, 1.0, 1.0)
        side = max(wide, high)
        left = min(max(left + (wide - side) / 2, 0.0), 1.0 - side)
        top = min(max(top + (high - side) / 2, 0.0), 1.0 - side)
        x, y, scale = window
        keyframes.append((at, side * scale, x + left * scale, y + top * scale))
    side = eased([(at, value) for at, value, _, _ in keyframes], "it")
    x = eased([(at, value) for at, _, value, _ in keyframes], "it")
    y = eased([(at, value) for at, _, _, value in keyframes], "it")
    return (f"zoompan=z='1/({side})':x='({x})*iw':y='({y})*ih':d=1:fps={FPS}"
            f":s={FILM_SIZE[0]}x{FILM_SIZE[1]}")


# The pointer drawn over the Space switch, whose tip is its top left corner. The recording has no
# pointer of its own, so the window holds a Space's name on show and this is drawn where the
# window says the Space's square is. It is sized for the recording's pixels, before the camera.
POINTER = ROOT / "film" / "pointer.png"


class Pointer(typing.NamedTuple):
    """The image the edit draws as the pointer, and its hotspot: the pixel that points."""

    path: Path
    hot: tuple[int, int]


# The committed drawing, whose tip is its top left corner.
DRAWN_POINTER = Pointer(POINTER, (0, 0))


class XcursorImage(typing.NamedTuple):
    """One image of an Xcursor file: its size, its hotspot and its pixels as straight RGBA."""

    width: int
    height: int
    hot: tuple[int, int]
    rgba: bytes


# The cursor's size on the desktop when the session names none, as Hyprland's XCURSOR_SIZE does.
CURSOR_SIZE = 24
XCURSOR_IMAGE = 0xFFFD0002


def read_xcursor(path: Path, size: int) -> XcursorImage:
    """The image of the Xcursor file at `path` whose nominal size is nearest `size`, its pixels as
    straight RGBA. Xcursor keeps them premultiplied ARGB, little-endian."""
    data = path.read_bytes()
    magic, header, _, count = struct.unpack_from("<4sIII", data)
    if magic != b"Xcur":
        raise ValueError(f"{path} is not an Xcursor file")
    images = [struct.unpack_from("<III", data, header + 12 * index) for index in range(count)]
    images = [(nominal, position) for kind, nominal, position in images if kind == XCURSOR_IMAGE]
    if not images:
        raise ValueError(f"{path} holds no image")
    _, position = min(images, key=lambda image: abs(image[0] - size))
    width, height, x_hot, y_hot = struct.unpack_from("<4I", data, position + 16)
    pixels = struct.unpack_from(f"<{width * height}I", data, position + 36)
    rgba = bytearray()
    for argb in pixels:
        alpha = argb >> 24
        rgba += bytes(min(255, ((argb >> shift) & 0xFF) * 255 // alpha) if alpha else 0
                      for shift in (16, 8, 0))
        rgba.append(alpha)
    return XcursorImage(width, height, (x_hot, y_hot), bytes(rgba))


def write_png(image: XcursorImage, path: Path) -> None:
    """`image` as an RGBA PNG at `path`, for ffmpeg to draw."""

    def chunk(kind: bytes, body: bytes) -> bytes:
        return (struct.pack(">I", len(body)) + kind + body
                + struct.pack(">I", zlib.crc32(kind + body)))

    stride = image.width * 4
    rows = b"".join(b"\x00" + image.rgba[row * stride:(row + 1) * stride]
                    for row in range(image.height))
    path.write_bytes(b"\x89PNG\r\n\x1a\n"
                     + chunk(b"IHDR", struct.pack(">IIBBBBB", image.width, image.height,
                                                  8, 6, 0, 0, 0))
                     + chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b""))


def desktop_cursor(environ: typing.Mapping[str, str], home: Path) -> Path | None:
    """The desktop's pointer image, as libXcursor finds it: the theme the session names, or the
    one the `default` theme inherits, in each folder of the Xcursor path in turn."""
    folders = ([Path(folder) for folder in environ["XCURSOR_PATH"].split(":") if folder]
               if environ.get("XCURSOR_PATH") else
               [home / ".local" / "share" / "icons", home / ".icons", Path("/usr/share/icons"),
                Path("/usr/share/pixmaps")])
    seen: set[str] = set()

    def find(theme: str) -> Path | None:
        if theme in seen:
            return None
        seen.add(theme)
        inherited: list[str] = []
        for folder in folders:
            for name in ("left_ptr", "default"):
                if (folder / theme / "cursors" / name).is_file():
                    return folder / theme / "cursors" / name
            index = folder / theme / "index.theme"
            if index.is_file():
                inherits = re.search(r"^Inherits\s*=\s*(.+)$",
                                     index.read_text(encoding="utf-8", errors="replace"),
                                     re.MULTILINE)
                if inherits:
                    inherited += [name.strip() for name in inherits.group(1).split(",")]
        for name in inherited:
            found = find(name)
            if found:
                return found
        return None

    return find(environ.get("XCURSOR_THEME") or "default")


def pointer_for_desktop(environ: typing.Mapping[str, str], home: Path, out: Path) -> Pointer:
    """The desktop's own pointer, written into `out` at the film's size, or the committed drawing
    when there is none to read. The film's frame is the window at `OUTPUT_SCALE`, so the cursor is
    read at its size on the desktop times that."""
    path = desktop_cursor(environ, home)
    try:
        size = round(int(environ.get("XCURSOR_SIZE") or CURSOR_SIZE) * OUTPUT_SCALE)
        image = read_xcursor(path, size) if path else None
    except (OSError, ValueError, struct.error):
        image = None
    if image is None:
        log(f"the pointer is {POINTER}, as no desktop cursor could be read")
        return DRAWN_POINTER
    drawn = out / "pointer.png"
    write_png(image, drawn)
    log(f"the pointer is the desktop's, {path.resolve()}, at {image.width} pixels")
    return Pointer(drawn, image.hot)


def pointer_for_recording(out: Path, environ: typing.Mapping[str, str], home: Path) -> Pointer:
    """The pointer a recording in `out` is cut with: the desktop's own for a desktop recording,
    which leaves its stage beside it, so cutting it again draws what its first cut drew, and the
    committed drawing for one made in the container."""
    if (out / STAGE).exists():
        return pointer_for_desktop(environ, home, out)
    return DRAWN_POINTER


def pointer(entry: dict, hot: tuple[int, int] = (0, 0),
            origin: tuple[int, int] = (0, 0)) -> str | None:
    """The overlay filter that draws the pointer over a beat, or `None` for a beat without one.

    `entry["pointer"]` lists where it moves, each a time into the beat and a fraction of the
    window, whose top left corner is at `origin` on the stage. It comes in at the first and glides
    to each next one, and stays to the beat's end, its `hot` pixel on each place.
    """
    moves = entry.get("pointer")
    if not moves:
        return None
    x = eased([(move["at"], origin[0] + move["x"] * OUTPUT_MODE[0] - hot[0]) for move in moves],
              "t")
    y = eased([(move["at"], origin[1] + move["y"] * OUTPUT_MODE[1] - hot[1]) for move in moves],
              "t")
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


# Where `record` tells the edit the desktop film's window is in the recording.
STAGE = "stage.json"


class Stage(typing.NamedTuple):
    """What the camera films: the recording scaled so the film's window is cage's `OUTPUT_MODE`,
    and padded to the film's shape. Under cage it is the window alone; on the desktop it is the
    monitor, with the window where it was placed."""

    # The filters that make the recording into the stage, to follow a trim.
    scaling: str
    # Its width and height, in pixels.
    size: tuple[int, int]
    # The window's top left corner on the stage, in its pixels.
    origin: tuple[int, int]

    @property
    def window(self) -> tuple[float, float, float]:
        """The window on the stage as fractions of it: left, top, and its side, the same fraction
        of the stage's width as of its height, since both are the film's shape."""
        return (self.origin[0] / self.size[0], self.origin[1] / self.size[1],
                OUTPUT_MODE[0] / self.size[0])


def staging(size: tuple[int, int], window: tuple[int, int, int, int] | None = None) -> Stage:
    """The stage for a recording of `size` whose film's window is at `window`, in its pixels, or
    the whole of it. A monitor at another scale than cage's is scaled, so the pointer and the
    camera find what they look for where they look, and one of another shape than the film's is
    padded with black, so the whole of it shows when the camera draws back."""
    x, y, wide, _ = window or (0, 0, *size)
    zoom = OUTPUT_MODE[0] / wide
    scaled = tuple(round(side * zoom / 2) * 2 for side in size)
    # The film's 16:9 in an even number of pixels each way, as yuv420p needs.
    units = max(math.ceil(scaled[0] / 16), math.ceil(scaled[1] / 9))
    units += units % 2
    padded = (16 * units, 9 * units)
    border = ((padded[0] - scaled[0]) // 4 * 2, (padded[1] - scaled[1]) // 4 * 2)
    scaling = ""
    if scaled != tuple(size):
        scaling += ",scale={}:{}:flags=lanczos".format(*scaled)
    if padded != scaled:
        scaling += ",pad={}:{}:{}:{}".format(*padded, *border)
    return Stage(scaling, padded, (round(x * zoom) + border[0], round(y * zoom) + border[1]))


def recording_size(path: Path) -> tuple[int, int]:
    answer = subprocess.run(["ffprobe", "-v", "error", "-select_streams", "v:0", "-show_entries",
                             "stream=width,height", "-of", "csv=p=0", str(path)],
                            capture_output=True, text=True, check=True).stdout
    width, height = answer.strip().split(",")[:2]
    return int(width), int(height)


def framed(entry: dict, stage: Stage, drawn: Pointer, pointer_input: str | None,
           label: str) -> list[str]:
    """One beat of the raw recording as filter chains that end in `[label]`: trimmed to the beat,
    its pointer drawn as `drawn` from the input labelled `pointer_input`, and its camera followed.
    """
    beat = BEAT_NAMED[entry["beat"]]
    chains = []
    trimmed = (f"[0:v]trim=start={entry['start']:.3f}:end={entry['end']:.3f},"
               f"setpts=PTS-STARTPTS{stage.scaling}")
    overlay = pointer(entry, drawn.hot, stage.origin)
    if overlay:
        chains.append(f"{trimmed}[r{label}]")
        trimmed = f"[r{label}][{pointer_input}]{overlay}"
    # A beat whose moments are timed as it plays brings its focus with it.
    focus = [(at, tuple(place) if isinstance(place, list) else place)
             for at, place in entry.get("focus", beat.focus)]
    chains.append(f"{trimmed},{camera(focus, stage.window)},setsar=1,format=yuv420p[{label}]")
    return chains


# A frame whose chroma, averaged over what is watched, moves this far from the one before it is a
# theme showing: a sweep of the desktop or a repaint of the window. The beat is quiet otherwise.
# Frames of one change are less than a half second apart, and the next change is further off.
ONSET_SHIFT = 0.1
ONSET_APART = 0.5


def chroma_shifts(raw: Path, start: float, end: float,
                  above: float = 1.0) -> list[tuple[float, float]]:
    """How far each frame of `raw` between `start` and `end` seconds moves the mean chroma of its
    top `above` (a fraction of its height) from the frame before, as (seconds into that span, shift)
    pairs."""
    answer = subprocess.run(
        ["ffmpeg", "-v", "error", "-ss", f"{start:.3f}", "-t", f"{end - start:.3f}", "-i", str(raw),
         "-vf", f"crop=iw:ih*{above}:0:0,scale=320:-2,signalstats,metadata=print:file=-",
         "-f", "null", "-"],
        capture_output=True, text=True, check=True).stdout
    shifts, at, mean, last = [], 0.0, None, None
    for line in answer.splitlines():
        if "pts_time:" in line:
            at = float(line.split("pts_time:")[1])
        elif "lavfi.signalstats.UAVG=" in line:
            mean = float(line.split("=")[1])
        elif "lavfi.signalstats.VAVG=" in line:
            now = (mean, float(line.split("=")[1]))
            if last is not None:
                shifts.append((at, abs(now[0] - last[0]) + abs(now[1] - last[1])))
            last = now
    return shifts


def theme_onsets(themes: list, shifts: list[tuple[float, float]], length: float) -> list:
    """`themes` with each change after the first moved to when the recording first shows it.

    The recorder marks a change when it saw it, which on the desktop is some 2.7 s after the
    recording shows it, so each change takes the start of its own run of frames in `shifts`, in
    order. On the desktop the window repaints at once and the desktop's sweep follows, and the
    page's reveal is the sweep's, so `shifts` come from the wallpaper above the window. A beat whose recording shows another number of changes keeps the times it was marked
    with. The first theme is the beat's own, from its start.
    """
    runs: list[float] = []
    previous = None
    for at, shift in shifts:
        if at > length or shift < ONSET_SHIFT:
            continue
        if previous is None or at - previous >= ONSET_APART:
            runs.append(at)
        previous = at
    if len(runs) != len(themes) - 1:
        return themes
    return [themes[0]] + [[round(at, 3), name] for at, (_, name) in zip(runs, themes[1:])]


def cut_clips(out: Path, marks: list[dict], stage: Stage, drawn: Pointer) -> None:
    """Each beat a card plays as a clip of its own, `clip-<card>`, in the page's two codecs with a
    poster of its first frame, and `clips.json`: each clip's length, and for the Theme beat the
    moment each theme first shows, so the page can wear it with the clip."""
    clips = {}
    for entry in marks:
        card = CLIP_OF.get(entry["beat"])
        if card is None:
            continue
        if "themes" in entry:
            # Under cage the recording is the window, and its repaint is the change; on the desktop
            # the wallpaper above the window shows the desktop's sweep.
            above = 0.75 * stage.window[1] or 1.0
            entry["themes"] = theme_onsets(entry["themes"], chroma_shifts(
                out / "raw.mkv", entry["start"], entry["end"], above),
                entry["end"] - entry["start"])
        drawn_input = ["-i", str(drawn.path)] if pointer(entry) else []
        chains = framed(entry, stage, drawn, "1:v" if drawn_input else None, "beat")
        chains.append(f"[beat]scale={CLIP_WIDTH}:-2,{COLOURS}[clip]")
        master = out / f"clip-{card}.mkv"
        subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", str(out / "raw.mkv"), *drawn_input,
                        "-filter_complex", ";".join(chains), "-map", "[clip]", "-an",
                        "-c:v", "libx264", "-preset", "veryfast", "-crf", "8", str(master)],
                       check=True)
        for name, arguments in (
                (f"clip-{card}.webm", ["-c:v", "libvpx-vp9", "-b:v", "0", "-crf", "38",
                                       "-row-mt", "1", "-deadline", "good", "-cpu-used", "2"]),
                (f"clip-{card}.mp4", ["-c:v", "libx264", "-preset", "slower",
                                      "-profile:v", "high", "-crf", "28",
                                      "-movflags", "+faststart"])):
            subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", str(master), "-an",
                            "-pix_fmt", "yuv420p", *arguments, str(out / name)], check=True)
        subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", str(master), "-frames:v", "1",
                        "-c:v", "libwebp", "-quality", "80", str(out / f"clip-{card}.webp")],
                       check=True)
        master.unlink()
        clips[card] = {"length": round(entry["end"] - entry["start"], 3)}
        if "themes" in entry:
            clips[card]["themes"] = entry["themes"]
    (out / "clips.json").write_text(json.dumps(clips, indent=2) + "\n", encoding="utf-8")
    expect_within("Clips", sorted(out.glob("clip-*")), CLIPS_BUDGET)


def compose(out: Path, drawn: Pointer = DRAWN_POINTER) -> None:
    """Cuts the raw recording to its beats, follows each beat's focus, and captions it, and draws
    the pointer as `drawn`; then cuts each beat a card plays into a clip of its own."""
    marks = json.loads((out / "marks.json").read_text(encoding="utf-8"))
    placed = out / STAGE
    window = (json.loads(placed.read_text(encoding="utf-8"))["window"] if placed.exists()
              else None)
    stage = staging(recording_size(out / "raw.mkv"), window)
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
        chains += framed(entry, stage, drawn,
                         f"p{pointed.index(entry)}" if pointer(entry) else None, f"b{index}")
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
    pointer_input = ["-i", str(drawn.path)] if pointed else []
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
    cut_clips(out, marks, stage, drawn)


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
    parser.add_argument("step", choices=("record", "compose", "desktop"))
    parser.add_argument("--browser", type=Path, default=ROOT / "build" / "ci" / "omaweb-browser")
    parser.add_argument("--out", type=Path, default=ROOT / "build" / "film")
    parser.add_argument("--dry-run", action="store_true",
                        help="desktop: print what the run would do, and do none of it")
    parser.add_argument("--change-to", metavar="THEME", default=DEFAULT_CHANGE,
                        help="desktop: the installed Omarchy theme the Theme beat changes to and "
                             f"back from (default: {DEFAULT_CHANGE})")
    parser.add_argument("--workspace", default=Desktop.WORKSPACE,
                        help="desktop: the empty workspace to record in, by number or name "
                             f"(default: a workspace of its own, {Desktop.WORKSPACE})")
    arguments = parser.parse_args()
    arguments.out.mkdir(parents=True, exist_ok=True)
    desktop = (Desktop(dry_run=arguments.dry_run,
                       ledger=arguments.out.resolve() / "desktop-run.json",
                       workspace=arguments.workspace, change_to=arguments.change_to)
               if arguments.step == "desktop" else None)
    try:
        if arguments.step == "record":
            record(arguments.browser.resolve(), arguments.out)
        elif desktop is not None:
            record_on_desktop(desktop, arguments.out.resolve())
        else:
            compose(arguments.out, pointer_for_recording(arguments.out, os.environ, Path.home()))
    except (BeatMissed, subprocess.CalledProcessError, StopIteration) as missed:
        if isinstance(missed, subprocess.CalledProcessError):
            missed = f"{shlex.join(map(str, missed.cmd))} exited {missed.returncode}"
        print(f"the film was not made: {missed or 'an answer it needed never came'}"
              f"{put_back(desktop, '')}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print(f"the film was stopped{put_back(desktop, ', and the desktop is as it was')}",
              file=sys.stderr)
        return 130
    return 0


def put_back(desktop: Desktop | None, whole: str) -> str:
    """The end of the run's last word: what to put back by hand, or `whole` when nothing is."""
    if desktop is None:
        return ""
    if desktop.not_put_back:
        return "; put back by hand: " + "; ".join(desktop.not_put_back)
    return whole


if __name__ == "__main__":
    sys.exit(main())
