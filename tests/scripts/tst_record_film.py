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
import urllib.request
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

    def test_the_daypack_is_shown_in_its_photograph(self):
        _, _, page = self.ask("fernwood.test", "/products/canvas-daypack/")
        figure = page.split("<figure>")[1].split("</figure>")[0]
        self.assertRegex(figure, r'<img\s+src="/products/canvas-daypack/canvas-daypack.webp"'
                                 r'\s+alt="Canvas Daypack 22L in Moss"')
        self.assertNotIn("<svg", figure)
        status, kind, _ = self.ask("fernwood.test", "/products/canvas-daypack/canvas-daypack.webp")
        self.assertEqual((status, kind), (200, "image/webp"))

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

    def test_the_omnibar_beat_waits_for_kestrel_to_be_asked(self):
        server = mock.Mock()
        server.paths.side_effect = [[], ["/suggest?q=t"], ["/suggest?q=t", "/suggest?q=tra"]]
        with mock.patch.object(film.time, "sleep"):
            film.expect_asked("Omnibar", server, "kestrel.test", "/suggest?q=tra", timeout=5.0)
        self.assertEqual(server.paths.call_count, 3)

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
            "windowHeight": 900, "button": {"x": 16, "y": 40, "width": 18, "height": 18},
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

    def test_the_camera_frames_site_informations_button_and_card(self):
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
        # The window with the keyboard: the terminal the run was started from, until the film's.
        self.active = {"address": "0x5566cc", "class": "Alacritty", "title": "~/omaweb"}
        # What an action answers, where it is not Hyprland's "ok".
        self.replies = {}
        self.read_log = []
        self.acted = []
        self.interrupt_at = None
        self.failing = set()
        self.signals_while_acting = []

    def which(self, program):
        return f"/usr/bin/{program}" if program in (self.notifier, "omarchy-theme-set") else None

    def options(self, **overrides):
        """What a `Desktop` is made with against this stand-in: its commands, and an Omarchy of
        its own whose desktop wears Ethereal Nebula Silver and which has Retro 82 installed."""
        if not hasattr(self, "_omarchy"):
            self._omarchy = tempfile.TemporaryDirectory()
            root = Path(self._omarchy.name)
            for theme, colour in (("current/theme", "#050718"), ("themes/retro-82", "#031222")):
                (root / theme).mkdir(parents=True)
                (root / theme / "colors.toml").write_text(
                    f'mode = "dark"\ndark_background = "{colour}"\n', encoding="utf-8")
            (root / "current" / "theme.name").write_text("ethereal-nebula-silver\n")
            (root / "home").mkdir()
        root = Path(self._omarchy.name)
        return dict({"run": self.run, "which": self.which, "current": root / "current",
                     "themes": (root / "themes",), "session": {"HOME": str(root / "home")}},
                    **overrides)

    def run(self, command, **_):
        command = tuple(command)
        if command == ("hyprctl", "activewindow", "-j"):
            self.read_log.append(list(command))
            return subprocess.CompletedProcess(command, 0, json.dumps(self.active), "")
        if command == ("hyprctl", "clients", "-j"):
            answer = self.clients.pop(0) if len(self.clients) > 1 else self.clients[0]
            return subprocess.CompletedProcess(command, 0, json.dumps(answer), "")
        if command in self.answers:
            self.read_log.append(list(command))
            return subprocess.CompletedProcess(command, 0, self.answers[command], "")
        self.acted.append(list(command))
        self.signals_while_acting.append(signal.getsignal(signal.SIGINT))
        if self.interrupt_at is not None and len(self.acted) == self.interrupt_at:
            raise KeyboardInterrupt
        if command in self.failing:
            raise subprocess.CalledProcessError(1, command)
        return subprocess.CompletedProcess(command, 0, self.replies.get(command, "ok\n"), "")


