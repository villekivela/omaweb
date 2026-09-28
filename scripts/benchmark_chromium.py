#!/usr/bin/env python3
"""Run the same benchmarks in Omaweb and in Chromium and report Omaweb's score as a share of each.

Everything else under `performance/` holds Omaweb against an earlier Omaweb. This holds it against
two Chromiums, because a raw score cannot tell Omaweb's own cost from the engine's version lag:

- `latest` is the stable Chromium a reader of Arch has, whose gap is the one readers see.
- `matched` is the Chromium major the engine Omaweb loaded is built on, whose gap is the one Omaweb
  and Qt WebEngine add. Its version is read from the browser under test, never written down here,
  so an engine update moves the baseline with it. The binary comes from the Chromium builds the
  Playwright project publishes for every release, for both aarch64 and x86_64, and is cached
  outside the repository, one per version.

The suites are Speedometer 3.1, JetStream 2.2 and MotionMark 1.3.2, each pinned to a commit and a
digest, fetched once into the same cache and served from loopback, so a run needs no network once
the cache is filled and no suite changes under it.

Each run launches the browser on a fresh profile: scratch data and configuration roots for Omaweb,
so it reads none of the reader's settings or sync state, and a scratch `--user-data-dir` and
`XDG_CONFIG_HOME` for Chromium, so Arch's launcher reads no `chromium-flags.conf`. Both are driven
over DevTools, `--remote-debugging=<port>` for Omaweb and `--remote-debugging-port` for Chromium.
The runs alternate between the browsers, and the spread of the latest Chromium's runs is reported
as the check that the host was idle: a virtual machine's scores move with its host's load and the
guest cannot see it.

This reports and does not fail over a ceiling, because host noise here is larger than the gap it
measures. `--record` appends the run to `performance/history.jsonl`, and `plot` draws that file.
`--profile` adds a run of the first suite per browser under `perf record` and prints where the
busiest renderer's time went, by library and by symbol.

Usage:

    scripts/benchmark_chromium.py
    scripts/benchmark_chromium.py --suite speedometer --runs 5
    scripts/benchmark_chromium.py --omaweb build/dev/omaweb --record
    scripts/benchmark_chromium.py --profile
    scripts/benchmark_chromium.py plot --output build/performance-history.html
"""

from __future__ import annotations

import argparse
import base64
import dataclasses
import datetime
import functools
import hashlib
import http.server
import json
import os
import platform
import re
import shutil
import signal
import socket
import ssl
import struct
import subprocess
import sys
import tarfile
import tempfile
import threading
import time
import urllib.error
import urllib.parse
import urllib.request
import zipfile
from pathlib import Path

import benchmark_runtime as runtime
import performance_history as history

ROOT = Path(__file__).resolve().parent.parent
SECURITY_BASELINE = ROOT / "security" / "baseline.json"


@dataclasses.dataclass(frozen=True)
class Suite:
    """One benchmark, pinned, and how to start it and read its score over DevTools.

    `start` is evaluated until it answers true, for a suite that has no way to be told to start by
    its address. `result` is evaluated until it answers an object: `{score}` when the run finished,
    `{error}` when the suite says it failed.
    """

    name: str
    title: str
    version: str
    repository: str
    commit: str
    sha256: str
    entry: str
    result: str
    start: str = ""
    timeout: float = 1800.0

    @property
    def directory(self) -> str:
        return f"{self.name}-{self.commit[:12]}"

    @property
    def archive(self) -> str:
        return f"https://codeload.github.com/{self.repository}/tar.gz/{self.commit}"


SUITES = {
    suite.name: suite for suite in (
        Suite(
            name="speedometer",
            title="Speedometer",
            version="3.1",
            repository="WebKit/Speedometer",
            commit="1386415be8fef2f6b6bbdbe1828872471c5d802a",
            sha256="cfefa818d7789ed2f2b9f3f1bf608cc35e4e8241489eef47049296ce92791313",
            entry="index.html?startAutomatically",
            result="""(() => {
                const client = globalThis.benchmarkClient;
                if (!client || !client._hasResults) return null;
                const score = client.metrics && client.metrics.Score;
                return score ? {score: score.mean} : {error: "Speedometer ended without a score"};
            })()""",
        ),
        Suite(
            name="jetstream",
            title="JetStream",
            version="2.2",
            repository="WebKit/JetStream",
            commit="332d8ee1e4d5c40d9492c56f6865e64a9525ee9f",
            sha256="d81329a7558faa9f7357e2956b3dfcb3028999da1ba14b696f3626d0e30d4281",
            # `report=true` starts the run once the resources have loaded and posts the results to
            # `/report`, which the server here accepts and ignores.
            entry="index.html?report=true",
            result="""(() => {
                if (window.allIsGood === false) return {error: "JetStream reported an error"};
                if (!document.querySelector("#result-summary.done")) return null;
                return {score: geomean(JetStream.benchmarks.map(benchmark => benchmark.score))};
            })()""",
        ),
        Suite(
            name="motionmark",
            title="MotionMark",
            version="1.3.2",
            repository="WebKit/MotionMark",
            commit="0e740d50f2321d255f6176e2f57493574c735996",
            sha256="14b746f05b388a382d315186efd6e19017f7f67b3846d20759ab30fe83c2f985",
            entry="MotionMark/index.html",
            # The start button is enabled once the frame rate has been measured, which is the
            # moment a reader could press it. A start before the page's images have loaded does
            # nothing, as one did in Chromium 153, so the run counts as started only once the test
            # is showing, and a start that has not taken after ten seconds is made again.
            start="""(() => {
                if (document.body.classList.contains("showing-test-container")) return true;
                const button = document.getElementById("start-button");
                if (!button || button.disabled) return false;
                if (!document.body.classList.contains("images-loaded")) return false;
                if (!window.omawebStarted || Date.now() - window.omawebStarted > 10000) {
                    window.omawebStarted = Date.now();
                    benchmarkController.startBenchmark();
                }
                return false;
            })()""",
            result="""(() => {
                if (!document.body.classList.contains("showing-results")) return null;
                return {score: benchmarkRunnerClient.results.score};
            })()""",
        ),
    )
}

