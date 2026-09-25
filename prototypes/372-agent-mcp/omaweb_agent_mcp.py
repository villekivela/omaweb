#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# dependencies = ["websockets>=13"]
# ///
"""PROTOTYPE (#372, proposed ADR 0051). Throwaway: answers whether handing Omaweb to an Agent is
useful and fast enough. Not product code, no tests, minimal error handling.

A stdio MCP server that drives Omaweb through the `--remote-debugging` development launch. That
listener reaches every Space and turns Private windows off, which is why the real design uses a
socket and an engine patch instead.

    omaweb --remote-debugging            # port 9222; OMAWEB_CDP overrides it here
    claude mcp add omaweb-proto -- uv run --script <this file>
    uv run --script <this file> --stats  # summarise the call log
"""

import asyncio
import base64
import json
import os
import pathlib
import shlex
import statistics
import sys
import tempfile
import time
import urllib.request

import websockets

PORT = int(os.environ.get("OMAWEB_CDP", "9222"))
HERE = pathlib.Path(__file__).resolve().parent
PAGE_JS = (HERE / "page.js").read_text()
WORLD = "omaweb-agent-prototype"
OUT = pathlib.Path(tempfile.gettempdir()) / "omaweb-agent-prototype"
LOG = OUT / "calls.jsonl"

KEYS = {
    "Enter": (13, "Enter", "\r"),
    "Tab": (9, "Tab", ""),
    "Escape": (27, "Escape", ""),
    "Backspace": (8, "Backspace", ""),
    "Delete": (46, "Delete", ""),
    "ArrowUp": (38, "ArrowUp", ""),
    "ArrowDown": (40, "ArrowDown", ""),
    "ArrowLeft": (37, "ArrowLeft", ""),
    "ArrowRight": (39, "ArrowRight", ""),
    "PageUp": (33, "PageUp", ""),
    "PageDown": (34, "PageDown", ""),
    "Home": (36, "Home", ""),
    "End": (35, "End", ""),
    "Space": (32, "Space", " "),
}
MODIFIERS = {"Alt": 1, "Ctrl": 2, "Control": 2, "Meta": 4, "Cmd": 4, "Shift": 8}


def pages():
    with urllib.request.urlopen(f"http://127.0.0.1:{PORT}/json/list", timeout=3) as r:
        targets = json.load(r)
    return [
        t
        for t in targets
        if t.get("type") == "page" and not t.get("url", "").startswith("devtools://")
    ]