# Hyprland asked for two things that change nothing, in the run's own forms, before it changes any.
PREFLIGHT = [
    ["hyprctl", "eval", "hl.dispatch(hl.dsp.cursor.move({ x = 1280, y = 730 }))"],
    ["hyprctl", "dispatch", 'hl.dsp.focus({ window = "address:0x5566cc" })'],
]
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
    def desktop(self, fake, **options):
        desktop = film.Desktop(**fake.options(**options))
        desktop.survey()
        return desktop

    def test_a_run_given_a_workspace_takes_that_one_and_comes_back(self):
        fake = FakeDesktop(workspaces=[{"id": 1, "name": "1", "windows": 1},
                                       {"id": 9, "name": "9", "windows": 0}])
        to_nine = ["hyprctl", "dispatch", 'hl.dsp.focus({ workspace = "9" })']
        with self.desktop(fake, workspace="9").taken():
            self.assertEqual(fake.acted, TAKE[:-1] + [to_nine])
        self.assertEqual(fake.acted, TAKE[:-1] + [to_nine] + GIVE_BACK)

    def test_a_workspace_given_by_name_is_taken_by_name(self):
        fake = FakeDesktop()
        desktop = self.desktop(fake, workspace="name:film")
        self.assertEqual((desktop.film_workspace, desktop.film_selector), ("film", "name:film"))

    def test_a_given_workspace_with_windows_of_the_readers_stops_the_film(self):
        fake = FakeDesktop(workspaces=[{"id": 9, "name": "9", "windows": 2}])
        with self.assertRaisesRegex(film.BeatMissed, "workspace 9 already holds 2 windows"):
            self.desktop(fake, workspace="9")
        self.assertEqual(fake.acted, [])

    def test_the_window_is_moved_to_the_given_workspace(self):
        fake = FakeDesktop()
        fake.clients = [[FILMS_WINDOW], [PLACED]]
        desktop = self.desktop(fake, workspace="9")
        desktop.place(4000, getpgid=SESSIONS.__getitem__)
        self.assertEqual(fake.acted[0], ["hyprctl", "dispatch",
                                         'hl.dsp.window.move({ window = "address:0x5566bb", '
                                         'workspace = "9", follow = false })'])

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

    def test_a_desktop_hyprctl_cannot_reach_stops_the_film_with_what_it_said(self):
        fake = FakeDesktop()

        def unreachable(command, **options):
            if command[0] == "hyprctl":
                raise subprocess.CalledProcessError(
                    4, command, "", "HYPRLAND_INSTANCE_SIGNATURE not set! (is hyprland running?)")
            return fake.run(command, **options)

        with self.assertRaisesRegex(film.BeatMissed, "hyprctl monitors -j.*is hyprland running"):
            film.Desktop(run=unreachable, which=fake.which).survey()
        self.assertEqual(fake.acted, [])

    def test_a_desktop_without_hyprctl_stops_the_film(self):
        def missing(command, **_):
            raise FileNotFoundError(2, "No such file or directory", command[0])

        with self.assertRaisesRegex(film.BeatMissed, "hyprctl"):
            film.Desktop(run=missing, which=FakeDesktop().which).survey()

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
        desktop = film.Desktop(**fake.options(**options))
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
        # The content blocker's cargo build, which bootstrap_content_blocker.sh runs, too.
        blocker = next(options for command, options in run.ran
                       if command == ["scripts/bootstrap_content_blocker.sh"])
        self.assertEqual(blocker["env"]["CARGO_BUILD_JOBS"], "2")

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
        self.desktop = film.Desktop(**self.fake.options())
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
            # The desktop is read after the key, as it stands when the run starts.
            self.assertEqual(self.fake.read_log, [])
            self.assertIn("keyboard", self.said.getvalue())
            return ""

        with mock.patch("builtins.input", side_effect=at_the_prompt) as asked, \
                mock.patch.object(film, "record") as recorded:
            film.record_on_desktop(self.desktop, self.out)
        asked.assert_called_once()
        recorded.assert_called_once_with(Path("/build/omaweb-browser"), self.out, self.desktop)
        self.assertEqual(self.fake.acted, PREFLIGHT + TAKE + GIVE_BACK)
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
        self.assertEqual(self.fake.acted, PREFLIGHT + TAKE + GIVE_BACK)
        film.compose.assert_not_called()

    def test_a_dry_run_prints_every_command_it_would_issue_and_issues_none(self):
        self.desktop.dry_run = True
        with mock.patch("builtins.input", side_effect=AssertionError("waited for a key")):
            film.record_on_desktop(self.desktop, self.out)
        self.assertEqual(self.fake.acted, [])
        printed = [line.removeprefix("would run: ") for line in self.said.getvalue().splitlines()
                   if line.startswith("would run: ")]
        expected = [shlex.join(command) for command in PREFLIGHT + TAKE] + [
            shlex.join(command).replace("0x5566bb", "<film window>")
            for command in PLACE] + [
            shlex.join(film.recorder_command(self.out / "raw.mkv", (480, 283, 1600, 900))),
            "omarchy-theme-set retro-82", "omarchy-theme-set ethereal-nebula-silver"] + [
            shlex.join(command) for command in GIVE_BACK]
        self.assertEqual(printed, expected)
        film.compose.assert_not_called()

    def test_the_command_line_names_the_theme_the_beat_changes_to(self):
        for arguments, named in (([], "retro-82"), (["--change-to", "gruvbox"], "gruvbox")):
            with mock.patch.object(film, "Desktop", return_value=self.desktop) as made, \
                    mock.patch.object(film, "record_on_desktop"), \
                    mock.patch.object(sys, "argv", ["record_film.py", "desktop", *arguments]):
                film.main()
            self.assertEqual(made.call_args.kwargs["change_to"], named)

    def test_a_stopped_film_says_the_desktop_is_as_it_was(self):
        with mock.patch.object(film, "Desktop", return_value=self.desktop), \
                mock.patch.object(film, "record_on_desktop", side_effect=KeyboardInterrupt), \
                mock.patch.object(sys, "argv", ["record_film.py", "desktop"]), \
                mock.patch("sys.stderr", new_callable=io.StringIO) as said:
            self.assertEqual(film.main(), 130)
        self.assertIn("stopped", said.getvalue())