# MotionMark measures the compositing path, which software compositing is not.
GPU_SUITES = {"motionmark"}

# A browser's DevTools endpoint answers once its network service is up, which on a loaded virtual
# machine is seconds after the launch rather than milliseconds.
DEVTOOLS_TIMEOUT = 90.0

# How often a run is asked whether it has finished. A run is minutes and a poll is a round trip.
POLL_INTERVAL = 2.0

# How many times one run may lose its DevTools connection and take it again.
MAX_RECONNECTS = 5

# The busiest renderer is chosen after the page has been running long enough to be the one doing
# the work, and recorded for as long as #356 did by hand.
PROFILE_DELAY = 15.0
PROFILE_SECONDS = 30
PROFILE_FREQUENCY = 999
PROFILE_LIBRARIES = 10
PROFILE_SYMBOLS = 20

# Where Playwright says which Chromium each of its releases carries, and where it keeps the builds.
PLAYWRIGHT_RELEASES = "https://api.github.com/repos/microsoft/playwright/releases?per_page=100&page={}"
PLAYWRIGHT_BROWSERS = ("https://raw.githubusercontent.com/microsoft/playwright/{}/packages/"
                       "playwright-core/browsers.json")
PLAYWRIGHT_BUILD = "https://cdn.playwright.dev/builds/chromium/{}/{}"
PLAYWRIGHT_ARCHIVES = {"x86_64": "chromium-linux.zip", "aarch64": "chromium-linux-arm64.zip"}

# What the report calls each browser.
BROWSER_NAMES = {
    history.OMAWEB: "Omaweb",
    history.LATEST: "latest Chromium",
    history.MATCHED: "matched Chromium",
}

NO_GPU_FLAGS = ("--disable-gpu", "--disable-gpu-compositing")

# The switches that choose how a browser draws. Omaweb's are handed to both Chromiums by default,
# so all three composite the same way: on a guest where Omaweb has to go around ANGLE, a Chromium
# left on its own falls back to software, and the comparison would measure the compositors. Any
# other switch in `QTWEBENGINE_CHROMIUM_FLAGS` is Omaweb's business and stays with it.
GL_FLAGS = ("--use-gl", "--use-angle", "--use-vulkan", "--disable-gpu", "--enable-gpu",
            "--ignore-gpu-blocklist", "--disable-software-rasterizer", "--enable-zero-copy")


class Unavailable(RuntimeError):
    """This machine cannot run the comparison as asked, which is not a failed run."""


class IntegrityFailed(RuntimeError):
    """A download is not the file it was the first time, which fails the run rather than skipping
    it: a comparison against a binary nobody can name is not one worth taking."""


class RunFailed(RuntimeError):
    """A browser did not finish a suite, which is a run worth a non-zero exit."""


class DevToolsClosed(RunFailed):
    """The DevTools connection went away, which the page it was attached to may well outlive."""


def log(message: str) -> None:
    print(message, flush=True)


def cache_root() -> Path:
    base = os.environ.get("XDG_CACHE_HOME") or os.path.join(os.path.expanduser("~"), ".cache")
    return Path(base) / "omaweb-benchmarks"


def download(url: str, target: Path) -> None:
    """Fetches over HTTPS with the certificate verified, and refuses anything else.

    A redirect is followed, so where the answer came from is checked as well as where the request
    went: a redirect to plain HTTP would otherwise hand the file to anyone on the path.
    """
    if urllib.parse.urlsplit(url).scheme != "https":
        raise IntegrityFailed(f"{url} is not HTTPS, and nothing here is fetched without it")
    partial = target.with_name(target.name + ".part")
    target.parent.mkdir(parents=True, exist_ok=True)
    log(f"  fetching {url}")
    request = urllib.request.Request(url, headers={"User-Agent": "omaweb-benchmark"})
    with urllib.request.urlopen(request, timeout=120, context=ssl.create_default_context()) \
            as response:
        if urllib.parse.urlsplit(response.geturl()).scheme != "https":
            raise IntegrityFailed(f"{url} was redirected to {response.geturl()}, which is not HTTPS")
        with open(partial, "wb") as handle:
            shutil.copyfileobj(response, handle, 1 << 20)
    partial.rename(target)


def digest(path: Path) -> str:
    hashed = hashlib.sha256()
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(1 << 20), b""):
            hashed.update(block)
    return hashed.hexdigest()


def fetch_suite(suite: Suite, root: Path) -> Path:
    """The suite's files, fetched and checked the first time and read from the cache after."""
    directory = root / suite.directory
    if (directory / ".complete").exists():
        return directory
    archive = root / f"{suite.directory}.tar.gz"
    if not archive.exists():
        download(suite.archive, archive)
    found = digest(archive)
    if found != suite.sha256:
        archive.unlink()
        raise Unavailable(f"{suite.title} {suite.version} fetched with digest {found}, not the "
                          f"pinned {suite.sha256}, so it is not the suite this was pinned to")
    shutil.rmtree(directory, ignore_errors=True)
    directory.mkdir(parents=True)
    with tarfile.open(archive) as bundle:
        for member in bundle.getmembers():
            # The archive holds one top directory named for the commit, which is dropped.
            parts = Path(member.name).parts[1:]
            if not parts or ".." in parts:
                continue
            member.name = str(Path(*parts))
            bundle.extract(member, directory, filter="data")
    (directory / ".complete").touch()
    archive.unlink()
    return directory


