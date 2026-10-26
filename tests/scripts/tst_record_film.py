#!/usr/bin/env python3
"""The introductory film's fixture sites, their server and the beat checks, without a browser.

`scripts/record_film.py` records the real browser in a container or on the maintainer's desktop,
and a recording that went wrong still produces a video. What decides whether it went wrong is here:
the sites the film browses reach nothing real, the server answers each one under its own name and
says what was asked of it, and each beat check refuses the answer a broken beat would give. The
desktop mode is held to the commands it runs and to giving the desktop back, against a stand-in
for Hyprland, since a test must never move the reader's windows. The recording itself needs the
engine and a compositor and runs through `scripts/record_film.sh` or `record_film.py desktop`.
"""

from __future__ import annotations

import http.client
import io
import json
import os
import re
import shlex
import shutil
import signal
import subprocess
import sys
import tempfile
import time
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

    def test_every_site_serves_the_end_card_with_the_install_line(self):
        for host in ("fernwood.test", "quillstack.test"):
            status, kind, page = self.ask(host, film.END_CARD_PATH)
            self.assertEqual(status, 200)
            self.assertIn("text/html", kind)
            self.assertIn("curl -fsSL https://omaweb.app/install | sh", page)
            self.assertIn('src="/film/report.js"', page)

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


def shared_memory(megabytes, free):
    """`os.statvfs` of a `/dev/shm` this many megabytes in size with `free` of them free."""
    return os.statvfs_result((4096, 4096, megabytes * 256, free * 256, free * 256,
                              0, 0, 0, 0, 255))


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