class DesktopKeyboard(unittest.TestCase):
    """Each key goes to the film's window or nowhere: a pointer nudged onto another window, or a
    window that takes focus, would otherwise have the reader's own windows typed into."""

    def setUp(self):
        self.addCleanup(setattr, film, "BEFORE_KEYS", None)
        patch = mock.patch.object(film.time, "sleep")
        patch.start()
        self.addCleanup(patch.stop)

    def test_each_key_is_sent_only_after_the_film_is_seen_to_have_the_keyboard(self):
        order = []
        film.BEFORE_KEYS = lambda: order.append("asked")
        with mock.patch.object(film.subprocess, "run",
                               side_effect=lambda command, **_: order.append(command[0])):
            film.keys("Primary+l", "Return")
            film.type_text("tra")
        self.assertEqual(order, ["asked", "wtype", "asked", "wtype", "asked", "wtype"])

    def test_no_key_is_sent_once_another_window_has_the_keyboard(self):
        def elsewhere():
            raise film.BeatMissed("Desktop", "Alacritty has the keyboard")

        film.BEFORE_KEYS = elsewhere
        with mock.patch.object(film.subprocess, "run") as run:
            with self.assertRaises(film.BeatMissed):
                film.keys("Primary+l")
            with self.assertRaises(film.BeatMissed):
                film.type_text("tra")
        run.assert_not_called()

    def placed(self, fake):
        fake.clients = [[FILMS_WINDOW], [PLACED]]
        desktop = film.Desktop(**fake.options())
        desktop.survey()
        desktop.place(4000, getpgid=SESSIONS.__getitem__, timeout=1)
        return desktop

    def test_the_films_window_with_the_keyboard_lets_the_keys_through(self):
        fake = FakeDesktop()
        desktop = self.placed(fake)
        fake.active = {"address": "0x5566bb", "class": "omaweb", "title": "Omaweb"}
        desktop.expect_keyboard()

    def test_another_window_with_the_keyboard_stops_the_film_and_names_it(self):
        fake = FakeDesktop()
        desktop = self.placed(fake)
        fake.active = {"address": "0x5566aa", "class": "omaweb", "title": "Inbox"}
        with self.assertRaisesRegex(film.BeatMissed, "omaweb .Inbox. has the keyboard"):
            desktop.expect_keyboard()