class SuiteServer:
    """The cached suites, served from loopback with nothing cached by the browser."""

    def __init__(self, root: Path) -> None:
        class Handler(http.server.SimpleHTTPRequestHandler):
            def __init__(self, *arguments, **keywords) -> None:
                super().__init__(*arguments, directory=str(root), **keywords)

            def log_message(self, format: str, *arguments) -> None:  # noqa: A002
                pass

            def end_headers(self) -> None:
                self.send_header("Cache-Control", "no-store")
                super().end_headers()

            def do_POST(self) -> None:  # noqa: N802
                # JetStream posts its results when asked to report; they are read over DevTools.
                self.rfile.read(int(self.headers.get("Content-Length", "0")))
                self.send_response(200)
                self.send_header("Content-Length", "0")
                self.end_headers()

        self.server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        self.server.daemon_threads = True
        self.origin = f"http://127.0.0.1:{self.server.server_address[1]}"
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()

    def address(self, suite: Suite) -> str:
        return f"{self.origin}/{suite.directory}/{suite.entry}"

    def stop(self) -> None:
        self.server.shutdown()
        self.server.server_close()


# The matched Chromium.


def major(version: str) -> int:
    try:
        return int(version.split(".", 1)[0])
    except ValueError:
        return 0


def release_order(tag: str) -> tuple[int, ...]:
    return tuple(int(part) for part in re.findall(r"\d+", tag))


def find_release(tags: list[str], wanted: int, chromium_of) -> tuple[str, dict] | None:
    """The newest release whose Chromium has the wanted major, by bisecting the releases.

    Playwright's Chromium only moves forward from one release to the next, so the releases sort
    into runs of one major each, and a bisection reads a handful of manifests rather than hundreds.
    `chromium_of` answers a release's Chromium entry, or `None` for a release that has none.
    """
    ordered = sorted(tags, key=release_order)
    low, high = 0, len(ordered)
    while low < high:
        middle = (low + high) // 2
        entry = chromium_of(ordered[middle])
        if entry is not None and major(entry["browserVersion"]) <= wanted:
            low = middle + 1
        else:
            high = middle
    if low == 0:
        return None
    entry = chromium_of(ordered[low - 1])
    if entry is None or major(entry["browserVersion"]) != wanted:
        return None
    return ordered[low - 1], entry


def fetch_json(url: str):
    request = urllib.request.Request(url, headers={"User-Agent": "omaweb-benchmark",
                                                   "Accept": "application/json"})
    with urllib.request.urlopen(request, timeout=60) as response:
        return json.load(response)


def playwright_releases() -> list[str]:
    tags = []
    for page in range(1, 20):
        answer = fetch_json(PLAYWRIGHT_RELEASES.format(page))
        if not answer:
            break
        tags += [release["tag_name"] for release in answer
                 if not release.get("prerelease") and not release.get("draft")]
    return tags


@functools.cache
def playwright_chromium(tag: str) -> dict | None:
    try:
        manifest = fetch_json(PLAYWRIGHT_BROWSERS.format(tag))
    except (urllib.error.URLError, json.JSONDecodeError):
        return None
    return next((browser for browser in manifest.get("browsers", [])
                 if browser.get("name") == "chromium"), None)


def extract_zip(archive: Path, target: Path) -> None:
    """Unpacks keeping each file's mode, which `zipfile` otherwise drops and a browser needs."""
    with zipfile.ZipFile(archive) as bundle:
        for info in bundle.infolist():
            path = Path(bundle.extract(info, target))
            mode = (info.external_attr >> 16) & 0o777
            if mode:
                path.chmod(mode)


@dataclasses.dataclass(frozen=True)
class MatchedChromium:
    """The matched Chromium's executable, where it came from, and the digest of what was run."""

    executable: str
    source: str
    sha256: str
    hashed: str


def matched_chromium(wanted: int, root: Path) -> MatchedChromium:
    """A Chromium of the wanted major for this machine, from an archive verified on every run.

    Playwright publishes no digest to check a build against, so the one pinned is the archive's
    own on its first fetch, recorded in `majors.json`. Every later run hashes the kept archive
    against it, and a fetch after that is held to it too, so the history can say exactly which
    binary a run measured and a build that changed under the same name fails the run. The build
    is extracted afresh from the verified archive each run, so what runs is what was hashed.
    """
    architecture = platform.machine()
    archive_name = PLAYWRIGHT_ARCHIVES.get(architecture)
    if archive_name is None:
        raise Unavailable(f"no Chromium builds are published for {architecture}")
    index = root / "chromium" / "majors.json"
    known = json.loads(index.read_text()) if index.exists() else {}
    if str(wanted) not in known:
        log(f"  looking for a Chromium {wanted} build")
        try:
            found = find_release(playwright_releases(), wanted, playwright_chromium)
        except urllib.error.URLError as error:
            raise Unavailable(f"the Chromium builds could not be listed: {error}") from error
        if found is None:
            raise Unavailable(f"no Playwright release carries Chromium {wanted}; pass "
                              "--matched-chromium with a Chromium of that major")
        tag, entry = found
        known[str(wanted)] = {"tag": tag, "revision": entry["revision"],
                              "version": entry["browserVersion"]}
    build = known[str(wanted)]
    name = f"{build['version']}-{architecture}"
    archive = root / "chromium" / f"{name}.zip"
    recorded = build.get("sha256", "")
    if archive.exists() and not recorded:
        # An archive with no digest beside it is not one this can vouch for.
        archive.unlink()
    if not archive.exists():
        download(PLAYWRIGHT_BUILD.format(build["revision"], archive_name), archive)
    found = digest(archive)
    if recorded and found != recorded:
        raise IntegrityFailed(
            f"the Chromium {build['version']} archive {archive} has digest {found}, and its first "
            f"fetch had {recorded}. Delete the archive and its entry in {index} only if the "
            "change is known to be benign.")
    if not recorded:
        build["sha256"] = found
        index.parent.mkdir(parents=True, exist_ok=True)
        index.write_text(json.dumps(known, indent=2) + "\n")
    directory = root / "chromium" / name
    shutil.rmtree(directory, ignore_errors=True)
    extract_zip(archive, directory)
    executable = next(directory.glob("*/chrome"), None)
    if executable is None:
        raise Unavailable(f"the Chromium {build['version']} archive holds no chrome")
    return MatchedChromium(str(executable), f"Playwright {build['tag']}, build {build['revision']}",
                           found, archive_name)