def ffmpeg_can_read_brightness() -> bool:
    """Whether this machine's ffmpeg can make a short clip and read every frame's brightness."""
    if not shutil.which("ffmpeg"):
        return False
    listed = "".join(subprocess.run(["ffmpeg", "-hide_banner", option],
                                    capture_output=True, text=True).stdout
                     for option in ("-filters", "-encoders"))
    names = {line.split()[1] for line in listed.splitlines() if len(line.split()) > 1}
    return {"signalstats", "drawbox", "libx264"} <= names


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

    def test_link_hints_are_a_page_that_reported_its_hints_on_show(self):
        reports = [{"host": "quillstack.test", "path": "/docs/tracing/", "hints": 0},
                   {"host": "quillstack.test", "path": "/docs/tracing/", "hints": 14}]
        film.expect_hints("Hints", reports, "quillstack.test")

    def test_hints_that_came_up_and_went_miss_the_hints_beat(self):
        reports = [{"host": "quillstack.test", "path": "/docs/tracing/", "hints": 14},
                   {"host": "quillstack.test", "path": "/docs/tracing/", "hints": 0}]
        self.assertMissed("Hints", film.expect_hints, reports, "quillstack.test")

    def test_a_page_that_never_showed_its_hints_misses_the_hints_beat(self):
        reports = [{"host": "quillstack.test", "path": "/docs/tracing/", "hints": 0},
                   {"host": "halyard.test", "path": "/", "hints": 9}]
        self.assertMissed("Hints", film.expect_hints, reports, "quillstack.test")

    def test_a_held_space_is_pointed_at_the_middle_of_its_square(self):
        held = {"ok": True, "x": 40, "y": 860, "width": 18, "height": 28,
                "windowWidth": 1600, "windowHeight": 900}
        self.assertEqual(film.hover_point("Space switch", held), (49 / 1600, 874 / 900))

    def test_a_space_the_window_could_not_hold_misses_the_beat(self):
        self.assertMissed("Space switch", film.hover_point,
                          {"ok": False, "code": "not-found", "error": "no Space"})

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

    def test_the_agents_row_names_each_new_act(self):
        film.expect_acts_on_the_row("Agent", [f"{film.AGENT} · looked at the page",
                                              f"{film.AGENT} · filled \"Amount\""])
        message = self.assertMissed("Agent", film.expect_acts_on_the_row,
                                    [f"{film.AGENT} is driving"])
        self.assertIn("last act", message)
        same = f"{film.AGENT} · looked at the page"
        message = self.assertMissed("Agent", film.expect_acts_on_the_row, [same, same])
        self.assertIn("did not change", message)

    def test_stop_hands_the_agents_tab_back(self):
        film.expect_handed_back("Agent", subprocess.CompletedProcess(
            ["omaweb", "film-row"], 1, "", 'No Agent is attached to tab "t1".\n'))
        message = self.assertMissed("Agent", film.expect_handed_back, subprocess.CompletedProcess(
            ["omaweb", "film-row"], 0, "{}", ""))
        self.assertIn("still attached", message)
        self.assertMissed("Agent", film.expect_handed_back, subprocess.CompletedProcess(
            ["omaweb", "film-row"], 1, "", "Omaweb has no verb \"film-row\".\n"))

    def test_a_held_stop_is_pointed_at_its_middle(self):
        held = {"ok": True, "x": 150, "y": 90, "width": 50, "height": 24,
                "windowWidth": 1600, "windowHeight": 900, "line": f"{film.AGENT} · sent"}
        self.assertEqual(film.hover_point("Agent", held), (175 / 1600, 102 / 900))

    # The window's answer to `film-site` with Site information open at the blocked list, in the
    # film window's logical pixels.
    SITE = {"ok": True, "open": True, "detail": "blocked", "windowWidth": 1600,
            "windowHeight": 900, "lock": {"x": 16, "y": 40, "width": 18, "height": 18},
            "card": {"x": 8, "y": 64, "width": 360, "height": 420},
            "listed": ["adsprout.test/serve.js?slot=top", "pixelmint.test/pixel.gif",
                       "adsprout.test/serve.js?slot=side"]}

    def test_the_blocked_list_on_show_names_the_ad_network_and_the_tracker(self):
        film.expect_blocked_list("Blocking", self.SITE, film.AD_HOSTS)

    def test_site_information_that_never_opened_misses_the_blocking_beat(self):
        with self.assertRaisesRegex(film.BeatMissed, "not open"):
            film.expect_blocked_list("Blocking", dict(self.SITE, open=False), film.AD_HOSTS)

    def test_site_information_left_at_its_top_misses_the_blocking_beat(self):
        with self.assertRaisesRegex(film.BeatMissed, "at its top"):
            film.expect_blocked_list("Blocking", dict(self.SITE, detail="", listed=[]),
                                     film.AD_HOSTS)

    def test_a_blocked_list_without_the_tracker_misses_the_blocking_beat(self):
        listed = ["adsprout.test/serve.js?slot=top"]
        with self.assertRaisesRegex(film.BeatMissed, "pixelmint.test"):
            film.expect_blocked_list("Blocking", dict(self.SITE, listed=listed), film.AD_HOSTS)

    def test_the_camera_frames_the_lock_and_the_card_it_opened(self):
        left, top, wide, high = film.framing(self.SITE)
        self.assertEqual(left, 0.0)
        self.assertAlmostEqual(top, 40 / 900 - 0.03)
        self.assertAlmostEqual(left + wide, 368 / 1600 + 0.03)
        self.assertAlmostEqual(top + high, 484 / 900 + 0.03)

    def test_the_camera_follows_a_focus_timed_as_the_beat_played(self):
        whole = film.camera([(0.0, None)])
        closer = film.camera([(0.0, None), (9.5, film.ROW)])
        self.assertNotEqual(whole, closer)
        self.assertIn("9.500", closer)
        self.assertNotIn("9.500", whole)

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

    @unittest.skipUnless(ffmpeg_can_read_brightness(), "this ffmpeg cannot read brightness")
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

    def test_room_in_shared_memory_sets_the_stage(self):
        film.expect_shared_memory("Stage", lambda path: shared_memory(1024, free=1024))
        # Less than `record_film.sh` asks for, and still twice what the film was seen to use.
        film.expect_shared_memory("Stage", lambda path: shared_memory(512, free=512))

    def test_dockers_default_shared_memory_stops_the_film_before_it_starts(self):
        # In 64 MB the renderer of the fourth tab the Stage loads, the magazine, was killed (#672).
        why = self.assertMissed("Stage", film.expect_shared_memory,
                                lambda path: shared_memory(64, free=64))
        self.assertIn("/dev/shm", why)
        self.assertIn("64 MB", why)
        self.assertIn("--shm-size", why)

    def test_shared_memory_another_run_has_filled_stops_the_film(self):
        why = self.assertMissed("Stage", film.expect_shared_memory,
                                lambda path: shared_memory(1024, free=100))
        self.assertIn("100 MB", why)

    def test_a_machine_without_dev_shm_is_not_stopped(self):
        def statvfs(path):
            raise FileNotFoundError(path)

        film.expect_shared_memory("Stage", statvfs)

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

    def test_a_beat_with_commands_shows_its_caption_and_then_each_command(self):
        marks = [
            {"beat": "Theme", "start": 0.0, "end": 5.0},
            {"beat": "Agent", "start": 5.0, "end": 13.0,
             "lines": [{"at": 1.8, "text": "$ omaweb space new Invoices"},
                       {"at": 3.2, "text": "$ omaweb look"}]},
            {"beat": "End", "start": 13.0, "end": 15.5},
        ]
        cues = film.cues(marks)
        position = 5.0 - film.FADE

        def at(seconds):
            return round(seconds, 3)

        self.assertEqual(
            [(cue.start, cue.end, cue.text, cue.mono) for cue in cues],
            [(0.3, at(5.0 - film.FADE), "Your Omarchy theme reaches the whole browser", False),
             (at(position + 0.3), at(position + 1.8), "An Agent works in a Space of its own",
              False),
             (at(position + 1.8), at(position + 3.2), "$ omaweb space new Invoices", True),
             (at(position + 3.2), at(position + 8.0 - film.FADE), "$ omaweb look", True)],
        )

    def test_the_pointer_glides_between_the_places_it_was_held(self):
        entry = {"beat": "Space switch", "start": 10.0, "end": 16.0,
                 "pointer": [{"at": 1.5, "x": 0.5, "y": 0.5}, {"at": 2.6, "x": 0.25, "y": 1.0}]}
        overlay = film.pointer(entry)
        self.assertIn("enable='between(t,1.500,6.000)'", overlay)
        self.assertIn(f"{0.5 * film.OUTPUT_MODE[0]:.5f}", overlay)
        self.assertIn(f"{film.OUTPUT_MODE[1]:.5f}", overlay)
        self.assertIsNone(film.pointer({"beat": "Theme", "start": 0.0, "end": 5.0}))

    @unittest.skipUnless(ffmpeg_can_cut_the_film(), "this ffmpeg cannot cut the film")
    def test_a_recording_is_cut_into_the_film_its_poster_and_its_captions(self):
        with tempfile.TemporaryDirectory() as directory:
            out = Path(directory)
            # A stand-in for the raw recording: a dark field at the recording's size, as dark as
            # the film so that its flash check passes, cut into every beat, the Omnibar long
            # enough to hold the poster's frame.
            # Written as wf-recorder writes it: limited-range values, labelled full range.
            subprocess.run(["ffmpeg", "-v", "error", "-f", "lavfi", "-i",
                            "color=c=0x202020:size={}x{}:rate={}:duration=16".format(
                                *film.OUTPUT_MODE, film.FPS),
                            "-c:v", "libx264", "-preset", "ultrafast", "-pix_fmt", "yuv420p",
                            "-bsf:v", "h264_metadata=video_full_range_flag=1",
                            str(out / "raw.mkv")],
                           check=True)
            marks, start = [], 0.0
            for beat in film.BEATS:
                length = 4.2 if beat.name == film.POSTER[0] else 1.4
                marks.append({"beat": beat.name, "start": start, "end": start + length})
                start += length
            # The pointer the Space switch draws, and a command the Agent runs, quotes and all.
            drawn = {entry["beat"]: entry for entry in marks}
            drawn["Space switch"]["pointer"] = [{"at": 0.2, "x": 0.3, "y": 0.8},
                                                {"at": 0.3, "x": 0.02, "y": 0.97}]
            drawn["Agent"]["lines"] = [{"at": 0.8, "text": """$ omaweb do 'fill 3 "A: 50%"'"""}]
            (out / "marks.json").write_text(json.dumps(marks), encoding="utf-8")
            film.compose(out)
            for name, limit in film.BUDGET.items():
                with self.subTest(file=name):
                    self.assertGreater((out / name).stat().st_size, 0)
                    self.assertLessEqual((out / name).stat().st_size, limit)
            self.assertEqual((out / "poster.webp").read_bytes()[8:12], b"WEBP")
            # Labelled as what it holds, so a browser shows the dark as dark, not grey.
            for name in ("omaweb.webm", "omaweb.mp4"):
                tags = subprocess.run(
                    ["ffprobe", "-v", "error", "-select_streams", "v", "-show_entries",
                     "stream=color_range,color_space,color_primaries,color_transfer",
                     "-of", "csv=p=0", str(out / name)],
                    capture_output=True, text=True, check=True).stdout.strip()
                # The field's grey, 0x20, is 43 in limited range, and stays 43 rather than being
                # squeezed again.
                level = film.frame_brightness(out / name)[0]
                with self.subTest(file=name):
                    self.assertEqual(sorted(tags.split(",")), ["bt709", "bt709", "bt709", "tv"])
                    self.assertAlmostEqual(level, 43, delta=3)
            captions = (out / "captions.vtt").read_text(encoding="utf-8")
            self.assertTrue(captions.startswith("WEBVTT"))
            captioned = [beat.caption for beat in film.BEATS if beat.caption]
            self.assertEqual([line for line in captions.splitlines() if line in captioned],
                             captioned)
            self.assertIn("""$ omaweb do 'fill 3 "A: 50%"'""", captions)
            duration = float(subprocess.run(
                ["ffprobe", "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0",
                 str(out / "omaweb.mp4")], capture_output=True, text=True, check=True).stdout)
            self.assertAlmostEqual(duration, start - film.FADE * (len(marks) - 1), delta=0.2)



