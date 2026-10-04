#!/usr/bin/env python3
"""The introductory film's fixture sites, their server and the beat checks, without a browser.

`scripts/record_film.py` records the real browser in a container, and a recording that went wrong
still produces a video. What decides whether it went wrong is here: the sites the film browses
reach nothing real, the server answers each one under its own name and says what was asked of it,
and each beat check refuses the answer a broken beat would give. The recording itself needs the
engine and a compositor and runs through `scripts/record_film.sh`.
"""

from __future__ import annotations

import http.client
import io
import json
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT / "scripts"))

import record_film as film  # noqa: E402

# Anything the browser would fetch or follow: an attribute holding an address, a stylesheet's
# `url()`, and an address a script fetches or posts to.
ADDRESS = re.compile(
    r"""(?:\b(?:src|href|action|poster)\s*=\s*["']|url\(\s*["']?|fetch\(\s*["'`]"""
    r"""|sendBeacon\(\s*["'`])(?P<address>[^"'`)\s]+)"""
)


class FixtureSites(unittest.TestCase):
    def test_every_site_the_film_names_has_a_front_page(self):
        for host in film.SITES:
            with self.subTest(host=host):
                self.assertTrue((film.site_directory(host) / "index.html").is_file())

    def test_the_sites_reach_no_address_outside_the_film(self):
        for path in sorted(film.SITE_ROOT.rglob("*")):
            if path.suffix not in {".html", ".css", ".js", ".svg"}:
                continue
            for match in ADDRESS.finditer(path.read_text(encoding="utf-8")):
                address = match.group("address")
                if address.startswith(("#", "data:", "mailto:")) or "//" not in address:
                    continue
                host = re.sub(r"^[a-z]+:", "", address).lstrip("/").split("/")[0].split(":")[0]
                with self.subTest(file=str(path.relative_to(ROOT)), address=address):
                    self.assertIn(host, film.SITES)

    def test_every_relative_address_is_a_file_of_its_site(self):
        for host in film.SITES:
            directory = film.site_directory(host)
            for path in sorted(directory.rglob("*.html")) + sorted(directory.rglob("*.css")):
                for match in ADDRESS.finditer(path.read_text(encoding="utf-8")):
                    address = match.group("address").split("?")[0].split("#")[0]
                    if not address or "//" in address or address.startswith(("data:", "mailto:")):
                        continue
                    if address in film.SERVED_PATHS:
                        continue
                    base = directory if address.startswith("/") else path.parent
                    target = base / address.lstrip("/")
                    if target.is_dir():
                        target = target / "index.html"
                    with self.subTest(file=str(path.relative_to(ROOT)), address=address):
                        self.assertTrue(target.is_file(), f"{target} is not there")


    def test_every_file_a_site_holds_is_served_as_its_kind(self):
        for path in sorted(film.SITE_ROOT.rglob("*")):
            if path.is_file():
                with self.subTest(file=str(path.relative_to(ROOT))):
                    self.assertIn(path.suffix, film.CONTENT_TYPES)