def given_chromium(executable: str) -> MatchedChromium:
    """A matched Chromium the reader named, identified by its executable's digest."""
    return MatchedChromium(executable, "given with --matched-chromium",
                           digest(Path(executable)), os.path.basename(executable))


# DevTools, over the one WebSocket each browser offers. The standard library has no WebSocket
# client, and the protocol needs a small part of one: text frames out, masked, and text frames in.


def encode_frame(text: str, mask: bytes) -> bytes:
    payload = text.encode()
    length = len(payload)
    if length < 126:
        header = struct.pack("!BB", 0x81, 0x80 | length)
    elif length < 1 << 16:
        header = struct.pack("!BBH", 0x81, 0x80 | 126, length)
    else:
        header = struct.pack("!BBQ", 0x81, 0x80 | 127, length)
    return header + mask + bytes(byte ^ mask[index % 4] for index, byte in enumerate(payload))


def read_frame(receive) -> tuple[int, bool, bytes]:
    """One frame from `receive(count)`: its opcode, whether it ends a message, and its payload."""
    first, second = receive(2)
    length = second & 0x7F
    if length == 126:
        length = struct.unpack("!H", receive(2))[0]
    elif length == 127:
        length = struct.unpack("!Q", receive(8))[0]
    mask = receive(4) if second & 0x80 else b""
    payload = receive(length) if length else b""
    if mask:
        payload = bytes(byte ^ mask[index % 4] for index, byte in enumerate(payload))
    return first & 0x0F, bool(first & 0x80), payload


class DevTools:
    def __init__(self, address: str, timeout: float = 60.0) -> None:
        parts = urllib.parse.urlsplit(address)
        self.socket = socket.create_connection((parts.hostname, parts.port), timeout=timeout)
        key = base64.b64encode(os.urandom(16)).decode()
        self.socket.sendall(
            (f"GET {parts.path} HTTP/1.1\r\nHost: {parts.netloc}\r\nUpgrade: websocket\r\n"
             f"Connection: Upgrade\r\nSec-WebSocket-Key: {key}\r\n"
             "Sec-WebSocket-Version: 13\r\n\r\n").encode())
        answer = b""
        while b"\r\n\r\n" not in answer:
            chunk = self.socket.recv(4096)
            if not chunk:
                raise RunFailed("DevTools closed the connection during the handshake")
            answer += chunk
        head, self.pending = answer.split(b"\r\n\r\n", 1)
        if b" 101 " not in head.split(b"\r\n", 1)[0]:
            raise RunFailed(f"DevTools refused the connection: {head.decode(errors='replace')}")
        self.next_id = 0

    def _receive(self, count: int) -> bytes:
        while len(self.pending) < count:
            chunk = self.socket.recv(max(count - len(self.pending), 65536))
            if not chunk:
                raise DevToolsClosed("DevTools closed the connection")
            self.pending += chunk
        taken, self.pending = self.pending[:count], self.pending[count:]
        return taken

    def _message(self) -> dict:
        payload = b""
        while True:
            opcode, final, data = read_frame(self._receive)
            if opcode == 0x8:
                raise DevToolsClosed("DevTools closed the connection")
            if opcode == 0x9:
                continue
            payload += data
            if final:
                return json.loads(payload)

    def call(self, method: str, params: dict | None = None) -> dict:
        self.next_id += 1
        self.socket.sendall(encode_frame(
            json.dumps({"id": self.next_id, "method": method, "params": params or {}}),
            os.urandom(4)))
        while True:
            message = self._message()
            if message.get("id") == self.next_id:
                if "error" in message:
                    raise RunFailed(f"{method}: {message['error'].get('message')}")
                return message.get("result", {})

    def evaluate(self, expression: str):
        result = self.call("Runtime.evaluate", {"expression": expression, "returnByValue": True})
        if "exceptionDetails" in result:
            return None
        return result.get("result", {}).get("value")

    def close(self) -> None:
        self.socket.close()


def devtools_json(port: int, path: str):
    with urllib.request.urlopen(f"http://127.0.0.1:{port}{path}", timeout=5) as response:
        return json.load(response)


def free_port() -> int:
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", 0))
        return probe.getsockname()[1]


# The browsers.


@dataclasses.dataclass
class BrowserSpec:
    """A browser the comparison runs: which one, the executable, and the flags it is given."""

    role: str
    executable: str
    flags: str = ""
    source: str = ""
    # The digest of what was run, and of which file, where the harness fetched or was given it.
    sha256: str = ""
    hashed: str = ""

    @property
    def is_omaweb(self) -> bool:
        return self.role == history.OMAWEB


def gl_flags(flags: str) -> str:
    """The switches of a command line that choose how the browser draws."""
    return " ".join(flag for flag in flags.split()
                    if any(flag == name or flag.startswith(name + "=") or
                           flag.startswith(name + "-") for name in GL_FLAGS))


def chromium_flags(omaweb_flags: str, override: str | None) -> str:
    """What both Chromiums are launched with: Omaweb's GL flags, unless `--chromium-flags` said
    otherwise, an empty value included."""
    return gl_flags(omaweb_flags) if override is None else override