class DesktopTheme(unittest.TestCase):
    """The desktop film wears the theme the desktop does, which its browser follows from Omarchy's
    state, and the Theme beat switches it and back. Its browser lets the desktop through the
    sidebar as the reader's own Omaweb does."""

    def setUp(self):
        scratch = tempfile.TemporaryDirectory()
        self.addCleanup(scratch.cleanup)
        self.directory = Path(scratch.name)

    def worn(self, name, colour):
        current = self.directory / "omarchy" / "current"
        (current / "theme").mkdir(parents=True)
        (current / "theme" / "colors.toml").write_text(
            f'mode = "dark"\ndark_background = "{colour}"\n'
            'hyprland_inactive_border = "rgba(252e56aa)"\n', encoding="utf-8")
        (current / "theme.name").write_text(name + "\n", encoding="utf-8")
        return current

    def staged(self, opening, **options):
        _, config = film.seed(self.directory / "film", opening, **options)
        return config

    def test_the_desktop_film_opens_in_the_theme_the_desktop_wears(self):
        theme = film.desktop_theme(self.worn("ethereal-nebula-silver", "#050718"))
        self.assertEqual((theme.name, theme.colours["dark_background"]),
                         ("ethereal-nebula-silver", "#050718"))

    def test_a_desktop_without_a_current_omarchy_theme_stops_the_film(self):
        with self.assertRaisesRegex(film.BeatMissed, "no current theme"):
            film.desktop_theme(self.directory / "omarchy" / "current")

    def test_the_desktops_browser_follows_the_desktops_theme_and_is_given_none(self):
        current = self.worn("ethereal-nebula-silver", "#050718")
        home = self.directory / "film" / "home"
        film.follow_desktop_theme(home, current)
        followed = home / ".local" / "state" / "omarchy" / "current" / "theme" / "colors.toml"
        self.assertIn("#050718", followed.read_text(encoding="utf-8"))
        config = self.staged(None)
        self.assertFalse((config / "theme.json").exists())

    def test_the_container_film_is_given_retro_82(self):
        config = self.staged(film.film_palette("retro-82"))
        drawn = json.loads((config / "theme.json").read_text(encoding="utf-8"))
        self.assertEqual(drawn["sidebar"], "#031222")

    def installed(self, **themes):
        """Omarchy's two theme directories, the reader's and the system's, holding `themes` as
        name=(where, dark_background)."""
        directories = (self.directory / "user-themes", self.directory / "system-themes")
        for name, (where, colour) in themes.items():
            theme = directories[where] / name
            theme.mkdir(parents=True)
            (theme / "colors.toml").write_text(
                f'mode = "dark"\ndark_background = "{colour}"\n', encoding="utf-8")
        return directories

    def test_the_theme_beat_goes_to_the_theme_named_and_back_to_the_desktops(self):
        mine = film.Theme("ethereal-nebula-silver", {"dark_background": "#050718"})
        named = film.Theme("gruvbox", {"dark_background": "#282828"})
        changes = [theme.name for theme in film.theme_changes(mine, named)]
        self.assertEqual(changes, ["ethereal-nebula-silver", "gruvbox", "ethereal-nebula-silver"])

    def test_without_a_theme_named_the_beat_changes_to_retro_82(self):
        directories = self.installed(**{"retro-82": (1, "#031222")})
        change = film.change_theme({"dark_background": "#3b2a40"}, directories=directories)
        self.assertEqual((change.name, change.colours["dark_background"]),
                         ("retro-82", "#031222"))

    def test_a_theme_named_is_read_from_the_readers_themes_before_the_systems(self):
        directories = self.installed(gruvbox=(1, "#1d2021"))
        (directories[0] / "gruvbox").mkdir(parents=True)
        (directories[0] / "gruvbox" / "colors.toml").write_text(
            'dark_background = "#282828"\n', encoding="utf-8")
        change = film.change_theme({"dark_background": "#050718"}, "gruvbox", directories)
        self.assertEqual(change.colours["dark_background"], "#282828")

    def test_a_theme_that_is_not_installed_stops_the_film(self):
        with self.assertRaisesRegex(film.BeatMissed, "no Omarchy theme called sepia"):
            film.change_theme({"dark_background": "#050718"}, "sepia", self.installed())

    def test_a_theme_too_near_the_desktops_is_refused_with_retro_82_suggested(self):
        directories = self.installed(**{"tokyo-night": (1, "#13141c"), "retro-82": (1, "#031222")})
        with self.assertRaisesRegex(film.BeatMissed, "too near.*--change-to retro-82"):
            film.change_theme({"dark_background": "#15161f"}, "tokyo-night", directories)

    def test_retro_82_too_near_the_desktops_suggests_the_nearest_theme_far_enough(self):
        directories = self.installed(**{"teal": (1, "#04141f"), "slate": (1, "#2a2f3a"),
                                        "white": (1, "#ffffff"), "retro-82": (1, "#031222")})
        with self.assertRaisesRegex(film.BeatMissed, "too near.*--change-to slate"):
            film.change_theme({"dark_background": "#041325"}, directories=directories)

    def test_a_refused_theme_stops_the_run_before_it_changes_anything(self):
        fake = FakeDesktop()
        desktop = film.Desktop(**fake.options(change_to="sepia", themes=self.installed()))
        with self.assertRaisesRegex(film.BeatMissed, "sepia"):
            desktop.survey()
        self.assertEqual(fake.acted, [])

    def test_the_container_film_goes_from_retro_82_to_tokyo_night(self):
        changes = [theme.colours["dark_background"] for theme in film.theme_changes()]
        self.assertEqual(changes, ["#031222", "#13141c"])

    def readers_settings(self, text, **session):
        root = self.directory / "home" / ".config" / "omaweb"
        root.mkdir(parents=True)
        (root / "settings.json").write_text(text, encoding="utf-8")
        return dict({"HOME": str(self.directory / "home")}, **session)

    def test_the_readers_own_sidebar_opacity_is_read_from_their_settings(self):
        session = self.readers_settings('{"version": 1, "sidebar-opacity": 0.85}')
        self.assertEqual(film.reader_sidebar_opacity(session), 0.85)

    def test_a_reader_who_set_no_sidebar_opacity_keeps_the_themes(self):
        session = self.readers_settings('{"version": 1, "engine-suggestions": true}')
        self.assertIsNone(film.reader_sidebar_opacity(session))

    def test_a_reader_with_no_settings_keeps_the_themes(self):
        self.assertIsNone(film.reader_sidebar_opacity({"HOME": str(self.directory / "none")}))

    def test_settings_omaweb_could_not_read_keep_the_themes(self):
        session = self.readers_settings('{"sidebar-opacity": ')
        self.assertIsNone(film.reader_sidebar_opacity(session))

    def test_the_readers_settings_are_found_where_omaweb_keeps_them(self):
        elsewhere = self.directory / "xdg" / "omaweb"
        elsewhere.mkdir(parents=True)
        (elsewhere / "settings.json").write_text('{"sidebar-opacity": 0.7}', encoding="utf-8")
        session = self.readers_settings('{"sidebar-opacity": 0.85}',
                                        XDG_CONFIG_HOME=str(self.directory / "xdg"))
        self.assertEqual(film.reader_sidebar_opacity(session), 0.7)
        session["OMAWEB_CONFIG_ROOT"] = str(self.directory / "home" / ".config" / "omaweb")
        self.assertEqual(film.reader_sidebar_opacity(session), 0.85)

    def test_the_survey_reads_the_readers_sidebar_opacity(self):
        session = self.readers_settings('{"sidebar-opacity": 0.85}')
        fake = FakeDesktop()
        desktop = film.Desktop(**fake.options(session=session))
        desktop.survey()
        self.assertEqual(desktop.sidebar_opacity, 0.85)

    def test_the_desktops_sidebar_is_staged_at_the_readers_opacity(self):
        config = self.staged(film.film_palette("retro-82"), sidebar_opacity=0.85)
        settings = json.loads((config / "settings.json").read_text(encoding="utf-8"))
        drawn = json.loads((config / "theme.json").read_text(encoding="utf-8"))
        self.assertEqual(drawn["opacity"]["sidebar"], 0.95)
        self.assertEqual(settings["sidebar-opacity"], 0.85)

    def test_the_containers_sidebar_keeps_the_themes_opacity(self):
        config = self.staged(film.film_palette("retro-82"))
        settings = json.loads((config / "settings.json").read_text(encoding="utf-8"))
        self.assertNotIn("sidebar-opacity", settings)

    def test_record_gives_the_desktops_browser_the_readers_sidebar_and_kestrel_by_address(self):
        desktop = mock.Mock(theme=film.Theme("nebula", {}), change=film.Theme("retro-82", {}),
                            sidebar_opacity=0.7)
        with mock.patch.object(film, "expect_shared_memory"), \
                mock.patch.object(film, "FixtureServer", return_value=mock.MagicMock(port=4321)), \
                mock.patch.object(film, "seed", side_effect=KeyboardInterrupt) as seed, \
                self.assertRaises(KeyboardInterrupt):
            film.record(Path("/build/omaweb-browser"), self.directory, desktop)
        _, opening, suggestions, sidebar = seed.call_args.args
        self.assertEqual((opening, suggestions, sidebar), (None, "http://127.0.0.1:4321", 0.7))


