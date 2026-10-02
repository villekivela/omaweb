#!/usr/bin/env python3
"""The Elephant menu that lists Omaweb's tabs, run outside Elephant.

Elephant is a Go program with its own Lua, which hands a menu `jsonDecode` and runs its
`GetEntries` when the menu opens. The test supplies the two things Elephant would: the answer
`omaweb tabs --all --json` printed, through a stand-in for `io.popen`, and a `jsonDecode`
that returns what Python decoded from that same text. A menu that passed anything else to
`jsonDecode` would get nothing back, so the text is the one `omaweb` printed.
"""

from __future__ import annotations

import json
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
MENU = ROOT / "integrations/elephant/menus/omawebtabs.lua"


def lua_interpreter() -> str:
    named = os.environ.get("OMAWEB_LUA")
    if named:
        return named
    for candidate in ("lua", "lua5.4", "lua5.3", "lua5.1", "luajit"):
        found = shutil.which(candidate)
        if found:
            return found
    raise RuntimeError("No Lua interpreter: set OMAWEB_LUA.")


def lua_string(text: str) -> str:
    """A Lua string literal for text, written as decimal escapes so no byte needs quoting."""
    return '"' + "".join(f"\\{byte:03d}" for byte in text.encode("utf-8")) + '"'


def lua_value(value) -> str:
    if isinstance(value, dict):
        return "{" + ",".join(f"[{lua_string(k)}]={lua_value(v)}" for k, v in value.items()) + "}"
    if isinstance(value, list):
        return "{" + ",".join(lua_value(v) for v in value) + "}"
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, str):
        return lua_string(value)
    return str(value)


def decoded(hexed: str) -> str:
    return bytes.fromhex(hexed).decode("utf-8")


DRIVER = """
local printed = {}
local function hex(text)
    return (tostring(text):gsub(".", function(c) return string.format("%02x", c:byte()) end))
end
local function emit(...)
    local fields = {}
    for _, field in ipairs({...}) do table.insert(fields, hex(field)) end
    print(table.concat(fields, " "))
end

local answered = ASKED
local decodes = DECODES
io.popen = function(command)
    emit("popen", command)
    if answered == nil then return nil end
    return {
        read = function() return answered end,
        close = function() end,
    }
end
jsonDecode = function(text)
    local value = decodes[text]
    if value == nil then return nil, "not json" end
    return value
end
local executed = {}
os.execute = function(command) emit("execute", command) end

dofile(MENU)
emit("name", Name)
emit("action", Action)
if ACT == nil then
    for _, entry in ipairs(GetEntries()) do
        emit("entry", entry.Text, entry.Subtext, entry.Value, table.concat(entry.Keywords or {}, "\\n"))
    end
else
    Focus(ACT)
end
"""


def run_menu(*, answer: str | None, act: str | None = None) -> dict:
    """Runs the menu against `answer`, the text omaweb printed, or None when popen fails."""
    decodes = {}
    if answer:
        try:
            decodes[answer] = json.loads(answer)
        except ValueError:
            pass
    program = (
        f"MENU={lua_string(str(MENU))}\n"
        f"ASKED={'nil' if answer is None else lua_string(answer)}\n"
        f"DECODES={lua_value(decodes)}\n"
        f"ACT={'nil' if act is None else lua_string(act)}\n" + DRIVER
    )
    with tempfile.TemporaryDirectory() as directory:
        script = Path(directory) / "driver.lua"
        script.write_text(program)
        completed = subprocess.run(
            [lua_interpreter(), str(script)], capture_output=True, text=True, check=False
        )
    if completed.returncode != 0:
        raise AssertionError(completed.stderr)
    result: dict = {"entries": [], "popen": [], "execute": []}
    for line in completed.stdout.splitlines():
        kind, *fields = [decoded(field) for field in line.split(" ")]
        if kind == "entry":
            result["entries"].append(fields)
        elif kind in ("popen", "execute"):
            result[kind].append(fields[0])
        else:
            result[kind] = fields[0]
    return result