def command_line(spec: BrowserSpec, root: str, port: int, url: str) -> tuple[list[str], dict]:
    """How a browser is launched on a fresh profile, and the environment it is launched with.

    Omaweb's GL flags are the engine's, which it reads from `QTWEBENGINE_CHROMIUM_FLAGS` and not
    from its own arguments. Chromium's configuration home is a scratch one, because Arch's launcher
    reads `chromium-flags.conf` from it, and a reader's flags file can load extensions.
    """
    environment = dict(os.environ)
    if spec.is_omaweb:
        environment["OMAWEB_DATA_ROOT"] = os.path.join(root, "data")
        environment["OMAWEB_CONFIG_ROOT"] = os.path.join(root, "config")
        if spec.flags:
            environment["QTWEBENGINE_CHROMIUM_FLAGS"] = spec.flags
        else:
            environment.pop("QTWEBENGINE_CHROMIUM_FLAGS", None)
        # A private session bus, so the launch does not hand its address to an Omaweb the reader
        # already has open and exit.
        return (["dbus-run-session", "--", spec.executable, f"--remote-debugging={port}", url],
                environment)
    environment["XDG_CONFIG_HOME"] = os.path.join(root, "config")
    return ([spec.executable, f"--user-data-dir={os.path.join(root, 'profile')}",
             f"--remote-debugging-port={port}", "--no-first-run", "--no-default-browser-check",
             "--password-store=basic", "--ozone-platform-hint=auto", *spec.flags.split(), url],
            environment)


def process_tree(root: int) -> list[int]:
    tree = runtime.children_by_parent()
    found, pending = [], [root]
    while pending:
        pid = pending.pop()
        found.append(pid)
        pending += tree.get(pid, [])
    return found


def command_of(pid: int) -> str:
    """A process's command line as one string. The zygote rewrites its children's titles into one
    argument with spaces in it, so a renderer's switches are not separate arguments to look for."""
    try:
        with open(f"/proc/{pid}/cmdline", "rb") as handle:
            return handle.read().replace(b"\0", b" ").decode(errors="replace")
    except OSError:
        return ""


def is_page_renderer(command: str) -> bool:
    """A renderer of a page, rather than of Chromium's own interface, which the busiest-renderer
    choice would otherwise be free to profile when the page is between tests."""
    arguments = command.split()
    return "--type=renderer" in arguments and "--top-chrome-webui" not in arguments


def cpu_seconds(pid: int) -> float:
    try:
        with open(f"/proc/{pid}/stat", encoding="utf-8") as handle:
            fields = handle.read().rsplit(")", 1)[1].split()
    except (OSError, IndexError):
        return 0.0
    return (int(fields[11]) + int(fields[12])) / os.sysconf("SC_CLK_TCK")


def is_software(gpu: dict, flags: str) -> bool:
    """Whether a browser composited without a GPU, which makes its MotionMark score no result."""
    if any(flag in flags.split() or flags.startswith(flag + "=") for flag in NO_GPU_FLAGS):
        return True
    compositing = gpu.get("gpu_compositing", "")
    return bool(compositing) and not compositing.startswith("enabled")


class RunningBrowser:
    """One launch, on its own throwaway profile, driven over DevTools."""

    def __init__(self, spec: BrowserSpec, url: str) -> None:
        self.spec = spec
        self.root = tempfile.mkdtemp(prefix=f"omaweb-compare-{spec.role}-")
        self.port = free_port()
        if spec.is_omaweb:
            # Content blocking as a first run leaves it, but from the committed lists: a first run
            # would otherwise fetch and compile them in the middle of the benchmark.
            runtime.seed_content_blocking(os.path.join(self.root, "data"), [], [])
        self.page: DevTools | None = None
        self.process: subprocess.Popen | None = None
        self.log = None
        # Nothing outside this constructor holds the browser until it returns, so a launch that
        # fails, times out or is interrupted ends the browser here, whole process group and
        # `dbus-run-session` included, rather than leaving it running into the next measurement.
        try:
            command, environment = command_line(spec, self.root, self.port, url)
            self.log = open(os.path.join(self.root, "browser.log"), "w", encoding="utf-8")
            self.process = subprocess.Popen(command, env=environment, stdout=self.log,
                                            stderr=subprocess.STDOUT, start_new_session=True)
            self.version = self._await_devtools()
        except BaseException:
            self.stop()
            raise

    def _await_devtools(self) -> dict:
        deadline = time.time() + DEVTOOLS_TIMEOUT
        while time.time() < deadline:
            if self.process.poll() is not None:
                raise RunFailed(f"{BROWSER_NAMES[self.spec.role]} exited before DevTools answered: "
                                + self.tail())
            try:
                return devtools_json(self.port, "/json/version")
            except (OSError, ValueError):
                time.sleep(0.5)
        raise RunFailed(f"{BROWSER_NAMES[self.spec.role]}'s DevTools never answered")

    def tail(self, lines: int = 15) -> str:
        self.log.flush()
        with open(self.log.name, encoding="utf-8", errors="replace") as handle:
            return "\n    ".join([""] + handle.read().splitlines()[-lines:])

    def attach(self, origin: str) -> DevTools:
        """The page target the suite is loading in, once there is one."""
        deadline = time.time() + DEVTOOLS_TIMEOUT
        while time.time() < deadline:
            for target in devtools_json(self.port, "/json/list"):
                if target.get("type") == "page" and target.get("url", "").startswith(origin):
                    self.page = DevTools(target["webSocketDebuggerUrl"])
                    return self.page
            time.sleep(0.5)
        raise RunFailed(f"{BROWSER_NAMES[self.spec.role]} never opened the suite's page")

    @property
    def chromium_version(self) -> str:
        """The Chromium version this browser says it is, from the `Chrome/` token DevTools gives.

        Qt WebEngine gives its own name first and Chromium's after it, so the token is searched
        for rather than taken from the start."""
        text = " ".join(str(self.version.get(key, "")) for key in ("Browser", "User-Agent"))
        match = re.search(r"Chrome/([\d.]+)", text)
        return match.group(1) if match else ""

    def gpu(self) -> dict:
        """Chromium's own word on GPU compositing, where DevTools gives it.

        Not the WebGL renderer the page sees: creating a WebGL context to ask it left Omaweb not
        responding on a virgl guest with GPU compositing off, which is the machine this runs on.
        """
        address = self.version.get("webSocketDebuggerUrl")
        if not address:
            return {}
        try:
            browser = DevTools(address, timeout=10)
            try:
                status = browser.call("SystemInfo.getInfo")["gpu"].get("featureStatus", {})
            finally:
                browser.close()
        except (OSError, RunFailed, KeyError):
            return {}
        return {"gpu_compositing": status.get("gpu_compositing", "")}

    def engine_library(self) -> str:
        """The engine library the browser actually mapped, which for Omaweb says whose engine it
        is: ours under `/usr/lib/omaweb`, or Arch's `qt6-webengine`."""
        for pid in process_tree(self.process.pid):
            try:
                with open(f"/proc/{pid}/maps", encoding="utf-8") as handle:
                    for line in handle:
                        if history.ENGINE_LIBRARY.search(line):
                            return line.split(None, 5)[-1].strip()
            except OSError:
                continue
        return ""

    def busiest_renderer(self) -> int:
        renderers = [pid for pid in process_tree(self.process.pid)
                     if is_page_renderer(command_of(pid))]
        if not renderers:
            raise RunFailed(f"{BROWSER_NAMES[self.spec.role]} has no renderer to profile")
        before = {pid: cpu_seconds(pid) for pid in renderers}
        time.sleep(2)
        return max(renderers, key=lambda pid: cpu_seconds(pid) - before[pid])

    def stop(self) -> None:
        if self.page:
            self.page.close()
            self.page = None
        if self.process:
            # The browser leads a session of its own, so its process id is the group's.
            for stage in (signal.SIGTERM, signal.SIGKILL):
                try:
                    os.killpg(self.process.pid, stage)
                except (ProcessLookupError, PermissionError):
                    break
                try:
                    self.process.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    pass
            self.process = None
        if self.log:
            self.log.close()
            self.log = None
        shutil.rmtree(self.root, ignore_errors=True)