class EngineSuggestionsReachTheServer(unittest.TestCase):
    """Kestrel's suggestions, asked for as the browser asks: by its own network stack, which
    resolves a name through the system and never through the engine's host resolver rules."""

    def setUp(self):
        self.server = film.FixtureServer(("127.0.0.1", 0))
        self.server.start()
        self.addCleanup(self.server.stop)
        scratch = tempfile.TemporaryDirectory()
        self.addCleanup(scratch.cleanup)
        self.root = Path(scratch.name) / "film"

    def suggest_url(self, *seeded):
        _, config = film.seed(self.root, film.film_palette("retro-82"), *seeded)
        engines = json.loads((config / "search-engines.json").read_text(encoding="utf-8"))
        return engines["engines"][0]["suggestUrl"]

    def test_the_desktops_browser_reaches_kestrels_suggestions_without_a_hosts_file(self):
        address = self.suggest_url(f"http://127.0.0.1:{self.server.port}").replace("{query}",
                                                                                  "tra")
        opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
        with opener.open(address, timeout=5) as reply:
            self.assertEqual(json.loads(reply.read())[0], "tra")
        self.assertEqual(self.server.paths("kestrel.test"), ["/suggest?q=tra"])

    def test_only_a_suggestion_asked_of_the_address_is_kestrels(self):
        connection = http.client.HTTPConnection("127.0.0.1", self.server.port, timeout=5)
        self.addCleanup(connection.close)
        connection.request("GET", "/search?q=tra")
        connection.getresponse().read()
        self.assertEqual(self.server.paths("kestrel.test"), [])
        self.assertEqual(self.server.strays(), ["127.0.0.1/search?q=tra"])

    def test_the_containers_browser_asks_kestrel_by_name(self):
        self.assertEqual(self.suggest_url(), "http://kestrel.test/suggest?q={query}")


class RecordKeyboard(unittest.TestCase):
    def test_record_checks_the_keyboard_from_the_moment_the_window_is_placed(self):
        desktop = mock.Mock(theme=film.Theme("nebula", {}))
        desktop.place.return_value = (480, 283, 1600, 900)
        armed = []

        def start(_recorder):
            armed.append(film.BEFORE_KEYS)
            raise KeyboardInterrupt

        with tempfile.TemporaryDirectory() as directory, \
                mock.patch.object(film, "expect_shared_memory"), \
                mock.patch.object(film, "FixtureServer", return_value=mock.MagicMock(port=0)), \
                mock.patch.object(film, "seed", return_value=(Path(directory), Path(directory))), \
                mock.patch.object(film.subprocess, "Popen", return_value=mock.Mock(pid=4242)), \
                mock.patch.object(film, "omaweb", return_value=mock.Mock(returncode=0)), \
                mock.patch.object(film.time, "sleep"), \
                mock.patch.object(film.Recorder, "start", start), \
                mock.patch.object(film.os, "killpg"), \
                self.assertRaises(KeyboardInterrupt):
            film.record(Path("/build/omaweb-browser"), Path(directory), desktop)
        self.assertEqual(armed, [desktop.expect_keyboard])
        self.assertIsNone(film.BEFORE_KEYS)