# The XPS's monitor as `hyprctl monitors -j` describes it: 3840 by 2160 at a scale of 1.5, so 2560
# by 1440 logical pixels, with Omarchy's bar keeping 26 at the top.
MONITOR = {"name": "DP-1", "x": 0, "y": 0, "width": 3840, "height": 2160, "scale": 1.5,
           "transform": 0, "reserved": [0, 26, 0, 0], "focused": True,
           "activeWorkspace": {"id": 2, "name": "2"}}


class DesktopWindow(unittest.TestCase):
    def test_the_window_is_the_films_frame_in_the_middle_of_the_free_monitor(self):
        self.assertEqual(film.window_frame(MONITOR), (480, 283, 1600, 900))

    def test_a_monitor_to_the_right_places_the_window_on_itself(self):
        right = dict(MONITOR, x=2560, y=0, width=2560, height=1440, scale=1.0,
                     reserved=[0, 0, 0, 0])
        self.assertEqual(film.window_frame(right), (3040, 270, 1600, 900))

    def test_a_turned_monitor_is_measured_as_it_stands(self):
        turned = dict(MONITOR, width=2160, height=3840, scale=1.0, transform=1,
                      reserved=[0, 0, 0, 0])
        self.assertEqual(film.window_frame(turned), (1120, 630, 1600, 900))

    def test_a_monitor_without_room_for_the_frame_stops_the_film(self):
        small = dict(MONITOR, width=2880, height=1800, scale=2.0)
        with self.assertRaisesRegex(film.BeatMissed, "1440 by 874"):
            film.window_frame(small)

    def test_the_pointer_is_parked_off_the_window_on_the_same_monitor(self):
        frame = film.window_frame(MONITOR)
        self.assertEqual(film.parking(MONITOR, frame), (2559, 1439))

    def test_a_window_over_the_bottom_corners_parks_the_pointer_on_the_bar_side(self):
        # 1600 by 926 logical pixels: the window fills all the bar leaves.
        snug = dict(MONITOR, width=2400, height=1389, reserved=[0, 26, 0, 0])
        self.assertEqual(film.parking(snug, film.window_frame(snug)), (1599, 0))

    def test_a_window_over_the_whole_monitor_leaves_nowhere_to_park(self):
        whole = dict(MONITOR, width=2400, height=1350, reserved=[0, 0, 0, 0])
        with self.assertRaisesRegex(film.BeatMissed, "covers all"):
            film.parking(whole, film.window_frame(whole))