def run_suite(browser: RunningBrowser, suite: Suite, origin: str, during=None) -> float:
    """Runs the suite to its score. `during` is called once the suite has started, and the run
    is waited for after it returns, which is how a profile is taken of a run in progress.

    A connection DevTools drops is taken again rather than failing the run: the page goes on
    running without it, and Qt WebEngine has been seen closing one in the middle of a run.
    """
    name = BROWSER_NAMES[browser.spec.role]
    page = browser.attach(origin)
    deadline = time.time() + suite.timeout
    started = not suite.start
    reconnects = 0
    while time.time() < deadline:
        if browser.process.poll() is not None:
            raise RunFailed(f"{name} exited during {suite.title}: " + browser.tail())
        try:
            if not started:
                started = bool(page.evaluate(suite.start))
            elif during:
                during()
                during = None
                continue
            else:
                answer = page.evaluate(suite.result)
                if isinstance(answer, dict):
                    if "error" in answer:
                        raise RunFailed(f"{suite.title} in {name}: {answer['error']}")
                    return float(answer["score"])
        except (DevToolsClosed, OSError) as error:
            reconnects += 1
            if reconnects > MAX_RECONNECTS:
                raise RunFailed(f"{name}'s DevTools connection kept closing: {error}") from error
            log(f"  DevTools dropped the connection ({error}), taking it again")
            page.close()
            browser.page = None
            page = browser.attach(origin)
            continue
        time.sleep(POLL_INTERVAL)
    raise RunFailed(f"{suite.title} did not finish in {name} within "
                    f"{suite.timeout / 60:.0f} minutes")


# The plan and the report.


@dataclasses.dataclass(frozen=True)
class Run:
    suite: str
    browser: str
    number: int


def plan(suites: list[str], browsers: list[str], runs: int) -> list[Run]:
    """Every run in the order it happens: each suite in turn, the browsers alternating within it.

    The order rotates from one round to the next, so no browser always runs straight after the
    same one, and a host that drifts over a suite drifts over all three browsers.
    """
    order = []
    for suite in suites:
        for number in range(runs):
            shift = number % len(browsers)
            for browser in browsers[shift:] + browsers[:shift]:
                order.append(Run(suite, browser, number))
    return order


def notes(record: dict, approved: str) -> list[str]:
    """What a reader has to know before reading the ratios."""
    found = []
    engine = record["engine"].get("chromium", "")
    if approved and engine and engine != approved:
        found.append(f"the engine reports Chromium {engine}, and security/baseline.json approves "
                     f"{approved}")
    matched = record["browsers"].get(history.MATCHED, {}).get("version", "")
    if matched and engine and major(matched) != major(engine):
        found.append(f"the matched Chromium is {matched}, which is not the engine's major, "
                     f"{major(engine)}")
    for suite in record.get("no_gpu", []):
        found.append(f"{SUITES[suite].title if suite in SUITES else suite} ran without GPU "
                     "compositing in at least one browser, so its scores are not a result")
    return found


def comparison_record(scores: dict, browsers: dict, *, suites: list[str], engine: dict,
                      commit: str, omaweb: str, machine: str, date: str,
                      approved: str) -> dict:
    no_gpu = [suite for suite in suites if suite in GPU_SUITES
              and any(browser.get("no_gpu") for browser in browsers.values())]
    return {
        "kind": history.KIND_COMPARISON,
        "date": date,
        "commit": commit,
        "omaweb": omaweb,
        "machine": machine,
        "engine": engine,
        "approved_chromium": approved,
        "browsers": browsers,
        "suites": {name: {"version": SUITES[name].version, "commit": SUITES[name].commit}
                   for name in suites},
        "scores": scores,
        "no_gpu": no_gpu,
    }