class DesktopAnswers(unittest.TestCase):
    """Hyprland answers a dispatch it could not do with exit 0 and the error on stdout."""

    def test_a_dispatch_hyprland_refused_stops_the_film_and_puts_the_desktop_back(self):
        fake = FakeDesktop()
        fake.replies[tuple(TAKE[3])] = "Error: no such workspace\n"
        desktop = film.Desktop(**fake.options())
        desktop.survey()
        with self.assertRaisesRegex(film.BeatMissed, "no such workspace"), desktop.taken():
            self.fail("the run went on after Hyprland refused")
        self.assertEqual(fake.acted, TAKE + GIVE_BACK)

    def test_a_put_back_hyprland_refused_is_named_for_the_reader(self):
        fake = FakeDesktop()
        fake.replies[tuple(GIVE_BACK[0])] = "Error: no such workspace\n"
        desktop = film.Desktop(**fake.options())
        desktop.survey()
        with mock.patch("sys.stdout", new_callable=io.StringIO), desktop.taken():
            pass
        self.assertEqual(fake.acted, TAKE + GIVE_BACK)
        self.assertEqual(desktop.not_put_back, [shlex.join(GIVE_BACK[0])])

    def test_the_preflight_asks_hyprland_for_two_things_that_change_nothing(self):
        fake = FakeDesktop()
        desktop = film.Desktop(**fake.options())
        desktop.survey()
        desktop.preflight()
        self.assertEqual(fake.acted, PREFLIGHT)

    def test_a_hyprland_that_will_not_take_the_runs_dispatches_stops_it_before_any_change(self):
        fake = FakeDesktop()
        fake.replies[("hyprctl", "dispatch", 'hl.dsp.focus({ window = "address:0x5566cc" })')] = (
            "Error: unknown dispatcher\n")
        desktop = film.Desktop(**fake.options())
        desktop.survey()
        with self.assertRaisesRegex(film.BeatMissed, "unknown dispatcher"):
            desktop.preflight()

    def test_a_desktop_with_no_monitor_focused_stops_the_film_with_a_sentence(self):
        fake = FakeDesktop()
        fake.answers[("hyprctl", "monitors", "-j")] = json.dumps([dict(MONITOR, focused=False)])
        with self.assertRaisesRegex(film.BeatMissed, "no monitor"):
            film.Desktop(**fake.options()).survey()


class DesktopLastWord(unittest.TestCase):
    """However the run ends, the reader gets a sentence, and what was not put back."""

    def main(self, failure, not_put_back=()):
        fake = FakeDesktop()
        desktop = film.Desktop(**fake.options())
        desktop.not_put_back.extend(not_put_back)
        with tempfile.TemporaryDirectory() as directory, \
                mock.patch.object(film, "Desktop", return_value=desktop), \
                mock.patch.object(film, "record_on_desktop", side_effect=failure), \
                mock.patch.object(sys, "argv", ["record_film.py", "desktop", "--out", directory]), \
                mock.patch("sys.stderr", new_callable=io.StringIO) as said:
            code = film.main()
        return code, said.getvalue()

    def test_a_command_that_failed_is_named_in_a_sentence(self):
        code, said = self.main(subprocess.CalledProcessError(2, ["cmake", "--build", "build"]))
        self.assertEqual(code, 1)
        self.assertIn("the film was not made: cmake --build build exited 2", said)
        self.assertNotIn("Traceback", said)

    def test_an_answer_that_ran_out_is_a_sentence(self):
        code, said = self.main(StopIteration())
        self.assertEqual(code, 1)
        self.assertIn("the film was not made", said)

    def test_what_could_not_be_put_back_follows_a_missed_beat(self):
        code, said = self.main(film.BeatMissed("Theme", "the chrome kept its colour"),
                               ["omarchy-shell notifications setDnd off"])
        self.assertEqual(code, 1)
        self.assertIn("put back by hand: omarchy-shell notifications setDnd off", said)


class RecordCleanup(unittest.TestCase):
    """The browser is stopped whatever stops the run, and a second Ctrl+C cannot keep it going."""

    def record(self, killpg):
        server = mock.MagicMock()
        with tempfile.TemporaryDirectory() as directory, \
                mock.patch.object(film, "expect_sites_on_loopback"), \
                mock.patch.object(film, "expect_shared_memory"), \
                mock.patch.object(film, "FixtureServer", return_value=server), \
                mock.patch.object(film, "seed", return_value=(Path(directory), Path(directory))), \
                mock.patch.object(film.subprocess, "run"), \
                mock.patch.object(film.subprocess, "Popen", return_value=mock.Mock(pid=4242)), \
                mock.patch.object(film, "omaweb", side_effect=KeyboardInterrupt), \
                mock.patch.object(film.os, "killpg", side_effect=killpg), \
                self.assertRaises(KeyboardInterrupt):
            film.record(Path("/build/omaweb-browser"), Path(directory))
        return server

    def test_a_second_ctrl_c_cannot_keep_the_film_browser_running(self):
        before = signal.getsignal(signal.SIGINT)
        stopped = []
        self.record(lambda group, sent: stopped.append(
            (group, sent, signal.getsignal(signal.SIGINT))))
        self.assertEqual(stopped, [(4242, signal.SIGTERM, signal.SIG_IGN)])
        self.assertIs(signal.getsignal(signal.SIGINT), before)

    def test_a_browser_already_gone_does_not_cut_the_cleanup_short(self):
        def gone(group, sent):
            raise ProcessLookupError(3, "No such process")

        server = self.record(gone)
        server.stop.assert_called_once()