class FakeDesktop:
    """The commands a `Desktop` runs, answered as the XPS's Hyprland and Omarchy shell answer them.

    A query it knows is answered; anything else is an action, which is kept, and fails or is
    interrupted when a test says so.
    """

    def __init__(self, dnd="off", workspaces=None, notifier="omarchy-shell"):
        self.answers = {
            ("hyprctl", "monitors", "-j"): json.dumps([MONITOR]),
            ("hyprctl", "activeworkspace", "-j"): json.dumps({"id": 2, "name": "2"}),
            ("hyprctl", "workspaces", "-j"): json.dumps(
                workspaces or [{"id": 1, "name": "1", "windows": 1}]),
            ("hyprctl", "cursorpos", "-j"): json.dumps({"x": 1280, "y": 730}),
            ("omarchy-shell", "notifications", "dndState"): dnd + "\n",
            ("makoctl", "mode"): "default\n",
        }
        self.notifier = notifier
        # What `hyprctl clients -j` answers each time it is asked, the last one from then on.
        self.clients = [[]]
        self.acted = []
        self.interrupt_at = None
        self.failing = set()
        self.signals_while_acting = []

    def which(self, program):
        return f"/usr/bin/{program}" if program == self.notifier else None

    def run(self, command, **_):
        command = tuple(command)
        if command == ("hyprctl", "clients", "-j"):
            answer = self.clients.pop(0) if len(self.clients) > 1 else self.clients[0]
            return subprocess.CompletedProcess(command, 0, json.dumps(answer), "")
        if command in self.answers:
            return subprocess.CompletedProcess(command, 0, self.answers[command], "")
        self.acted.append(list(command))
        self.signals_while_acting.append(signal.getsignal(signal.SIGINT))
        if self.interrupt_at is not None and len(self.acted) == self.interrupt_at:
            raise KeyboardInterrupt
        if command in self.failing:
            raise subprocess.CalledProcessError(1, command)
        return subprocess.CompletedProcess(command, 0, "ok\n", "")


TAKE = [
    ["omarchy-shell", "notifications", "setDnd", "on"],
    ["omarchy-shell", "-q", "omarchy.indicators", "refresh"],
    ["hyprctl", "eval", "hl.dispatch(hl.dsp.cursor.move({ x = 2559, y = 1439 }))"],
    ["hyprctl", "dispatch", 'hl.dsp.focus({ workspace = "name:omaweb-film" })'],
]
GIVE_BACK = [
    ["hyprctl", "dispatch", 'hl.dsp.focus({ workspace = "2" })'],
    ["hyprctl", "eval", "hl.dispatch(hl.dsp.cursor.move({ x = 1280, y = 730 }))"],
    ["omarchy-shell", "notifications", "setDnd", "off"],
    ["omarchy-shell", "-q", "omarchy.indicators", "refresh"],
]