def report_lines(record: dict) -> list[str]:
    lines = [""]
    engine = record["engine"]
    lines.append(f"machine: {record['machine']}")
    lines.append(f"engine: {engine.get('library') or 'unknown library'} "
                 f"({history.engine_label(engine)}, Qt WebEngine {engine.get('qtwebengine')}, "
                 f"Chromium {engine.get('chromium')})")
    for role, browser in record["browsers"].items():
        gpu = browser.get("gpu", {})
        lines.append(f"{BROWSER_NAMES[role]}: {browser.get('version')}"
                     + (f", from {browser['source']}" if browser.get("source") else "")
                     + f"; flags: {browser.get('flags') or 'none'}"
                     + f"; GPU compositing: {gpu.get('gpu_compositing') or 'not reported'}"
                     + (f"; {browser['sha256_of']} sha256 {browser['sha256']}"
                        if browser.get("sha256") else ""))
    lines.append("")
    ratios = history.comparison_ratios(record)
    for suite, scores in record["scores"].items():
        title = f"{SUITES[suite].title} {SUITES[suite].version}"
        if suite in record.get("no_gpu", []):
            title += " (without GPU compositing: not a result)"
        lines.append(title)
        for role, values in scores.items():
            line = (f"  {BROWSER_NAMES[role]:<17} " + ", ".join(f"{value:.2f}" for value in values)
                    + f"  mean {history.mean(values):.2f}")
            if role == history.LATEST:
                line += (f", spread {history.spread(values) * 100:.1f}% (the host check)"
                         if len(values) > 1 else ", one run, so no host check")
            lines.append(line)
        for baseline, value in ratios.get(suite, {}).items():
            lines.append(f"  Omaweb / {BROWSER_NAMES[baseline]}: {value:.3f}")
    found = notes(record, record.get("approved_chromium", ""))
    if found:
        lines.append("")
        lines += [f"note: {note}" for note in found]
    lines.append("")
    return lines


# Profiling.

PERF_SHARE = re.compile(r"^\s*([\d.]+)%\s+(.*?)\s*$")
PERF_SYMBOL = re.compile(r"\s+\[.\]\s+")


def parse_perf_report(text: str) -> list[tuple[float, str, str]]:
    """`perf report --stdio` lines as share, library and symbol, the symbol blank when the report
    was sorted by library alone.

    The library column is padded to its widest entry, and a JIT entry's name has spaces in it,
    `[JIT] tid 1234`, so the symbol is found by the `[.]` or `[k]` that opens it rather than by
    counting columns.
    """
    rows = []
    for line in text.splitlines():
        if line.startswith("#"):
            continue
        match = PERF_SHARE.match(line)
        if not match:
            continue
        parts = PERF_SYMBOL.split(match.group(2), 1)
        symbol = parts[1].strip() if len(parts) > 1 else ""
        rows.append((float(match.group(1)), parts[0].strip(), symbol))
    return rows


def profile(browser: RunningBrowser, directory: Path) -> list[str]:
    """Records the busiest renderer and answers where its time went."""
    if shutil.which("perf") is None:
        raise Unavailable("--profile needs perf")
    time.sleep(PROFILE_DELAY)
    pid = browser.busiest_renderer()
    directory.mkdir(parents=True, exist_ok=True)
    data = directory / f"{browser.spec.role}.data"
    recorded = subprocess.run(
        ["perf", "record", "-F", str(PROFILE_FREQUENCY), "-p", str(pid), "-o", str(data), "--",
         "sleep", str(PROFILE_SECONDS)], capture_output=True, text=True, check=False)
    if recorded.returncode or not data.exists():
        raise RunFailed(f"perf record on renderer {pid} failed: {recorded.stderr.strip()}")
    lines = [f"{BROWSER_NAMES[browser.spec.role]}, renderer {pid}, {PROFILE_SECONDS} s from "
             f"{PROFILE_DELAY:.0f} s in, {data}"]
    for sort, count, name, label in (("dso", PROFILE_LIBRARIES, "libraries", "library"),
                                     ("dso,sym", PROFILE_SYMBOLS, "symbols", "symbol")):
        text = subprocess.run(["perf", "report", "-i", str(data), "--stdio", "--no-children",
                               "--sort", sort], capture_output=True, text=True,
                              check=False).stdout
        (directory / f"{browser.spec.role}-{name}.txt").write_text(text)
        lines.append(f"  by {label}:")
        lines += [f"    {share:6.2f}%  {library}" + (f"  {symbol}" if symbol else "")
                  for share, library, symbol in parse_perf_report(text)[:count]]
    return lines


# The run.


def read_approved() -> str:
    try:
        return json.loads(SECURITY_BASELINE.read_text())["chromium"]
    except (OSError, KeyError, json.JSONDecodeError):
        return ""