class Tab:
    def __init__(self, target):
        self.id = target["id"]
        self.ws_url = target["webSocketDebuggerUrl"]
        self.ws = None
        self.seq = 0
        self.pending = {}
        self.context = None
        self.main_frame = None
        self.navigations = 0
        self.loaded = asyncio.Event()
        self.console = []
        self.console_seq = 0
        self.dialog = None

    async def connect(self):
        self.ws = await websockets.connect(self.ws_url, max_size=None)
        asyncio.create_task(self.pump())
        await self.send("Page.enable")
        await self.send("Runtime.enable")
        await self.send("Log.enable")
        tree = await self.send("Page.getFrameTree")
        self.main_frame = tree["frameTree"]["frame"]["id"]

    async def pump(self):
        async for raw in self.ws:
            msg = json.loads(raw)
            if "id" in msg:
                future = self.pending.pop(msg["id"], None)
                if future and not future.done():
                    if "error" in msg:
                        future.set_exception(RuntimeError(msg["error"].get("message")))
                    else:
                        future.set_result(msg.get("result", {}))
            else:
                self.event(msg["method"], msg.get("params", {}))

    def event(self, method, p):
        if method == "Page.frameStartedLoading" and p.get("frameId") == self.main_frame:
            self.navigations += 1
            self.loaded.clear()
        elif method in ("Page.domContentEventFired", "Page.loadEventFired"):
            self.loaded.set()
        elif method in ("Page.navigatedWithinDocument", "Page.frameStoppedLoading"):
            # A fragment or history.pushState change fires no load event.
            if p.get("frameId") == self.main_frame:
                self.loaded.set()
        elif method == "Page.frameNavigated" and not p["frame"].get("parentId"):
            self.main_frame = p["frame"]["id"]
            self.context = None
        elif method == "Runtime.executionContextsCleared":
            self.context = None
        elif method == "Runtime.executionContextDestroyed":
            if p.get("executionContextId") == self.context:
                self.context = None
        elif method == "Runtime.consoleAPICalled":
            kind = p["type"]
            if kind == "debug":
                return
            text = " ".join(
                str(a.get("value", a.get("description", a.get("type")))) for a in p["args"]
            )
            if text.startswith("__omaweb"):
                return
            frame = (p.get("stackTrace") or {}).get("callFrames") or [{}]
            where = f"{frame[0].get('url', '')}:{frame[0].get('lineNumber', 0) + 1}"
            self.remember("warning" if kind == "warn" else kind, text, where)
        elif method == "Runtime.exceptionThrown":
            d = p["exceptionDetails"]
            text = (d.get("exception") or {}).get("description") or d.get("text", "")
            self.remember("error", text.split("\n")[0], f"{d.get('url', '')}:{d.get('lineNumber', 0) + 1}")
        elif method == "Log.entryAdded":
            e = p["entry"]
            self.remember(e["level"], e["text"], e.get("url", ""))
        elif method == "Page.javascriptDialogOpening":
            self.dialog = f'{p["type"]}: "{p["message"]}"'
        elif method == "Page.javascriptDialogClosed":
            self.dialog = None

    def remember(self, level, text, where):
        self.console_seq += 1
        self.console.append((self.console_seq, level, text[:300], where))
        del self.console[:-500]

    async def send(self, method, **params):
        self.seq += 1
        future = asyncio.get_running_loop().create_future()
        self.pending[self.seq] = future
        await self.ws.send(json.dumps({"id": self.seq, "method": method, "params": params}))
        return await asyncio.wait_for(future, 30)

    async def world(self):
        if self.context is None:
            r = await self.send("Page.createIsolatedWorld", frameId=self.main_frame, worldName=WORLD)
            self.context = r["executionContextId"]
            await self.send(
                "Runtime.evaluate", expression=PAGE_JS, contextId=self.context, returnByValue=True
            )
        return self.context

    async def call(self, expression, retry=True):
        try:
            r = await self.send(
                "Runtime.evaluate",
                expression=expression,
                contextId=await self.world(),
                returnByValue=True,
                awaitPromise=True,
            )
        except RuntimeError as e:
            if retry and "context" in str(e).lower():
                self.context = None
                return await self.call(expression, retry=False)
            raise
        if "exceptionDetails" in r:
            d = r["exceptionDetails"]
            message = (d.get("exception") or {}).get("description") or d.get("text")
            if retry and "__agent is not defined" in message:
                self.context = None
                return await self.call(expression, retry=False)
            raise RuntimeError(message.split("\n")[0])
        return r["result"].get("value")

    async def settle(self, before, quiet_ms, deadline):
        await asyncio.sleep(0.05)
        if self.navigations != before:
            remaining = max(0.1, deadline - time.monotonic())
            try:
                await asyncio.wait_for(self.loaded.wait(), remaining)
            except asyncio.TimeoutError:
                return "load timed out"
        for _ in range(3):
            remaining_ms = max(100, int((deadline - time.monotonic()) * 1000))
            try:
                r = await self.call(f"__agent.quiet({quiet_ms}, {remaining_ms})")
                return None if r["settled"] else "page never went quiet"
            except RuntimeError:
                await asyncio.sleep(0.1)
        return None