class DesktopTaken(unittest.TestCase):
    def desktop(self, fake):
        desktop = film.Desktop(run=fake.run, which=fake.which)
        desktop.survey()
        return desktop

    def test_a_run_holds_notifications_parks_the_pointer_and_takes_a_workspace_of_its_own(self):
        fake = FakeDesktop()
        with self.desktop(fake).taken():
            self.assertEqual(fake.acted, TAKE)
        self.assertEqual(fake.acted, TAKE + GIVE_BACK)

    def test_a_run_stopped_with_ctrl_c_puts_everything_back(self):
        fake = FakeDesktop()
        with self.assertRaises(KeyboardInterrupt), self.desktop(fake).taken():
            raise KeyboardInterrupt
        self.assertEqual(fake.acted, TAKE + GIVE_BACK)

    def test_a_run_that_fails_puts_everything_back(self):
        fake = FakeDesktop()
        with self.assertRaises(film.BeatMissed), self.desktop(fake).taken():
            raise film.BeatMissed("Theme", "the chrome kept its colour")
        self.assertEqual(fake.acted, TAKE + GIVE_BACK)

    def test_a_run_terminated_puts_everything_back(self):
        fake = FakeDesktop()
        with self.assertRaises(KeyboardInterrupt), self.desktop(fake).taken():
            os.kill(os.getpid(), signal.SIGTERM)
            time.sleep(1)
        self.assertEqual(fake.acted, TAKE + GIVE_BACK)

    def test_ctrl_c_while_the_desktop_is_being_taken_puts_back_what_was_taken(self):
        fake = FakeDesktop()
        fake.interrupt_at = 3
        with self.assertRaises(KeyboardInterrupt), self.desktop(fake).taken():
            self.fail("the run went on after Ctrl+C")
        self.assertEqual(fake.acted, TAKE[:3] + GIVE_BACK[1:])

    def test_a_second_ctrl_c_cannot_stop_the_desktop_being_put_back(self):
        fake = FakeDesktop()
        before = signal.getsignal(signal.SIGINT)
        with self.assertRaises(KeyboardInterrupt), self.desktop(fake).taken():
            raise KeyboardInterrupt
        self.assertEqual(fake.signals_while_acting[len(TAKE):],
                         [signal.SIG_IGN] * len(GIVE_BACK))
        self.assertIs(signal.getsignal(signal.SIGINT), before)

    def test_what_cannot_be_put_back_does_not_keep_the_rest_from_it(self):
        fake = FakeDesktop()
        fake.failing.add(tuple(GIVE_BACK[0]))
        desktop = self.desktop(fake)
        with mock.patch("sys.stdout", new_callable=io.StringIO) as said, desktop.taken():
            pass
        self.assertEqual(fake.acted, TAKE + GIVE_BACK)
        self.assertIn("could not put back", said.getvalue())
        self.assertEqual(desktop.not_put_back, [shlex.join(GIVE_BACK[0])])

    def test_do_not_disturb_already_on_stays_on(self):
        fake = FakeDesktop(dnd="on")
        with self.desktop(fake).taken():
            pass
        self.assertIn(["omarchy-shell", "notifications", "setDnd", "on"], fake.acted[len(TAKE):])

    def test_mako_modes_are_held_and_restored_where_there_is_no_omarchy_shell(self):
        fake = FakeDesktop(notifier="makoctl")
        with self.desktop(fake).taken():
            pass
        self.assertEqual(fake.acted[0], ["makoctl", "mode", "-a", "do-not-disturb"])
        self.assertEqual(fake.acted[-1], ["makoctl", "mode", "-s", "default"])

    def test_a_desktop_with_nothing_to_hold_notifications_stops_the_film(self):
        fake = FakeDesktop(notifier=None)
        with self.assertRaisesRegex(film.BeatMissed, "notifications"):
            self.desktop(fake)
        self.assertEqual(fake.acted, [])

    def test_a_film_workspace_left_with_windows_stops_the_film(self):
        fake = FakeDesktop(workspaces=[{"id": 7, "name": "omaweb-film", "windows": 1}])
        with self.assertRaisesRegex(film.BeatMissed, "omaweb-film"):
            self.desktop(fake)
        self.assertEqual(fake.acted, [])



# The reader's own Omaweb, which has the same class as the film's and must never be touched.
READERS_WINDOW = {"address": "0x5566aa", "pid": 2236555, "class": "omaweb", "floating": False,
                  "at": [11, 32], "size": [2538, 1397]}
FILMS_WINDOW = {"address": "0x5566bb", "pid": 4100, "class": "omaweb", "floating": False,
                "at": [11, 32], "size": [1269, 1397]}