def compare(arguments) -> int:
    if not os.environ.get("WAYLAND_DISPLAY"):
        log("skipped: this runs browsers on a Wayland display, and there is none")
        return 0
    omaweb = shutil.which(arguments.omaweb) or arguments.omaweb
    if not os.access(omaweb, os.X_OK):
        raise Unavailable(f"{arguments.omaweb} is not a browser this can run")
    latest = shutil.which(arguments.chromium) or arguments.chromium
    if not os.access(latest, os.X_OK):
        raise Unavailable(f"{arguments.chromium} is not a Chromium this can run")
    suites = arguments.suite or list(SUITES)
    cache = cache_root()

    log("fetching what the run needs:")
    for name in suites:
        fetch_suite(SUITES[name], cache / "suites")
    versions = history.read_version(omaweb)
    engine_chromium = versions.get("chromium", "")
    if not engine_chromium:
        raise Unavailable(f"{omaweb} --version did not name its Chromium")
    if arguments.matched_chromium:
        matched = given_chromium(arguments.matched_chromium)
    else:
        matched = matched_chromium(major(engine_chromium), cache)
    omaweb_flags = os.environ.get("QTWEBENGINE_CHROMIUM_FLAGS", "")
    flags = chromium_flags(omaweb_flags, arguments.chromium_flags)
    specs = {
        history.OMAWEB: BrowserSpec(history.OMAWEB, omaweb, omaweb_flags),
        history.LATEST: BrowserSpec(history.LATEST, latest, flags, latest),
        history.MATCHED: BrowserSpec(history.MATCHED, matched.executable, flags, matched.source,
                                     matched.sha256, matched.hashed),
    }
    server = SuiteServer(cache / "suites")

    scores: dict[str, dict[str, list[float]]] = {name: {role: [] for role in specs}
                                                  for name in suites}
    seen: dict[str, dict] = {}
    library = ""
    failure = ""
    try:
        runs = plan(suites, list(specs), arguments.runs)
        for index, run in enumerate(runs, 1):
            suite, spec = SUITES[run.suite], specs[run.browser]
            log(f"[{index}/{len(runs)}] {suite.title} in {BROWSER_NAMES[run.browser]}, "
                f"run {run.number + 1}")
            browser = RunningBrowser(spec, server.address(suite))
            try:
                score = run_suite(browser, suite, server.origin)
                if run.browser not in seen:
                    gpu = browser.gpu()
                    seen[run.browser] = {
                        "version": (versions.get("omaweb", "") if spec.is_omaweb
                                    else browser.chromium_version),
                        "chromium": browser.chromium_version,
                        "flags": spec.flags,
                        "source": spec.source,
                        **({"sha256": spec.sha256, "sha256_of": spec.hashed}
                           if spec.sha256 else {}),
                        "gpu": gpu,
                        "no_gpu": is_software(gpu, spec.flags),
                    }
                    if spec.is_omaweb:
                        library = browser.engine_library()
            finally:
                browser.stop()
            scores[run.suite][run.browser].append(score)
            log(f"  {score:.2f}")
        if arguments.profile:
            directory = cache / "profiles" / datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
            suite = SUITES[suites[0]]
            for role, spec in specs.items():
                log(f"profiling {suite.title} in {BROWSER_NAMES[role]}:")
                browser = RunningBrowser(spec, server.address(suite))
                lines: list[str] = []
                try:
                    score = run_suite(browser, suite, server.origin,
                                      during=lambda: lines.extend(profile(browser, directory)))
                finally:
                    browser.stop()
                log(f"  the profiled run scored {score:.2f}, which is not counted")
                for line in lines:
                    log(line)
    except (RunFailed, OSError) as error:
        # A socket that timed out is a browser that stopped answering, which is a failed run
        # rather than a traceback, and the scores taken before it are still worth printing.
        failure = f"{type(error).__name__}: {error}" if isinstance(error, OSError) else str(error)
        log(f"FAILED: {failure}")
    finally:
        server.stop()

    engine = history.describe_engine(omaweb, library)
    record = comparison_record(
        {suite: {role: values for role, values in by_role.items() if values}
         for suite, by_role in scores.items()},
        seen, suites=suites, engine=engine,
        commit=history.omaweb_commit(omaweb, versions.get("omaweb", "")),
        omaweb=versions.get("omaweb", ""), machine=arguments.machine or history.describe_machine(),
        date=history.now(), approved=read_approved())
    for line in report_lines(record):
        log(line)
    if arguments.record:
        if failure:
            log("not recorded: the run did not finish")
        else:
            history.append(record, Path(arguments.history))
            log(f"recorded into {arguments.history}")
    return 1 if failure else 0


def plot(arguments) -> int:
    records = history.read(Path(arguments.history))
    output = Path(arguments.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(history.render(records, arguments.history), encoding="utf-8")
    log(f"wrote {output} from {len(records)} runs")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("command", nargs="?", default="run", choices=("run", "plot"),
                        help="run the comparison, or plot the history; run by default")
    parser.add_argument("--omaweb", default="omaweb", help="the Omaweb to measure")
    parser.add_argument("--chromium", default="chromium",
                        help="the latest stable Chromium, Arch's by default")
    parser.add_argument("--matched-chromium", default="",
                        help="a Chromium of the engine's major, instead of fetching one")
    parser.add_argument("--chromium-flags", default=None,
                        help="flags for both Chromiums instead of Omaweb's GL flags from "
                        "QTWEBENGINE_CHROMIUM_FLAGS; an empty value passes none")
    parser.add_argument("--suite", action="append", choices=list(SUITES),
                        help="a suite to run, all of them by default; may be repeated")
    parser.add_argument("--runs", type=int, default=3, help="runs of each suite in each browser")
    parser.add_argument("--profile", action="store_true",
                        help="add a profiled run of the first suite per browser under perf")
    parser.add_argument("--record", action="store_true",
                        help="append the run to the history")
    parser.add_argument("--machine", default="",
                        help="what the history calls this machine, described from the hardware "
                        "by default")
    parser.add_argument("--history", default=str(history.HISTORY),
                        help="the history file to append to or plot")
    parser.add_argument("--output", default=str(ROOT / "build" / "performance-history.html"),
                        help="where plot writes its page")
    arguments = parser.parse_args()
    if arguments.runs < 1:
        parser.error("a comparison needs at least one run")
    if arguments.command == "plot":
        return plot(arguments)
    try:
        return compare(arguments)
    except Unavailable as error:
        log(f"skipped: {error}")
        return 0
    except IntegrityFailed as error:
        log(f"FAILED: {error}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
