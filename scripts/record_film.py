#!/usr/bin/env python3
"""Records the website's introductory film from the real browser.

The website opens on a film of Omaweb in use: the Omnibar, a Space switch, the sidebar hidden,
blocking, a theme change and an Agent at work. It has to show the real browser on its real engine
and stay true to each release, so it is recorded by this script rather than by hand.
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

# Paths the server answers itself on every site rather than from a file: what a page reports, the
# search engine's two endpoints, the invoice form's target and the script that does the reporting.
SERVED_PATHS = {"/beacon", "/search", "/suggest", "/invoices", "/film/report.js"}


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


def expect_within(beat: str, paths: list[Path], limit: int) -> None:
    total = sum(path.stat().st_size for path in paths)
    if total > limit:
        raise BeatMissed(beat, f"the film is {total} bytes, over its {limit}")


def suggestions(query: str) -> list:
    """An OpenSearch suggestions answer: the terms, then what the engine proposes for them."""
    with open(site_directory("kestrel.test") / "suggestions.json", encoding="utf-8") as handle:
        proposals = json.load(handle)
    return [query, proposals.get(query.strip().lower(), [])]


# The film's beats, in the order they play: what the caption says and, for each moment the camera
# moves, where it looks. A place is a fraction of the window, (left, top, width, height), so it
# holds at any output size; `None` is the whole window. Times are seconds into the beat.
@dataclasses.dataclass
class Beat:
    name: str
    caption: str
    focus: list[tuple[float, tuple[float, float, float, float] | None]]


# Where the parts the camera follows sit in the window, with the sidebar at its default width: the
# Omnibar a little below the middle of the page, the Space dots at the foot of the sidebar, the
# address and its blocked count at its head, and the Agent's form under the Space's notice.
OMNIBAR = (0.29, 0.28, 0.6, 0.6)
FOOTER = (0.0, 0.62, 0.38, 0.38)
ADDRESS = (0.0, 0.0, 0.3, 0.3)
AGENT_TAB = (0.0, 0.0, 0.42, 0.42)
FORM = (0.32, 0.03, 0.66, 0.66)

BEATS = [
    Beat("Omnibar", "One Omnibar for tabs, Spaces and search",
         [(0.0, None), (1.0, OMNIBAR), (5.2, None)]),
    Beat("Space switch", "Each Space keeps its own logins and tabs",
         [(0.0, FOOTER), (2.2, None)]),
    Beat("Sidebar", "Hide the sidebar and the page takes the window", [(0.0, None)]),
    Beat("Blocking", "Ads and trackers stopped before they load",
         [(0.0, None), (2.4, ADDRESS)]),
    Beat("Theme", "Your Omarchy theme reaches the whole browser", [(0.0, None)]),
    Beat("Agent", "An Agent works in a Space of its own",
         [(0.0, AGENT_TAB), (3.0, FORM), (11.0, None)]),
]

# The frame the page shows before the film plays: the Omnibar with its rows open.
POSTER = ("Omnibar", 3.6)

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

# What the film may weigh, per file, which together stay under the 5 MB the page can afford.
BUDGET = {"omaweb.webm": 2_000_000, "omaweb.mp4": 2_600_000, "poster.webp": 250_000}

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
    (config / "privacy.json").write_text(
        json.dumps({"allow-agents": True, "engine-suggestions": True}), encoding="utf-8")
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
    # answer is cut out of the film rather than held on screen.
    def mark(beat: Beat, start: float, end: float) -> None:
        marks.append({"beat": beat.name, "start": start, "end": end})
        log(f"{beat.name}: {start:.1f}s to {end:.1f}s")

    beats = {beat.name: beat for beat in BEATS}

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
    time.sleep(0.8)
    keys("Primary+l", settle=0.6)
    type_text(TYPED)
    time.sleep(2.2)
    keys(*["Down"] * SUGGESTION_ROW, settle=0.5)
    # The results, a moment on screen before the cut.
    keys("Return", settle=LOAD_SETTLE + 1.4)
    end = recorder.now()
    expect_asked(beat.name, server, "kestrel.test", f"/suggest?q={TYPED}")
    expect_current_tab(beat.name, answer(beat.name, browser, "tabs", "--space", "Personal"),
                       SEARCH)
    mark(beat, start, end)

    beat = beats["Space switch"]
    start = recorder.now()
    time.sleep(0.6)
    keys("Primary+2", settle=3.4)
    end = recorder.now()
    expect_space_on_show(beat.name, answer(beat.name, browser, "spaces"), "Work")
    mark(beat, start, end)

    beat = beats["Sidebar"]
    before = server.reports()
    start = recorder.now()
    time.sleep(0.6)
    keys("Primary+b", settle=3.8)
    end = recorder.now()
    expect_wider(beat.name, before, server.reports(), "quillstack.test")
    mark(beat, start, end)

    # Off camera: the sidebar back, and the magazine on show in Personal.
    keys("Primary+b", "Primary+1")
    expect_ran("Stage", omaweb(browser, "focus", MAGAZINE))
    time.sleep(LOAD_SETTLE)

    beat = beats["Blocking"]
    since = len(server.reports())
    start = recorder.now()
    time.sleep(0.4)
    expect_ran(beat.name, omaweb(browser, "run", "reload"))
    path = urllib.parse.urlsplit(MAGAZINE).path
    wait_for(beat.name, "the magazine to load", reported(server, "halyard.test", path, since))
    time.sleep(5.0)
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
    time.sleep(0.8)
    write_theme("tokyo-night", config)
    time.sleep(4.6)
    end = recorder.now()
    expect_repainted(beat.name, was, pixel(frame(), *sample),
                     theme_colour("retro-82", "dark_background"),
                     theme_colour("tokyo-night", "dark_background"))
    mark(beat, start, end)

    # Off camera: the new tab put away unopened.
    keys("Escape")

    beat = beats["Agent"]
    start = recorder.now()
    created = omaweb(browser, "space", "new", "Invoices", name=AGENT)
    expect_ran(beat.name, created)
    space = created.stdout.strip()
    expect_ran(beat.name, omaweb(browser, "space", space))
    expect_space_on_show(beat.name, answer(beat.name, browser, "spaces"), "Invoices")
    opened = omaweb(browser, "open", INVOICE_PAGE, "--space", space, name=AGENT)
    expect_ran(beat.name, opened)
    tab = opened.stdout.strip()
    # An Agent's tab never takes the reader's focus, so the reader selects it to watch, and rests
    # the keyboard in the sidebar: an Agent waits while the reader's keyboard is in its page.
    expect_ran(beat.name, omaweb(browser, "focus", tab))
    keys("Primary+e")
    wait_for(beat.name, "the invoice form to load",
             reported(server, "tallyhaus.test", urllib.parse.urlsplit(INVOICE_PAGE).path))
    time.sleep(1.0)
    look = answer(beat.name, browser, "look", "--tab", tab, name=AGENT)["look"]
    # Sent with Enter from the last field, as a person would: a click waits while the reader is
    # in Omaweb's own controls, and here the reader is in the sidebar.
    steps = [
        f'select {target(beat.name, look, "Customer")} "{INVOICE["customer"]}"',
        f'fill {target(beat.name, look, "Amount")} {INVOICE["amount"]}',
        f'fill {target(beat.name, look, "Note to customer")} "{INVOICE["note"]}"',
        f'fill {target(beat.name, look, "Due date")} "{INVOICE["due"]}"',
        "press Enter",
    ]
    for step in steps:
        expect_ran(beat.name, omaweb(browser, "do", step, "--tab", tab, name=AGENT))
        time.sleep(1.4)
    time.sleep(2.6)
    end = recorder.now()
    expect_invoice(beat.name, server, INVOICE)
    mark(beat, start, end)
    return marks


def record(browser: Path, out: Path) -> None:
    """Runs under the compositor: the browser, the beats and the raw recording, and the marks."""
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


# How long the camera takes to move from one place to the next.
MOVE = 0.7


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


def timestamp(seconds: float) -> str:
    minutes, seconds = divmod(seconds, 60)
    return f"{int(minutes):02d}:{seconds:06.3f}"


def compose(out: Path) -> None:
    """Cuts the raw recording to its beats, follows each beat's focus, and captions it."""
    marks = json.loads((out / "marks.json").read_text(encoding="utf-8"))
    beats = {beat.name: beat for beat in BEATS}
    font = ROOT / "website" / "assets" / "fonts" / "Tomorrow-SemiBold.woff2"
    chains, cues = [], []
    position = 0.0
    for index, entry in enumerate(marks):
        beat = beats[entry["beat"]]
        length = entry["end"] - entry["start"]
        chains.append(
            f"[0:v]trim=start={entry['start']:.3f}:end={entry['end']:.3f},setpts=PTS-STARTPTS,"
            f"{camera(beat)},setsar=1,format=yuv420p[b{index}]")
        cues.append((position + 0.3, position + length - FADE, beat.caption, beat.name, position))
        position += length - FADE
    joined = "[b0]"
    for index in range(1, len(marks)):
        offset = cues[index][4]
        chains.append(f"{joined}[b{index}]xfade=transition=fade:duration={FADE}"
                      f":offset={offset:.3f}[j{index}]")
        joined = f"[j{index}]"
    captions = []
    for start, end, text, _, _ in cues:
        alpha = (f"if(lt(t,{start + 0.3:.3f}),(t-{start:.3f})/0.3,"
                 f"if(gt(t,{end - 0.3:.3f}),({end:.3f}-t)/0.3,1))")
        captions.append(
            f"drawtext=fontfile='{font}':text='{text}':fontsize=46:fontcolor=white"
            f":box=1:boxcolor=0x05182e@0.82:boxborderw=22|34:x=(w-text_w)/2:y=h-text_h-84"
            f":alpha='{alpha}':enable='between(t,{start:.3f},{end:.3f})'")
    chains.append(f"{joined}{','.join(captions)}[film]")
    master = out / "master.mkv"
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", str(out / "raw.mkv"),
                    "-filter_complex", ";".join(chains), "-map", "[film]", "-an",
                    "-c:v", "libx264", "-preset", "veryfast", "-crf", "8", str(master)],
                   check=True)
    encode(out, master)
    poster_beat, poster_at = POSTER
    poster = next(cue for cue in cues if cue[3] == poster_beat)[4] + poster_at
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-ss", f"{poster:.3f}", "-i", str(master),
                    "-frames:v", "1", "-c:v", "libwebp", "-quality", "80",
                    str(out / "poster.webp")], check=True)
    vtt = ["WEBVTT", ""]
    for start, end, text, _, _ in cues:
        vtt += [f"{timestamp(start)} --> {timestamp(end)}", text, ""]
    (out / "captions.vtt").write_text("\n".join(vtt), encoding="utf-8")
    expect_within("Film", [out / name for name in BUDGET], sum(BUDGET.values()))
    for name, limit in BUDGET.items():
        expect_within(name, [out / name], limit)


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
    parser.add_argument("--browser", type=Path, default=ROOT / "build" / "ci" / "omaweb")
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