PLACED = dict(FILMS_WINDOW, floating=True, at=[480, 283], size=[1600, 900])
# The film's browser was started in a session of its own, which its window's process is in.
SESSIONS = {2236555: 1700, 4100: 4000}

PLACE = [
    ["hyprctl", "dispatch", 'hl.dsp.window.move({ window = "address:0x5566bb", '
                            'workspace = "name:omaweb-film", follow = false })'],
    ["hyprctl", "dispatch", 'hl.dsp.window.float({ window = "address:0x5566bb", '
                            'action = "toggle" })'],
    ["hyprctl", "dispatch", 'hl.dsp.window.resize({ window = "address:0x5566bb", '
                            'x = 1600, y = 900 })'],
    ["hyprctl", "dispatch", 'hl.dsp.window.move({ window = "address:0x5566bb", '
                            'x = 480, y = 283 })'],
    ["hyprctl", "dispatch", 'hl.dsp.focus({ window = "address:0x5566bb" })'],
]


class DesktopPlacement(unittest.TestCase):
    def place(self, fake, **options):
        desktop = film.Desktop(run=fake.run, which=fake.which, **options)
        desktop.survey()
        return desktop.place(4000, getpgid=SESSIONS.__getitem__, timeout=1)

    def test_the_films_window_is_found_by_its_session_and_put_in_the_films_frame(self):
        fake = FakeDesktop()
        fake.clients = [[READERS_WINDOW], [READERS_WINDOW, FILMS_WINDOW],
                        [READERS_WINDOW, PLACED]]
        self.assertEqual(self.place(fake), (480, 283, 1600, 900))
        self.assertEqual(fake.acted, PLACE)

    def test_the_window_is_read_where_it_came_to_rest_not_on_its_way(self):
        fake = FakeDesktop()
        fake.clients = [[FILMS_WINDOW], [dict(PLACED, at=[300, 200])],
                        [dict(PLACED, at=[420, 260])], [PLACED]]
        self.assertEqual(self.place(fake), (480, 283, 1600, 900))

    def test_a_window_that_floats_already_is_not_tiled_again(self):
        fake = FakeDesktop()
        fake.clients = [[dict(FILMS_WINDOW, floating=True)], [PLACED]]
        self.place(fake)
        self.assertEqual(fake.acted, PLACE[:1] + PLACE[2:])

    def test_a_window_hyprland_would_not_size_stops_the_film(self):
        fake = FakeDesktop()
        fake.clients = [[FILMS_WINDOW], [dict(PLACED, size=[1600, 880])]]
        with self.assertRaisesRegex(film.BeatMissed, "1600 by 880"):
            self.place(fake)

    def test_a_browser_that_never_opens_a_window_stops_the_film(self):
        fake = FakeDesktop()
        fake.clients = [[READERS_WINDOW]]
        with self.assertRaisesRegex(film.BeatMissed, "window"):
            self.place(fake)
        self.assertEqual(fake.acted, [])

    def test_a_dry_run_names_what_it_would_do_to_the_window_and_does_none_of_it(self):
        fake = FakeDesktop()
        with mock.patch("sys.stdout", new_callable=io.StringIO) as said:
            self.assertEqual(self.place(fake, dry_run=True), (480, 283, 1600, 900))
        self.assertEqual(fake.acted, [])
        self.assertIn("would run: hyprctl dispatch 'hl.dsp.window.resize({ window = "
                      '"address:<film window>", x = 1600, y = 900 })\'', said.getvalue())



class DesktopBrowser(unittest.TestCase):
    # The maintainer's session as the recording inherits it.
    SESSION = {"HOME": "/home/reader", "XDG_CONFIG_HOME": "/home/reader/.config",
               "XDG_DATA_HOME": "/home/reader/.local/share", "XDG_RUNTIME_DIR": "/run/user/1000",
               "WAYLAND_DISPLAY": "wayland-1", "QTWEBENGINE_CHROMIUM_FLAGS": "--enable-x",
               "QT_SCALE_FACTOR": "2", "PATH": "/usr/bin"}

    def environment(self):
        return film.desktop_environment(self.SESSION, Path("/tmp/omaweb-film-1"), 43210)

    def test_the_films_browser_keeps_nothing_in_the_readers_home(self):
        for name, value in self.environment().items():
            with self.subTest(name=name):
                self.assertNotIn("/home/reader", value)

    def test_the_films_browser_answers_on_a_socket_of_its_own(self):
        environment = self.environment()
        self.assertEqual(environment["OMAWEB_CONTROL_SOCKET"], "/tmp/omaweb-film-1/control.sock")
        self.assertEqual(environment["OMAWEB_DATA_ROOT"], "/tmp/omaweb-film-1/data")
        self.assertEqual(environment["OMAWEB_CONFIG_ROOT"], "/tmp/omaweb-film-1/config")

    def test_the_films_sites_are_reached_on_the_server_and_nothing_else_is_reached(self):
        self.assertEqual(self.environment()["QTWEBENGINE_CHROMIUM_FLAGS"],
                         '"--host-resolver-rules=MAP *.test 127.0.0.1:43210, MAP * ~NOTFOUND"')

    def test_the_films_browser_logs_to_the_film_not_the_journal(self):
        self.assertEqual(self.environment()["QT_FORCE_STDERR_LOGGING"], "1")

    def test_the_films_browser_draws_on_the_readers_wayland_session_at_its_own_scale(self):
        environment = self.environment()
        self.assertEqual(environment["WAYLAND_DISPLAY"], "wayland-1")
        self.assertEqual(environment["XDG_RUNTIME_DIR"], "/run/user/1000")
        self.assertEqual(environment["QT_QPA_PLATFORM"], "wayland")
        self.assertNotIn("QT_SCALE_FACTOR", environment)