class FixtureServerTest(unittest.TestCase):
    """The server the browser reaches every site through, asked the way the browser asks it."""

    def setUp(self):
        self.server = film.FixtureServer(("127.0.0.1", 0))
        self.server.start()
        self.addCleanup(self.server.stop)

    def ask(self, host, path, body=None, form=False):
        connection = http.client.HTTPConnection("127.0.0.1", self.server.port, timeout=5)
        self.addCleanup(connection.close)
        headers = {"Host": host}
        if body is not None:
            headers["Content-Type"] = (
                "application/x-www-form-urlencoded" if form else "text/plain;charset=UTF-8"
            )
        connection.request("POST" if body is not None else "GET", path, body, headers)
        response = connection.getresponse()
        text = response.read().decode(errors="replace")
        return response.status, response.getheader("Content-Type", ""), text

    def test_each_site_is_served_under_its_own_name(self):
        status, kind, page = self.ask("fernwood.test", "/products/trail-runner/")
        self.assertEqual(status, 200)
        self.assertIn("text/html", kind)
        self.assertIn("<title>Trail Runner 3 · Fernwood</title>", page)
        status, _, page = self.ask("quillstack.test", "/docs/tracing/")
        self.assertIn("<h1>Tracing requests</h1>", page)

    def test_a_name_the_film_does_not_use_is_refused_and_remembered(self):
        status, _, _ = self.ask("example.com", "/")
        self.assertEqual(status, 404)
        self.assertEqual(self.server.strays(), ["example.com/"])

    def test_every_request_is_kept_by_the_host_it_named(self):
        self.ask("adsprout.test", "/tag.js?slot=leader")
        self.ask("pixelmint.test", "/p.gif?site=halyard")
        self.assertEqual(self.server.paths("adsprout.test"), ["/tag.js?slot=leader"])
        self.assertEqual(self.server.paths("pixelmint.test"), ["/p.gif?site=halyard"])
        self.assertEqual(self.server.paths("halyard.test"), [])

    def test_the_search_engine_suggests_from_what_was_typed(self):
        status, kind, answer = self.ask("kestrel.test", "/suggest?q=tra")
        self.assertEqual(status, 200)
        self.assertIn("application/x-suggestions+json", kind)
        self.assertEqual(
            json.loads(answer),
            ["tra", ["trail running shoes", "trail running shoes waterproof", "trangia stove"]],
        )
        _, _, answer = self.ask("kestrel.test", "/suggest?q=zzz")
        self.assertEqual(json.loads(answer), ["zzz", []])

    def test_a_search_names_its_terms_and_cannot_be_written_into(self):
        _, _, page = self.ask("kestrel.test", "/search?q=trail+running+shoes")
        self.assertIn("<title>trail running shoes · Kestrel</title>", page)
        _, _, page = self.ask("kestrel.test", "/search?q=%3Cb%3E")
        self.assertIn("&lt;b&gt;", page)
        self.assertNotIn("<b>", page)

    def test_what_a_page_reports_is_kept(self):
        report = {"host": "quillstack.test", "path": "/docs/api/", "width": 812}
        status, _, _ = self.ask("quillstack.test", "/beacon", json.dumps(report))
        self.assertEqual(status, 204)
        self.assertEqual(self.server.reports(), [report])

    def test_every_site_carries_the_reporting_script(self):
        for host in ("fernwood.test", "tallyhaus.test"):
            status, kind, script = self.ask(host, "/film/report.js")
            self.assertEqual(status, 200)
            self.assertIn("javascript", kind)
            self.assertIn('fetch("/beacon"', script)

    def test_a_sent_invoice_is_kept_and_shown_back(self):
        body = "customer=Kiln+Coffee&amount=640.00&due=31+Oct+2026&note=%3Ci%3Ethanks"
        status, _, page = self.ask("tallyhaus.test", "/invoices", body, form=True)
        self.assertEqual(status, 200)
        self.assertIn("<h1>Invoice INV-1042 sent</h1>", page)
        self.assertIn("Kiln Coffee has it in their inbox", page)
        self.assertIn("&lt;i&gt;thanks", page)
        self.assertEqual(
            self.server.invoices(),
            [{"customer": "Kiln Coffee", "amount": "640.00", "due": "31 Oct 2026",
              "note": "<i>thanks"}],
        )


def spaces(on_show, *others):
    """A `spaces --json` answer, as the browser gives it."""
    names = [on_show, *others]
    return {"spaces": [
        {"id": f"s{index}", "name": name, "onShow": name == on_show, "agent": False}
        for index, name in enumerate(names)
    ]}


def tabs(current, *others):
    return {"tabs": [
        {"id": f"t{index}", "url": url, "title": "", "current": url == current, "pinned": False}
        for index, url in enumerate([current, *others])
    ]}