def answer_of(*tabs: dict) -> str:
    return json.dumps({"ok": True, "tabs": list(tabs)}, ensure_ascii=False, separators=(",", ":"))


def tab(identifier: str, title: str, url: str, space: str, **more) -> dict:
    return {
        "id": identifier,
        "space": space.lower(),
        "spaceName": space,
        "url": url,
        "title": title,
        "pinned": False,
        "current": False,
    } | more


class ElephantMenuTest(unittest.TestCase):
    def test_is_the_menu_the_package_ships_under_its_own_name(self):
        result = run_menu(answer=answer_of())
        self.assertEqual(result["name"], "omawebtabs")
        self.assertEqual(result["action"], "lua:Focus")

    def test_asks_the_browser_for_every_spaces_tabs_without_starting_it(self):
        result = run_menu(answer=answer_of())
        self.assertEqual(result["popen"], ["omaweb tabs --all --json 2>/dev/null"])

    def test_lists_a_tab_by_title_with_its_host_and_space_beneath(self):
        result = run_menu(
            answer=answer_of(
                tab("t1", "Inbox", "https://mail.example.org:8443/u/0/#inbox", "Personal"),
                tab("t2", "Pull requests", "https://github.com/pulls", "Work", pinned=True),
            )
        )
        self.assertEqual(
            result["entries"],
            [
                [
                    "Inbox",
                    "mail.example.org:8443 · Personal",
                    "t1",
                    "https://mail.example.org:8443/u/0/#inbox",
                ],
                ["Pull requests", "github.com · Work", "t2", "https://github.com/pulls"],
            ],
        )

    def test_keeps_quotes_and_non_ascii_text_and_flattens_tabs(self):
        result = run_menu(
            answer=answer_of(
                tab(
                    "t1",
                    'She said "hi", it\'s\tnice\n  Ääkköset 日本語 🙂',
                    "https://user:secret@pörssi.example/a?b=\"c\"",
                    "Työ \"x\"",
                )
            )
        )
        text, subtext, value, keywords = result["entries"][0]
        self.assertEqual(text, 'She said "hi", it\'s nice Ääkköset 日本語 🙂')
        # A password in an address is not shown.
        self.assertEqual(subtext, 'pörssi.example · Työ "x"')
        self.assertEqual(value, "t1")
        self.assertEqual(keywords, "https://user:secret@pörssi.example/a?b=\"c\"")

    def test_shows_the_address_of_a_tab_with_no_title(self):
        result = run_menu(answer=answer_of(tab("t1", "", "about:blank", "Personal")))
        self.assertEqual(result["entries"][0][:2], ["about:blank", "Personal"])

    def test_says_so_and_lists_nothing_when_omaweb_is_not_running(self):
        # `omaweb` prints nothing to stdout when no browser answers.
        for answer in ("", None):
            with self.subTest(answer=answer):
                result = run_menu(answer=answer)
                self.assertEqual(len(result["entries"]), 1)
                text, _, value, _ = result["entries"][0]
                self.assertEqual(text, "Omaweb is not running")
                self.assertEqual(value, "")

    def test_says_what_the_browser_refused_with(self):
        refusal = json.dumps({"ok": False, "code": "failed", "error": "Not today."})
        result = run_menu(answer=refusal)
        self.assertEqual(result["entries"][0][0], "Omaweb could not list its tabs")
        self.assertEqual(result["entries"][0][1], "Not today.")
        self.assertEqual(result["entries"][0][2], "")

    def test_focuses_the_chosen_tab_by_its_id(self):
        result = run_menu(answer=answer_of(), act="t1")
        self.assertEqual(result["execute"], ["omaweb focus -- 't1'"])

    def test_quotes_an_id_for_the_shell(self):
        result = run_menu(answer=answer_of(), act="a'; touch x; '")
        self.assertEqual(result["execute"], ["omaweb focus -- 'a'\\''; touch x; '\\'''"])

    def test_does_nothing_for_the_line_that_says_omaweb_is_not_running(self):
        self.assertEqual(run_menu(answer=answer_of(), act="")["execute"], [])


if __name__ == "__main__":
    unittest.main()