class DesktopRecording(unittest.TestCase):
    FRAME = (480, 283, 1600, 900)

    def test_under_cage_the_whole_output_is_recorded_as_it_was(self):
        self.assertEqual(
            film.recorder_command(Path("/film/raw.mkv")),
            ["wf-recorder", "-y", "-r", "30", "-c", "libx264", "-p", "preset=ultrafast",
             "-p", "crf=12", "-x", "yuv420p", "-f", "/film/raw.mkv"])
        self.assertEqual(film.frame_command(), ["grim", "-t", "ppm", "-"])

    def test_on_the_desktop_only_the_films_window_is_recorded(self):
        command = film.recorder_command(Path("/film/raw.mkv"), self.FRAME)
        self.assertEqual(command[command.index("-g") + 1], "480,283 1600x900")
        self.assertEqual(film.frame_command(self.FRAME),
                         ["grim", "-g", "480,283 1600x900", "-t", "ppm", "-"])

    def test_the_chrome_is_sampled_at_the_same_place_in_a_frame_of_any_size(self):
        def ppm(width, height, at):
            pixels = bytearray(width * height * 3)
            start = (at[1] * width + at[0]) * 3
            pixels[start:start + 3] = bytes((10, 20, 30))
            return f"P6 {width} {height} 255\n".encode() + bytes(pixels)

        # A hundredth of the way in and half way down: 24, 675 of the cage's 2400 by 1350.
        self.assertEqual(film.chrome_sample(ppm(2400, 1350, (24, 675))), (10, 20, 30))
        self.assertEqual(film.chrome_sample(ppm(1600, 900, (16, 450))), (10, 20, 30))

    def test_a_recording_at_the_films_size_is_cut_as_it_is(self):
        self.assertEqual(film.fitted(film.OUTPUT_MODE), "")

    def test_a_recording_from_a_monitor_of_another_scale_is_brought_to_the_films_size(self):
        self.assertEqual(film.fitted((3200, 1800)), ",scale=2400:1350:flags=lanczos")



class Commands:
    """Every command the build or the preflight runs, answered with the first answer matched."""

    def __init__(self, *answers):
        self.answers = list(answers)
        self.ran = []

    def __call__(self, command, **options):
        self.ran.append((list(command), options))
        for start, code, said in self.answers:
            if list(command[:len(start)]) == start:
                return subprocess.CompletedProcess(command, code, said, "")
        return subprocess.CompletedProcess(command, 0, "", "")


class DesktopBuild(unittest.TestCase):
    ENGINE = ["Qt6WebEngineCore", "Qt6WebEngineQuick"]

    def build(self, run, **options):
        with tempfile.TemporaryDirectory() as engine:
            for name in self.ENGINE:
                (Path(engine) / name).mkdir()
            return film.build_for_desktop(run=run, engine=Path(engine), **options), engine

    def test_the_film_build_is_of_the_commit_on_omawebs_engine_with_two_jobs(self):
        run = Commands()
        browser, engine = self.build(run)
        self.assertEqual(browser, film.ROOT / "build" / "film-desktop" / "omaweb-browser")
        commands = [command for command, _ in run.ran]
        configure = next(command for command in commands if command[:2] == ["cmake", "--preset"])
        self.assertIn("-DOMAWEB_FILM_HOOKS=ON", configure)
        self.assertIn(f"-DQt6WebEngineCore_DIR:PATH={engine}/Qt6WebEngineCore", configure)
        self.assertIn(["scripts/check_omaweb_engine.sh", str(film.ROOT / "build" / "film-desktop")],
                      commands)
        built = next(options for command, options in run.ran if command[:2] == ["cmake", "--build"])
        self.assertEqual(built["env"]["CMAKE_BUILD_PARALLEL_LEVEL"], "2")

    def test_a_checkout_with_changes_is_not_filmed(self):
        run = Commands((["git", "status"], 0, " M src/main.cpp\n"))
        with self.assertRaisesRegex(film.BeatMissed, "commit"):
            self.build(run)
        self.assertFalse([command for command, _ in run.ran if command[0] == "cmake"])

    def test_a_dry_run_names_the_build_and_builds_nothing(self):
        run = Commands()
        with mock.patch("sys.stdout", new_callable=io.StringIO) as said:
            self.build(run, dry_run=True)
        self.assertEqual([command[0] for command, _ in run.ran], ["git"])
        self.assertIn("would run: cmake --build", said.getvalue())

    def test_what_the_desktop_lacks_for_the_film_is_named_with_its_install_line(self):
        run = Commands((["pacman", "-T"], 127, "wf-recorder\ninter-font\n"))
        with self.assertRaisesRegex(film.BeatMissed,
                                    "sudo pacman -S --needed wf-recorder inter-font"):
            film.expect_desktop_packages(run=run)

    def test_a_desktop_with_everything_the_film_needs_passes(self):
        film.expect_desktop_packages(run=Commands((["pacman", "-T"], 0, "")))