class BeatChecks(unittest.TestCase):
    """Each check passes the answer a beat that happened gives, and stops the one that did not."""

    def assertMissed(self, beat, check, *arguments):
        with self.assertRaises(film.BeatMissed) as raised:
            check(beat, *arguments)
        self.assertTrue(str(raised.exception).startswith(f"{beat}: "), str(raised.exception))
        return str(raised.exception)

    def test_a_space_switch_is_the_named_space_on_show(self):
        film.expect_space_on_show("Space switch", spaces("Work", "Personal"), "Work")
        message = self.assertMissed(
            "Space switch", film.expect_space_on_show, spaces("Personal", "Work"), "Work")
        self.assertIn("Personal", message)

    def test_a_space_that_does_not_exist_is_not_on_show(self):
        self.assertMissed("Space switch", film.expect_space_on_show, spaces("Personal"), "Work")

    def test_the_current_tab_is_the_address_the_beat_went_to(self):
        answer = tabs("http://kestrel.test/search?q=trail+running+shoes", "http://fernwood.test/")
        film.expect_current_tab(
            "Omnibar", answer, "http://kestrel.test/search?q=trail+running+shoes")
        message = self.assertMissed(
            "Omnibar", film.expect_current_tab, tabs("http://fernwood.test/"),
            "http://kestrel.test/search?q=trail+running+shoes")
        self.assertIn("http://fernwood.test/", message)

    def test_an_engine_suggestion_is_one_the_engine_was_asked_for(self):
        server = mock.Mock()
        server.paths.return_value = ["/suggest?q=t", "/suggest?q=tra"]
        film.expect_asked("Omnibar", server, "kestrel.test", "/suggest?q=tra")
        server.paths.return_value = []
        self.assertMissed("Omnibar", film.expect_asked, server, "kestrel.test", "/suggest?q=tra")

    def test_a_hidden_sidebar_widens_every_page_on_show(self):
        before = [{"host": "quillstack.test", "path": "/docs/tracing/", "width": 610},
                  {"host": "quillstack.test", "path": "/docs/api/", "width": 610}]
        after = before + [{"host": "quillstack.test", "path": "/docs/tracing/", "width": 760},
                          {"host": "quillstack.test", "path": "/docs/api/", "width": 760}]
        film.expect_wider("Sidebar", before, after, "quillstack.test")
        half = before + [{"host": "quillstack.test", "path": "/docs/tracing/", "width": 760}]
        message = self.assertMissed("Sidebar", film.expect_wider, before, half, "quillstack.test")
        self.assertIn("/docs/api/", message)

    def test_a_sidebar_beat_with_no_page_to_measure_is_missed(self):
        self.assertMissed("Sidebar", film.expect_wider, [], [], "quillstack.test")

    def test_blocking_is_a_magazine_that_loaded_and_reached_no_ad(self):
        server = mock.Mock()
        server.paths.return_value = []
        loaded = [{"host": "halyard.test", "path": "/2026/the-last-lighthouse-keepers/",
                   "adsShown": 0, "adCreatives": 0}]
        film.expect_blocked("Blocking", server, loaded, "halyard.test",
                            ("adsprout.test", "pixelmint.test"))

    def test_an_ad_that_was_fetched_misses_the_blocking_beat(self):
        server = mock.Mock()
        server.paths.side_effect = lambda host: ["/p.gif"] if host == "pixelmint.test" else []
        loaded = [{"host": "halyard.test", "path": "/", "adsShown": 0, "adCreatives": 0}]
        message = self.assertMissed("Blocking", film.expect_blocked, server, loaded,
                                    "halyard.test", ("adsprout.test", "pixelmint.test"))
        self.assertIn("pixelmint.test", message)

    def test_an_ad_slot_still_drawn_misses_the_blocking_beat(self):
        server = mock.Mock()
        server.paths.return_value = []
        drawn = [{"host": "halyard.test", "path": "/", "adsShown": 3, "adCreatives": 0}]
        self.assertMissed("Blocking", film.expect_blocked, server, drawn, "halyard.test",
                          ("adsprout.test",))

    def test_a_magazine_that_never_loaded_misses_the_blocking_beat(self):
        server = mock.Mock()
        server.paths.return_value = []
        self.assertMissed("Blocking", film.expect_blocked, server, [], "halyard.test",
                          ("adsprout.test",))

    def test_a_theme_change_moves_the_chrome_to_the_new_theme(self):
        retro, tokyo = (3, 18, 34), (19, 20, 28)
        film.expect_repainted("Theme", (4, 18, 33), (19, 21, 29), retro, tokyo)

    def test_chrome_that_kept_its_colour_misses_the_theme_beat(self):
        retro, tokyo = (3, 18, 34), (19, 20, 28)
        self.assertMissed("Theme", film.expect_repainted, (4, 18, 33), (4, 18, 33), retro, tokyo)

    def test_chrome_that_was_never_in_the_first_theme_misses_the_theme_beat(self):
        retro, tokyo = (3, 18, 34), (19, 20, 28)
        self.assertMissed("Theme", film.expect_repainted, (200, 200, 200), (19, 21, 29),
                          retro, tokyo)

    def test_a_frame_is_read_a_pixel_at_a_time(self):
        # Two by two, the second row blue then white.
        frame = b"P6\n2 2\n255\n" + bytes([255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 255])
        self.assertEqual(film.pixel(frame, 0, 0), (255, 0, 0))
        self.assertEqual(film.pixel(frame, 1, 0), (0, 255, 0))
        self.assertEqual(film.pixel(frame, 0, 1), (0, 0, 255))
        self.assertEqual(film.pixel(frame, 1, 1), (255, 255, 255))

    def test_a_target_is_found_by_what_the_agent_would_read(self):
        look = {"title": "New invoice · Tallyhaus", "url": "http://tallyhaus.test/invoices/new/",
                "targets": [{"label": "1", "kind": "link", "name": "Overview"},
                            {"label": "7", "kind": "combobox", "name": "Customer"},
                            {"label": "8", "kind": "textbox", "name": "Amount (USD)"},
                            {"label": "12", "kind": "button", "name": "Send invoice"}]}
        self.assertEqual(film.target("Agent", look, "Customer"), "7")
        self.assertEqual(film.target("Agent", look, "Amount"), "8")
        self.assertEqual(film.target("Agent", look, "Send invoice"), "12")
        message = self.assertMissed("Agent", film.target, look, "Due date")
        self.assertIn("Due date", message)

    def test_an_agent_command_that_failed_misses_the_agent_beat(self):
        film.expect_ran("Agent", subprocess.CompletedProcess(["omaweb", "look"], 0, "", ""))
        refused = subprocess.CompletedProcess(["omaweb", "do", "click 12"], 1, "", "denied\n")
        message = self.assertMissed("Agent", film.expect_ran, refused)
        self.assertIn("omaweb do", message)
        self.assertIn("denied", message)

    def test_the_agents_invoice_is_the_one_it_was_asked_to_send(self):
        server = mock.Mock()
        wanted = {"customer": "Kiln Coffee", "amount": "640.00"}
        server.invoices.return_value = [{**wanted, "due": "31 Oct 2026", "note": ""}]
        film.expect_invoice("Agent", server, wanted)
        server.invoices.return_value = [{"customer": "", "amount": "640.00"}]
        self.assertMissed("Agent", film.expect_invoice, server, wanted)
        server.invoices.return_value = []
        self.assertMissed("Agent", film.expect_invoice, server, wanted)

    def test_a_film_over_its_budget_is_refused(self):
        with tempfile.TemporaryDirectory() as directory:
            small, large = Path(directory, "a.webm"), Path(directory, "b.mp4")
            small.write_bytes(b"x" * 300)
            large.write_bytes(b"x" * 800)
            film.expect_within("Film", [small], 1000)
            message = self.assertMissed("Film", film.expect_within, [small, large], 1000)
            self.assertIn("1100", message)


    def test_a_single_light_frame_in_a_dark_film_is_a_flash(self):
        dark = [36.0] * 20
        self.assertEqual(film.flashed_frames(dark), [])
        self.assertEqual(film.flashed_frames(dark[:9] + [133.0] + dark[10:]), [(9, 133.0)])
        self.assertEqual(film.flashed_frames(dark[:9] + [60.0] + dark[10:]), [(9, 60.0)])

    def test_a_scene_that_fades_in_or_changes_at_once_is_not_a_flash(self):
        fade = [20.0 + 4 * step for step in range(20)]
        self.assertEqual(film.flashed_frames(fade), [])
        cut = [30.0] * 10 + [90.0] * 10
        self.assertEqual(film.flashed_frames(cut), [])

    def test_a_film_with_a_flash_is_refused(self):
        with tempfile.TemporaryDirectory() as directory:
            dark = Path(directory, "dark.mp4")
            flash = Path(directory, "flash.mp4")
            subprocess.run(["ffmpeg", "-v", "error", "-f", "lavfi", "-i",
                            "color=c=0x202020:size=64x64:rate=30:duration=2", "-c:v", "libx264",
                            "-pix_fmt", "yuv420p", str(dark)], check=True)
            subprocess.run(["ffmpeg", "-v", "error", "-f", "lavfi", "-i",
                            "color=c=0x202020:size=64x64:rate=30:duration=2", "-vf",
                            "drawbox=w=iw:h=ih:color=white:t=fill:enable='eq(n,30)'", "-c:v",
                            "libx264", "-pix_fmt", "yuv420p", str(flash)], check=True)
            film.expect_no_flash("Film", dark)
            message = self.assertMissed("Film", film.expect_no_flash, flash)
            self.assertIn("1.00s", message)

    def test_the_films_files_together_stay_under_five_megabytes(self):
        self.assertEqual(set(film.BUDGET), {"omaweb.webm", "omaweb.mp4", "poster.webp"})
        self.assertLess(sum(film.BUDGET.values()), 5_000_000)


    def test_every_site_resolving_to_loopback_sets_the_stage(self):
        film.expect_sites_on_loopback("Sites", lambda host: "127.0.0.1")

    def test_a_site_the_hosts_file_lost_stops_the_film_before_it_starts(self):
        # A restarted container has a fresh hosts file, and every page would load as an error.
        def resolve(host):
            if host == "quillstack.test":
                raise OSError("Name or service not known")
            return "127.0.0.1"

        why = self.assertMissed("Sites", film.expect_sites_on_loopback, resolve)
        self.assertIn("quillstack.test", why)
        self.assertIn("/etc/hosts", why)

    def test_a_site_resolving_elsewhere_stops_the_film_before_it_starts(self):
        why = self.assertMissed("Sites", film.expect_sites_on_loopback,
                                lambda host: "192.0.2.7" if host == "kestrel.test" else "127.0.0.1")
        self.assertIn("kestrel.test", why)

    def test_a_missed_beat_fails_the_command_and_says_which(self):
        missed = film.BeatMissed("Sidebar", "/docs/api/ on quillstack.test did not widen")
        with tempfile.TemporaryDirectory() as directory, \
                mock.patch.object(film, "record", side_effect=missed), \
                mock.patch.object(sys, "argv", ["record_film.py", "record", "--out", directory]), \
                mock.patch("sys.stderr", new_callable=io.StringIO) as said:
            self.assertEqual(film.main(), 1)
        self.assertIn("the film was not made: Sidebar: /docs/api/", said.getvalue())