class Agent:
    def __init__(self):
        self.tabs = {}
        self.current = None

    async def tab(self, prefix=None):
        if prefix:
            matches = [t for t in pages() if t["id"].lower().startswith(prefix.lower())]
            if not matches:
                raise RuntimeError(f"no tab {prefix}; call tabs")
            target = matches[0]
        elif self.current:
            return self.tabs[self.current]
        else:
            target = await self.visible_page()
        if target["id"] not in self.tabs:
            t = Tab(target)
            await t.connect()
            self.tabs[target["id"]] = t
        self.current = target["id"]
        return self.tabs[target["id"]]

    async def visible_page(self):
        for target in pages():
            async with websockets.connect(target["webSocketDebuggerUrl"], max_size=None) as ws:
                await ws.send(
                    json.dumps(
                        {
                            "id": 1,
                            "method": "Runtime.evaluate",
                            "params": {"expression": "document.visibilityState", "returnByValue": True},
                        }
                    )
                )
                reply = json.loads(await asyncio.wait_for(ws.recv(), 5))
                if reply.get("result", {}).get("result", {}).get("value") == "visible":
                    return target
        raise RuntimeError("no visible tab; open one in Omaweb or pass tab")

    # Verbs

    async def v_tabs(self, a):
        lines = []
        for t in pages():
            mark = "*" if t["id"] == self.current else " "
            lines.append(f'{mark} {t["id"][:6].lower()} {t["title"][:60]!r} {t["url"][:100]}')
        return "\n".join(lines) or "no tabs"

    async def v_open(self, a):
        t = await self.tab(a.get("tab"))
        if a.get("url"):
            before = t.navigations
            await t.send("Page.navigate", url=a["url"])
            problem = await t.settle(before, 300, time.monotonic() + 10)
            if problem:
                return f"{problem}\n\n" + await self.look(t, False)
        return await self.look(t, False)

    async def v_look(self, a):
        return await self.look(await self.tab(a.get("tab")), a.get("all", False))

    async def look(self, t, everything):
        r = await t.call(f"__agent.look({json.dumps(bool(everything))})")
        out = [f'{r["title"]}\n{r["url"]}']
        if r["visibility"] != "visible":
            out.append("(tab not on show: it is throttled and cannot be clicked natively)")
        if t.dialog:
            out.append(f"DIALOG OPEN {t.dialog}; answer with: dialog accept|dismiss [text]")
        if r["heads"]:
            out.append("\n".join(r["heads"]))
        if r["text"]:
            out.append(r["text"])
        out.append("targets:\n" + ("\n".join(r["targets"]) or "(none)"))
        more = []
        if r["above"]:
            more.append(f'{r["above"]} above')
        if r["below"]:
            more.append(f'{r["below"]} below')
        out.append(f'scroll {r["scroll"]}' + (f' · more targets: {", ".join(more)}' if more else ""))
        return "\n\n".join(out)

    async def v_do(self, a):
        t = await self.tab(a.get("tab"))
        quiet = int(a.get("quiet_ms", 300))
        timeout = int(a.get("timeout_ms", 10000)) / 1000
        report = []
        for i, step in enumerate(a["steps"], 1):
            deadline = time.monotonic() + timeout
            before = t.navigations
            try:
                note = await self.step(t, step, deadline)
                # Typing into a field or waiting already leaves the page where the next step wants
                # it, so only steps that can start work in the page pay for the quiet period.
                cheap = step.split(" ", 1)[0] in ("fill", "select", "wait")
                problem = None
                if not cheap or t.navigations != before:
                    problem = await t.settle(before, quiet, deadline)
                report.append(f"{i}. ok {step}" + (f" ({note})" if note else "") + (f" [{problem}]" if problem else ""))
            except Exception as e:
                report.append(f"{i}. FAILED {step}: {e}")
                break
        return "\n".join(report) + "\n\n" + await self.look(t, False)

    async def step(self, t, step, deadline):
        verb, _, rest = step.strip().partition(" ")
        rest = rest.strip()
        if verb == "click":
            spot = await t.call(f"__agent.target({json.dumps(rest)})")
            if spot["visibility"] == "visible" and spot["reachable"]:
                x, y = spot["x"], spot["y"]
                await t.send("Input.dispatchMouseEvent", type="mouseMoved", x=x, y=y)
                for kind in ("mousePressed", "mouseReleased"):
                    await t.send(
                        "Input.dispatchMouseEvent", type=kind, x=x, y=y, button="left", clickCount=1
                    )
                return None
            await t.call(f"__agent.click({json.dumps(rest)})")
            return "synthetic click"
        if verb == "fill":
            target, _, text = rest.partition(" ")
            await t.call(f"__agent.focus({json.dumps(target)})")
            if text:
                await t.send("Input.insertText", text=text)
            else:
                await self.key(t, "Backspace")
            value = await t.call(f"__agent.value({json.dumps(target)})")
            if (value or "") != text:
                await t.call(f"__agent.setValue({json.dumps(target)}, {json.dumps(text)})")
                return "synthetic fill"
            return None
        if verb == "press":
            await self.key(t, rest)
            return None
        if verb == "select":
            target, _, value = rest.partition(" ")
            chosen = await t.call(f"__agent.select({json.dumps(target)}, {json.dumps(value)})")
            return f"chose {chosen!r}"
        if verb == "scroll":
            await t.call(f"__agent.scroll({json.dumps(rest or 'down')})")
            return None
        if verb == "back":
            await t.call("history.back()")
            return None
        if verb == "goto":
            await t.send("Page.navigate", url=rest)
            return None
        if verb == "dialog":
            answer, _, text = rest.partition(" ")
            params = {"accept": answer == "accept"}
            if text:
                params["promptText"] = text
            await t.send("Page.handleJavaScriptDialog", **params)
            return None
        if verb == "wait":
            kind, _, value = rest.partition(" ")
            if kind == "ms":
                await asyncio.sleep(int(value) / 1000)
                return None
            while time.monotonic() < deadline:
                try:
                    if kind == "text" and await t.call(f"__agent.has({json.dumps(value)})"):
                        return None
                    if kind == "url" and value in await t.call("location.href"):
                        return None
                except RuntimeError:
                    pass
                await asyncio.sleep(0.1)
            raise RuntimeError(f"timed out waiting for {kind} {value!r}")
        raise RuntimeError(
            "unknown step; use click/fill/press/select/scroll/back/goto/wait/dialog"
        )

    async def key(self, t, combo):
        *mods, name = combo.split("+")
        modifiers = sum(MODIFIERS.get(m, 0) for m in mods)
        if name in KEYS:
            code, key_name, text = KEYS[name]
            key = " " if name == "Space" else key_name
        elif len(name) == 1:
            code, key_name, text, key = ord(name.upper()), "Key" + name.upper(), name, name
        else:
            raise RuntimeError(f"unknown key {name}")
        if modifiers & (2 | 4):
            text = ""
        down = {"type": "keyDown" if text else "rawKeyDown", "key": key, "code": key_name,
                "windowsVirtualKeyCode": code, "modifiers": modifiers}
        if text:
            down["text"] = text
        await t.send("Input.dispatchKeyEvent", **down)
        await t.send("Input.dispatchKeyEvent", type="keyUp", key=key, code=key_name,
                     windowsVirtualKeyCode=code, modifiers=modifiers)

    async def v_read(self, a):
        t = await self.tab(a.get("tab"))
        offset = int(a.get("offset", 0))
        r = await t.call(
            f"__agent.read({json.dumps(a.get('selector'))}, {offset}, {int(a.get('max', 12000))})"
        )
        end = offset + len(r["md"])
        tail = f"\n\n[chars {offset}-{end} of {r['total']}; more with offset={end}]" if end < r["total"] else ""
        return r["md"] + tail

    async def v_shot(self, a):
        t = await self.tab(a.get("tab"))
        r = await t.send(
            "Page.captureScreenshot", format="png", captureBeyondViewport=bool(a.get("full"))
        )
        OUT.mkdir(parents=True, exist_ok=True)
        path = OUT / f"shot-{int(time.time() * 1000)}.png"
        path.write_bytes(base64.b64decode(r["data"]))
        return str(path)

    async def v_console(self, a):
        t = await self.tab(a.get("tab"))
        level = a.get("level", "warning")
        wanted = {"error": {"error"}, "warning": {"error", "warning"}}.get(level)
        since = int(a.get("since", 0))
        rows = [
            f"{seq} {lvl} {text}" + (f"  @ {where}" if where and where != ":1" else "")
            for seq, lvl, text, where in t.console
            if seq > since and (wanted is None or lvl in wanted)
        ]
        cursor = t.console_seq
        head = "(collected since this tab was first used)" if not since else ""
        return "\n".join(filter(None, [head, *rows[-100:], f"cursor {cursor}"]))

    async def v_eval(self, a):
        t = await self.tab(a.get("tab"))
        return json.dumps(await t.call(a["expression"]), ensure_ascii=False)[:8000]