class DesktopLedger(unittest.TestCase):
    """A run killed outright, or whose terminal closed first, cannot put the desktop back, so it
    leaves a ledger, and the next run stops what it left running and restores notifications."""

    BROWSER = "/build/film-desktop/omaweb-browser"
    RELEASE = [["omarchy-shell", "notifications", "setDnd", "off"],
               ["omarchy-shell", "-q", "omarchy.indicators", "refresh"]]

    def setUp(self):
        self.directory = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.directory)
        self.ledger = self.directory / "desktop-run.json"
        self.proc = self.directory / "proc"
        self.proc.mkdir()
        self.killed = []

    def process(self, pid, group, *command):
        entry = self.proc / str(pid)
        entry.mkdir()
        (entry / "stat").write_text(f"{pid} ({Path(command[0]).name[:15]}) S 1 {group} {group} 0")
        (entry / "cmdline").write_bytes(b"\0".join(part.encode() for part in command) + b"\0")

    def desktop(self, fake, **options):
        return film.Desktop(**fake.options(**options), ledger=self.ledger, proc=self.proc,
                            kill=lambda pid, sent: self.killed.append(("kill", pid, sent)),
                            killpg=lambda group, sent: self.killed.append(("killpg", group, sent)))

    def left(self, **fields):
        self.ledger.write_text(json.dumps(dict(
            {"browser_session": 4000, "browser": self.BROWSER, "recorder": 4100,
             "release": self.RELEASE}, **fields)))

    def test_a_run_notes_what_it_must_put_back_before_it_changes_anything(self):
        fake = FakeDesktop()
        desktop = self.desktop(fake)
        desktop.survey()
        with desktop.taken():
            noted = json.loads(self.ledger.read_text())
        self.assertEqual(noted["release"], self.RELEASE)
        self.assertFalse(self.ledger.exists())

    def test_a_run_that_could_not_put_everything_back_keeps_its_ledger(self):
        fake = FakeDesktop()
        fake.failing.add(tuple(GIVE_BACK[2]))
        desktop = self.desktop(fake)
        desktop.survey()
        with mock.patch("sys.stdout", new_callable=io.StringIO), desktop.taken():
            pass
        self.assertTrue(self.ledger.exists())

    def test_the_next_run_stops_what_a_killed_run_left_and_restores_notifications(self):
        self.process(4000, 4000, "dbus-run-session", "--", self.BROWSER)
        self.process(4001, 4000, self.BROWSER)
        self.process(4100, 4100, "wf-recorder", "-y", "-g", "480,283 1600x900")
        self.left()
        fake = FakeDesktop()
        with mock.patch("sys.stdout", new_callable=io.StringIO) as said:
            self.desktop(fake).recover()
        self.assertEqual(self.killed, [("killpg", 4000, signal.SIGTERM),
                                       ("kill", 4100, signal.SIGINT)])
        self.assertEqual(fake.acted, self.RELEASE)
        self.assertFalse(self.ledger.exists())
        self.assertIn("did not finish", said.getvalue())

    def test_a_run_notes_the_workspace_it_runs_in_and_the_one_to_come_back_to(self):
        fake = FakeDesktop()
        desktop = self.desktop(fake, workspace="9")
        desktop.survey()
        with desktop.taken():
            noted = json.loads(self.ledger.read_text())
        self.assertEqual((noted["workspace"], noted["return_to"]), ("9", "2"))

    def test_the_next_run_comes_back_from_the_workspace_a_killed_run_left_on_show(self):
        self.left(workspace="9", return_to="4")
        fake = FakeDesktop()
        fake.answers[("hyprctl", "activeworkspace", "-j")] = json.dumps({"id": 9, "name": "9"})
        with mock.patch("sys.stdout", new_callable=io.StringIO):
            self.desktop(fake).recover()
        self.assertEqual(fake.acted, self.RELEASE + [
            ["hyprctl", "dispatch", 'hl.dsp.focus({ workspace = "4" })']])
        self.assertFalse(self.ledger.exists())

    def test_the_next_run_comes_back_from_the_films_own_named_workspace(self):
        self.left(workspace="name:omaweb-film", return_to="2")
        fake = FakeDesktop()
        fake.answers[("hyprctl", "activeworkspace", "-j")] = json.dumps(
            {"id": -98, "name": "omaweb-film"})
        with mock.patch("sys.stdout", new_callable=io.StringIO):
            self.desktop(fake).recover()
        self.assertEqual(fake.acted[-1],
                         ["hyprctl", "dispatch", 'hl.dsp.focus({ workspace = "2" })'])

    def test_the_next_run_stays_where_the_reader_went_after_a_killed_run(self):
        self.left(workspace="9", return_to="4")
        fake = FakeDesktop()
        with mock.patch("sys.stdout", new_callable=io.StringIO):
            self.desktop(fake).recover()
        self.assertEqual(fake.acted, self.RELEASE)

    BACK_TO_NEBULA = ["omarchy-theme-set", "ethereal-nebula-silver"]

    def switched(self, fake, stop=None):
        """A run that switches the desktop to Retro 82 for its Theme beat, then ends by `stop`."""
        desktop = self.desktop(fake)
        desktop.survey()
        with desktop.taken():
            desktop.set_theme(desktop.change)
            self.noted = json.loads(self.ledger.read_text())
            if stop:
                raise stop
        return desktop

    def test_the_theme_the_run_started_with_is_noted_before_the_first_switch(self):
        fake = FakeDesktop()
        self.switched(fake)
        self.assertEqual(self.noted["theme"], "ethereal-nebula-silver")
        self.assertEqual(fake.acted[len(TAKE)], ["omarchy-theme-set", "retro-82"])

    def test_a_run_that_ends_on_the_theme_it_switched_to_puts_the_desktops_back_first(self):
        fake = FakeDesktop()
        self.switched(fake)
        self.assertEqual(fake.acted[len(TAKE) + 1:], [self.BACK_TO_NEBULA] + GIVE_BACK)

    def test_a_run_stopped_after_a_switch_puts_the_desktops_theme_back(self):
        for stop in (KeyboardInterrupt(), film.BeatMissed("Theme", "chrome kept its colour")):
            fake = FakeDesktop()
            with self.assertRaises(type(stop)):
                self.switched(fake, stop)
            self.assertEqual(fake.acted[len(TAKE) + 1:], [self.BACK_TO_NEBULA] + GIVE_BACK)

    def test_a_run_that_switched_back_itself_does_not_switch_again(self):
        fake = FakeDesktop()
        desktop = self.desktop(fake)
        desktop.survey()
        with desktop.taken():
            desktop.set_theme(desktop.change)
            desktop.set_theme(desktop.theme)
        self.assertEqual(fake.acted.count(self.BACK_TO_NEBULA), 1)

    def test_the_next_run_puts_back_the_theme_a_killed_run_switched_from(self):
        fake = FakeDesktop()
        (fake.options()["current"] / "theme.name").write_text("retro-82\n")
        self.left(theme="ethereal-nebula-silver")
        with mock.patch("sys.stdout", new_callable=io.StringIO):
            self.desktop(fake).recover()
        self.assertEqual(fake.acted, [self.BACK_TO_NEBULA] + self.RELEASE)

    def test_the_next_run_leaves_a_theme_already_put_back(self):
        fake = FakeDesktop()
        self.left(theme="ethereal-nebula-silver")
        with mock.patch("sys.stdout", new_callable=io.StringIO):
            self.desktop(fake).recover()
        self.assertEqual(fake.acted, self.RELEASE)

    def test_a_desktop_without_omarchy_theme_set_stops_the_film_before_any_change(self):
        fake = FakeDesktop()
        desktop = self.desktop(fake, which=lambda program: None if program == "omarchy-theme-set"
                               else fake.which(program))
        with self.assertRaisesRegex(film.BeatMissed, "omarchy-theme-set"):
            desktop.survey()
        self.assertEqual(fake.acted, [])

    def test_a_process_number_another_program_has_taken_since_is_left_alone(self):
        self.process(4000, 4000, "/usr/bin/bash")
        self.process(4100, 4100, "/usr/bin/nvim", "notes.md")
        self.left()
        fake = FakeDesktop()
        with mock.patch("sys.stdout", new_callable=io.StringIO):
            self.desktop(fake).recover()
        self.assertEqual(self.killed, [])
        self.assertEqual(fake.acted, self.RELEASE)

    def test_a_dry_run_names_what_it_would_stop_and_stops_nothing(self):
        self.process(4000, 4000, "dbus-run-session", "--", self.BROWSER)
        self.process(4100, 4100, "wf-recorder")
        self.left()
        fake = FakeDesktop()
        with mock.patch("sys.stdout", new_callable=io.StringIO) as said:
            self.desktop(fake, dry_run=True).recover()
        self.assertEqual(self.killed, [])
        self.assertEqual(fake.acted, [])
        self.assertTrue(self.ledger.exists())
        self.assertIn("would run: kill -TERM -- -4000", said.getvalue())
        self.assertIn("would run: kill -INT 4100", said.getvalue())

    def test_a_desktop_with_no_ledger_has_nothing_to_recover(self):
        fake = FakeDesktop()
        self.desktop(fake).recover()
        self.assertEqual((self.killed, fake.acted), ([], []))

    def test_the_run_recovers_after_the_key_and_before_it_reads_the_desktop(self):
        self.left(browser_session=None, recorder=None)
        fake = FakeDesktop()
        desktop = self.desktop(fake)
        with mock.patch.object(film, "build_for_desktop", return_value=Path(self.BROWSER)), \
                mock.patch.object(film, "expect_desktop_packages"), \
                mock.patch.object(film, "record"), mock.patch.object(film, "compose"), \
                mock.patch("builtins.input", return_value=""), \
                mock.patch("sys.stdout", new_callable=io.StringIO):
            film.record_on_desktop(desktop, self.directory)
        self.assertEqual(fake.acted[:2], self.RELEASE)
        self.assertEqual(fake.acted[2:4], PREFLIGHT)

    def test_record_notes_the_browsers_session_as_soon_as_it_starts(self):
        desktop = mock.Mock(theme=film.Theme("nebula", {}))
        with mock.patch.object(film, "expect_shared_memory"), \
                mock.patch.object(film, "FixtureServer", return_value=mock.MagicMock(port=0)), \
                mock.patch.object(film, "seed", return_value=(self.directory, self.directory)), \
                mock.patch.object(film.subprocess, "Popen", return_value=mock.Mock(pid=4242)), \
                mock.patch.object(film, "omaweb", side_effect=KeyboardInterrupt), \
                mock.patch.object(film.os, "killpg"), \
                self.assertRaises(KeyboardInterrupt):
            film.record(Path(self.BROWSER), self.directory, desktop)
        desktop.note.assert_any_call(browser_session=4242, browser=self.BROWSER)


if __name__ == "__main__":
    unittest.main()