def ffmpeg_can_cut_the_film() -> bool:
    """Whether this machine's ffmpeg has every filter and encoder the film's edit asks for."""
    if not (shutil.which("ffmpeg") and shutil.which("ffprobe")):
        return False
    listed = "".join(subprocess.run(["ffmpeg", "-hide_banner", option],
                                    capture_output=True, text=True).stdout
                     for option in ("-filters", "-encoders"))
    names = {line.split()[1] for line in listed.splitlines() if len(line.split()) > 1}
    return {"drawtext", "zoompan", "xfade", "libx264", "libvpx-vp9", "libwebp"} <= names


class FilmFiles(unittest.TestCase):
    @unittest.skipUnless((ROOT / ".git").exists(), "not a Git checkout")
    def test_the_film_is_not_kept_in_the_repository(self):
        for name in ("omaweb.webm", "omaweb.mp4", "poster.webp"):
            for place in ("build/film", "website/assets/film"):
                path = f"{place}/{name}"
                with self.subTest(path=path):
                    ignored = subprocess.run(["git", "-C", str(ROOT), "check-ignore", "-q", path])
                    self.assertEqual(ignored.returncode, 0, f"Git would keep {path}")
                    tracked = subprocess.run(["git", "-C", str(ROOT), "ls-files", path],
                                             capture_output=True, text=True, check=True)
                    self.assertEqual(tracked.stdout, "")

    @unittest.skipUnless(ffmpeg_can_cut_the_film(), "this ffmpeg cannot cut the film")
    def test_a_recording_is_cut_into_the_film_its_poster_and_its_captions(self):
        with tempfile.TemporaryDirectory() as directory:
            out = Path(directory)
            # A stand-in for the raw recording: a dark field at the recording's size, as dark as
            # the film so that its flash check passes, cut into every beat, the Omnibar long
            # enough to hold the poster's frame.
            subprocess.run(["ffmpeg", "-v", "error", "-f", "lavfi", "-i",
                            "color=c=0x202020:size={}x{}:rate={}:duration=12".format(
                                *film.OUTPUT_MODE, film.FPS),
                            "-c:v", "libx264", "-preset", "ultrafast", str(out / "raw.mkv")],
                           check=True)
            marks, start = [], 0.0
            for beat in film.BEATS:
                length = 4.2 if beat.name == film.POSTER[0] else 1.4
                marks.append({"beat": beat.name, "start": start, "end": start + length})
                start += length
            (out / "marks.json").write_text(json.dumps(marks), encoding="utf-8")
            film.compose(out)
            for name, limit in film.BUDGET.items():
                with self.subTest(file=name):
                    self.assertGreater((out / name).stat().st_size, 0)
                    self.assertLessEqual((out / name).stat().st_size, limit)
            self.assertEqual((out / "poster.webp").read_bytes()[8:12], b"WEBP")
            captions = (out / "captions.vtt").read_text(encoding="utf-8")
            self.assertTrue(captions.startswith("WEBVTT"))
            self.assertEqual([line for line in captions.splitlines()
                              if line in {beat.caption for beat in film.BEATS}],
                             [beat.caption for beat in film.BEATS])
            duration = float(subprocess.run(
                ["ffprobe", "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0",
                 str(out / "omaweb.mp4")], capture_output=True, text=True, check=True).stdout)
            self.assertAlmostEqual(duration, start - film.FADE * (len(marks) - 1), delta=0.2)


if __name__ == "__main__":
    unittest.main()