TAB = {"tab": {"type": "string", "description": "tab id prefix; default: current tab"}}
TOOLS = [
    ("tabs", "List Omaweb tabs. * marks the current one.", {}),
    ("open", "Make a tab current and optionally navigate it. Returns look.",
     {"url": {"type": "string"}, **TAB}),
    ("look", "Page title, URL, headings, short text, and interactive targets in view with "
             "labels. Labels stay valid until navigation.",
     {"all": {"type": "boolean", "description": "include off-screen targets"}, **TAB}),
    ("do", "Run steps in order, waiting for the page to settle after each; stops at the first "
           "failure; returns look. Steps: 'click <label>', 'fill <label> <text>', "
           "'press <Key|Ctrl+a>', 'select <label> <option>', 'scroll down|up|top|bottom|<label>', "
           "'back', 'goto <url>', 'wait text|url <value>', 'wait ms <n>', "
           "'dialog accept|dismiss [text]'.",
     {"steps": {"type": "array", "items": {"type": "string"}}, "quiet_ms": {"type": "integer"},
      "timeout_ms": {"type": "integer"}, **TAB}),
    ("read", "Page or a CSS selector's content as Markdown, paged.",
     {"selector": {"type": "string"}, "offset": {"type": "integer"}, **TAB}),
    ("shot", "Screenshot to a PNG file; returns its path.",
     {"full": {"type": "boolean"}, **TAB}),
    ("console", "Console errors and warnings, plus failed loads. Pass since=<cursor> for new ones.",
     {"level": {"type": "string", "enum": ["error", "warning", "all"]},
      "since": {"type": "integer"}, **TAB}),
    ("eval", "Evaluate a JS expression in an isolated world (DOM access, not page JS state); "
             "returns JSON.", {"expression": {"type": "string"}, **TAB}),
]
REQUIRED = {"do": ["steps"], "eval": ["expression"]}
INSTRUCTIONS = (
    "Omaweb prototype browser. Loop: look, then one do with several steps, which returns a fresh "
    "look. Prefer labels from look over eval. Use read for page content and console for errors."
)