class DesktopRun(unittest.TestCase):
    def setUp(self):
        self.fake = FakeDesktop()
        self.desktop = film.Desktop(run=self.fake.run, which=self.fake.which)
        self.out = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.out)
        for patch in (mock.patch.object(film, "build_for_desktop",
                                        return_value=Path("/build/omaweb-browser")),
                      mock.patch.object(film, "expect_desktop_packages"),
                      mock.patch.object(film, "compose")):
            patch.start()
            self.addCleanup(patch.stop)
        self.said = io.StringIO()
        patch = mock.patch("sys.stdout", self.said)
        patch.start()
        self.addCleanup(patch.stop)

    def test_the_run_says_it_takes_the_keyboard_and_waits_before_it_changes_anything(self):
        def at_the_prompt(_prompt=""):
            self.assertEqual(self.fake.acted, [])
            self.assertIn("keyboard", self.said.getvalue())
            return ""

        with mock.patch("builtins.input", side_effect=at_the_prompt) as asked, \
                mock.patch.object(film, "record") as recorded:
            film.record_on_desktop(self.desktop, self.out)
        asked.assert_called_once()
        recorded.assert_called_once_with(Path("/build/omaweb-browser"), self.out, self.desktop)
        self.assertEqual(self.fake.acted, TAKE + GIVE_BACK)
        self.assertIn(str(self.out / "omaweb.webm"), self.said.getvalue())

    def test_ctrl_c_at_the_prompt_changes_nothing(self):
        with mock.patch("builtins.input", side_effect=KeyboardInterrupt), \
                mock.patch.object(film, "record") as recorded, \
                self.assertRaises(KeyboardInterrupt):
            film.record_on_desktop(self.desktop, self.out)
        recorded.assert_not_called()
        self.assertEqual(self.fake.acted, [])

    def test_a_film_stopped_with_ctrl_c_puts_the_desktop_back_and_is_not_composed(self):
        with mock.patch("builtins.input", return_value=""), \
                mock.patch.object(film, "record", side_effect=KeyboardInterrupt), \
                self.assertRaises(KeyboardInterrupt):
            film.record_on_desktop(self.desktop, self.out)
        self.assertEqual(self.fake.acted, TAKE + GIVE_BACK)
        film.compose.assert_not_called()

    def test_a_dry_run_prints_every_command_it_would_issue_and_issues_none(self):
        self.desktop.dry_run = True
        with mock.patch("builtins.input", side_effect=AssertionError("waited for a key")):
            film.record_on_desktop(self.desktop, self.out)
        self.assertEqual(self.fake.acted, [])
        printed = [line.removeprefix("would run: ") for line in self.said.getvalue().splitlines()
                   if line.startswith("would run: ")]
        expected = [shlex.join(command) for command in TAKE] + [
            shlex.join(command).replace("0x5566bb", "<film window>")
            for command in PLACE] + [
            shlex.join(film.recorder_command(self.out / "raw.mkv", (480, 283, 1600, 900)))] + [
            shlex.join(command) for command in GIVE_BACK]
        self.assertEqual(printed, expected)
        film.compose.assert_not_called()

    def test_a_stopped_film_says_the_desktop_is_as_it_was(self):
        with mock.patch.object(film, "Desktop", return_value=self.desktop), \
                mock.patch.object(film, "record_on_desktop", side_effect=KeyboardInterrupt), \
                mock.patch.object(sys, "argv", ["record_film.py", "desktop"]), \
                mock.patch("sys.stderr", new_callable=io.StringIO) as said:
            self.assertEqual(film.main(), 130)
        self.assertIn("stopped", said.getvalue())


if __name__ == "__main__":
    unittest.main()