def log(tool, args, ms, chars, error, synthetic=0):
    OUT.mkdir(parents=True, exist_ok=True)
    safe = dict(args)
    if "steps" in safe:
        # A fill's text may be a password; the log keeps the label only.
        safe["steps"] = [" ".join(s.split(" ")[:2]) if s.startswith("fill ") else s for s in safe["steps"]]
    with LOG.open("a") as f:
        f.write(json.dumps({"t": time.time(), "tool": tool, "args": safe, "ms": ms,
                            "chars": chars, "error": error, "synthetic": synthetic}) + "\n")


async def handle(agent, msg):
    method = msg.get("method")
    if method == "initialize":
        return {
            "protocolVersion": msg["params"].get("protocolVersion", "2025-06-18"),
            "capabilities": {"tools": {}},
            "serverInfo": {"name": "omaweb-agent-prototype", "version": "0"},
            "instructions": INSTRUCTIONS,
        }
    if method == "ping":
        return {}
    if method == "tools/list":
        return {"tools": [
            {"name": n, "description": d,
             "inputSchema": {"type": "object", "properties": p, "required": REQUIRED.get(n, [])}}
            for n, d, p in TOOLS
        ]}
    if method == "tools/call":
        name = msg["params"]["name"]
        args = msg["params"].get("arguments") or {}
        start = time.monotonic()
        error = False
        try:
            text = await getattr(agent, "v_" + name)(args)
        except Exception as e:
            error = True
            text = f"error: {e}"
            if isinstance(e, (OSError, websockets.ConnectionClosed)):
                agent.tabs.clear()
                agent.current = None
                text += f"\n(is Omaweb running with --remote-debugging={PORT}?)"
        ms = int((time.monotonic() - start) * 1000)
        text += f"\n[{(agent.current or '')[:6].lower()} · {ms} ms]"
        log(name, args, ms, len(text), error, text.count("(synthetic "))
        return {"content": [{"type": "text", "text": text}], "isError": error}
    raise KeyError(method)


async def serve():
    agent = Agent()
    loop = asyncio.get_running_loop()
    reader = asyncio.StreamReader(limit=1 << 24)
    await loop.connect_read_pipe(lambda: asyncio.StreamReaderProtocol(reader), sys.stdin)
    lock = asyncio.Lock()

    async def answer(msg):
        try:
            async with lock:
                result = await handle(agent, msg)
            reply = {"jsonrpc": "2.0", "id": msg["id"], "result": result}
        except KeyError:
            reply = {"jsonrpc": "2.0", "id": msg["id"],
                     "error": {"code": -32601, "message": "method not found"}}
        sys.stdout.write(json.dumps(reply) + "\n")
        sys.stdout.flush()

    while line := await reader.readline():
        msg = json.loads(line)
        if "id" in msg and "method" in msg:
            asyncio.create_task(answer(msg))


def stats():
    rows = [json.loads(l) for l in LOG.read_text().splitlines()] if LOG.exists() else []
    print(f"{len(rows)} calls in {LOG}")
    by = {}
    for r in rows:
        by.setdefault(r["tool"], []).append(r)
    for tool, rs in sorted(by.items()):
        ms = statistics.median(r["ms"] for r in rs)
        chars = statistics.median(r["chars"] for r in rs)
        errors = sum(r["error"] for r in rs)
        print(f"{tool:8} n={len(rs):4} median {ms:6.0f} ms  ~{chars / 4:6.0f} tokens  errors {errors}")
    synthetic = sum(r.get("synthetic", 0) for r in rows)
    print(f"steps that fell back to synthetic input: {synthetic}")


if __name__ == "__main__":
    if "--stats" in sys.argv:
        stats()
    else:
        asyncio.run(serve())
