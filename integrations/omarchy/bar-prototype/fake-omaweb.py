#!/usr/bin/env python3
"""PROTOTYPE, throwaway: a fake omaweb that feeds the bar widget.

It serves the bar socket with made-up Spaces and tabs, takes the widget's
actions, and draws its state in the terminal it runs in. That terminal
stands in for the browser window: the widget counts it as omaweb being
focused, and switching a tab from the bar raises it.

Keys: v variant, n new tab, x close, a Agent, s sound, d download,
o pop out, 1-3 Space, q quit.
"""

import json
import os
import random
import selectors
import socket
import subprocess
import sys
import termios
import threading
import time
import tty
import urllib.request

WINDOW_CLASS = "dev.omaweb.prototype"
RUNTIME = os.path.join(os.environ.get("XDG_RUNTIME_DIR", "/tmp"), "omaweb")
SOCKET_PATH = os.path.join(RUNTIME, "bar-prototype.sock")
ICON_DIR = os.path.join(RUNTIME, "bar-prototype")

POOL = [
    ("lwn.net", "LWN.net: Kernel development"),
    ("doc.qt.io", "QML Socket Type | Qt 6.11"),
    ("archlinux.org", "Arch Linux - News"),
    ("github.com", "Issues · basecamp/omarchy"),
    ("en.wikipedia.org", "Wayland (protocol) - Wikipedia"),
    ("developer.mozilla.org", "View Transition API - Web APIs | MDN"),
    ("news.ycombinator.com", "Hacker News"),
    ("stackoverflow.com", "QLocalSocket readyRead not emitted - Stack Overflow"),
]

next_id = 0


def tab(host, title, **marks):
    global next_id
    next_id += 1
    return {
        "id": next_id,
        "host": host,
        "title": title,
        "url": f"https://{host}/",
        "icon": "",
        "audio": marks.get("audio", ""),
        "agent": marks.get("agent", False),
        "agentAct": marks.get("agentAct", ""),
        "poppedOut": marks.get("poppedOut", False),
        "back": marks.get("back", 2),
        "forward": 0,
    }


spaces = [
    {
        "id": "work",
        "name": "work",
        "colour": "#7aa2f7",
        "pins": [
            tab("github.com", "GitHub"),
            tab("mail.google.com", "Inbox (3) - Gmail"),
            tab("calendar.google.com", "Google Calendar - Week of 26 October"),
        ],
        "tabs": [
            tab("github.com", "feat: reveal the theme diagonally (#905) · villekivela/omaweb"),
            tab("doc.qt.io", "Qt Quick QML Types | Qt 6.11"),
            tab("github.com", "omarchy/shell/plugins/bar at quattro"),
            tab("quickshell.org", "Socket | Quickshell"),
            tab("wiki.hypr.land", "Dispatchers – Hyprland Wiki", agent=True, agentAct="read the page"),
            tab("www.youtube.com", "lofi hip hop radio - beats to relax/study to", audio="playing"),
        ],
    },
    {
        "id": "personal",
        "name": "personal",
        "colour": "#9ece6a",
        "pins": [tab("www.youtube.com", "YouTube"), tab("www.reddit.com", "Reddit - The heart of the internet")],
        "tabs": [
            tab("news.ycombinator.com", "Hacker News"),
            tab("en.wikipedia.org", "Tiling window manager - Wikipedia"),
            tab("archlinux.org", "Arch Linux"),
        ],
    },
    {
        "id": "reading",
        "name": "reading",
        "colour": "#e0af68",
        "pins": [],
        "tabs": [
            tab("lwn.net", "The rest of the 6.18 merge window [LWN.net]"),
            tab("martinfowler.com", "Is High Quality Software Worth the Cost?"),
            tab("www.inkandswitch.com", "Local-first software", poppedOut=True),
            tab("danluu.com", "Files are hard"),
            tab("blog.codinghorror.com", "The Best Code is No Code At All"),
        ],
    },
]
active_space = "work"
VARIANTS = ["capsule", "labelled", "spaces"]
variant = VARIANTS[0]
active_tab = {s["id"]: (s["tabs"][0]["id"] if s["tabs"] else None) for s in spaces}
downloads = []
clients = {}
last_action = "started"
icons = {}


def find(tab_id):
    for s in spaces:
        for group in ("pins", "tabs"):
            for i, t in enumerate(s[group]):
                if t["id"] == tab_id:
                    return s, group, i, t
    return None, None, None, None


def current():
    s = next(s for s in spaces if s["id"] == active_space)
    _, _, _, t = find(active_tab.get(active_space))
    return s, t


def snapshot():
    def view(t):
        return dict(t, icon=icons.get(t["host"], ""))

    s, t = current()
    return {
        "type": "state",
        "variant": variant,
        "activeSpace": active_space,
        "spaces": [
            {
                "id": sp["id"],
                "name": sp["name"],
                "colour": sp["colour"],
                "active": active_tab.get(sp["id"]),
                "count": len(sp["pins"]) + len(sp["tabs"]),
                "pins": [view(x) for x in sp["pins"]],
                "tabs": [view(x) for x in sp["tabs"]],
            }
            for sp in spaces
        ],
        "nav": {
            "host": t["host"] if t else "",
            "secure": True,
            "canBack": bool(t and t["back"] > 0),
            "canForward": bool(t and t["forward"] > 0),
        },
        "status": {
            "downloads": [dict(d) for d in downloads],
        },
    }


def broadcast():
    line = (json.dumps(snapshot()) + "\n").encode()
    for conn in list(clients):
        try:
            conn.sendall(line)
        except OSError:
            drop(conn)
    render()


def raise_window():
    subprocess.Popen(
        ["omarchy-launch-or-focus", WINDOW_CLASS, "true"],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )


def activate(tab_id):
    global active_space
    s, _, _, t = find(tab_id)
    if not t:
        return
    active_space = s["id"]
    active_tab[s["id"]] = tab_id
    if t["poppedOut"]:
        return f"raise the Tab window of \"{t['title']}\""
    return f"switch to \"{t['title']}\""


def close(tab_id):
    s, group, i, t = find(tab_id)
    if not t:
        return
    del s[group][i]
    if active_tab.get(s["id"]) == tab_id:
        rest = s["tabs"] or s["pins"]
        active_tab[s["id"]] = rest[min(i, len(rest) - 1)]["id"] if rest else None
    return f"close \"{t['title']}\""


def handle(message):
    global active_space
    act = message.get("do")
    tab_id = message.get("tab")
    s, t = current()
    if act == "activate":
        result = activate(tab_id)
        raise_window()
    elif act == "close":
        result = close(tab_id)
    elif act == "cycle":
        order = s["pins"] + s["tabs"]
        if not order:
            return
        ids = [x["id"] for x in order]
        i = ids.index(t["id"]) if t and t["id"] in ids else 0
        active_tab[s["id"]] = ids[(i + int(message.get("step", 1))) % len(ids)]
        result = f"cycle to \"{find(active_tab[s['id']])[3]['title']}\""
    elif act == "space":
        active_space = message.get("space", active_space)
        result = f"switch to Space {active_space}"
        raise_window()
    elif act in ("pin", "unpin"):
        sp, group, i, x = find(tab_id)
        if not x:
            return
        del sp[group][i]
        if act == "pin":
            sp["pins"].append(x)
        else:
            sp["tabs"].insert(0, x)
        result = f"{act} \"{x['title']}\""
    elif act == "duplicate":
        sp, group, i, x = find(tab_id)
        if not x:
            return
        copy = tab(x["host"], x["title"])
        sp["tabs"].insert(i + 1 if group == "tabs" else 0, copy)
        active_tab[sp["id"]] = copy["id"]
        result = f"duplicate \"{x['title']}\""
    elif act == "popout":
        _, _, _, x = find(tab_id)
        if not x:
            return
        x["poppedOut"] = not x["poppedOut"]
        result = ("pop out" if x["poppedOut"] else "put back") + f" \"{x['title']}\""
    elif act in ("back", "forward") and t:
        if act == "back" and t["back"]:
            t["back"] -= 1
            t["forward"] += 1
        elif act == "forward" and t["forward"]:
            t["forward"] -= 1
            t["back"] += 1
        result = f"go {act} on \"{t['title']}\""
    elif act == "reload" and t:
        result = f"reload \"{t['title']}\""
    elif act == "omnibar":
        result = "open the Omnibar (in the browser window)"
        raise_window()
    else:
        result = f"unknown action {message!r}"
    note(f"bar: {result}")
    broadcast()


def note(text):
    global last_action
    last_action = text


def key(ch):
    global active_space, variant
    s, t = current()
    if ch in ("q", ""):
        raise SystemExit
    if ch == "v":
        variant = VARIANTS[(VARIANTS.index(variant) + 1) % len(VARIANTS)]
        note(f"key: variant {variant}")
    elif ch == "n":
        host, title = random.choice(POOL)
        new = tab(host, title, back=0)
        s["tabs"].append(new)
        active_tab[s["id"]] = new["id"]
        fetch_icons([host])
        note(f"key: new tab \"{title}\"")
    elif ch == "x" and t:
        note("key: " + close(t["id"]))
    elif ch == "a" and t:
        t["agent"] = not t["agent"]
        t["agentAct"] = "clicked \"Sign in\"" if t["agent"] else ""
        note(f"key: Agent {'on' if t['agent'] else 'off'}")
    elif ch == "s" and t:
        t["audio"] = {"": "playing", "playing": "muted", "muted": ""}[t["audio"]]
        note(f"key: sound {t['audio'] or 'off'}")
    elif ch == "d":
        downloads.append({"name": random.choice(["omarchy-4.1.iso", "report.pdf", "qt-6.12.tar.xz"]), "progress": 0})
        note("key: start a download")
    elif ch == "o" and t:
        t["poppedOut"] = not t["poppedOut"]
        note(f"key: {'pop out' if t['poppedOut'] else 'put back'}")
    elif ch in "123":
        active_space = spaces[int(ch) - 1]["id"]
        note(f"key: Space {active_space}")
    else:
        return
    broadcast()


def tick():
    changed = False
    for d in downloads:
        if d["progress"] < 100:
            d["progress"] = min(100, d["progress"] + random.randint(4, 12))
            changed = True
    done = [d for d in downloads if d["progress"] >= 100]
    if done and not changed:
        for d in done:
            d.setdefault("doneAt", time.time())
        downloads[:] = [d for d in downloads if d["progress"] < 100 or time.time() - d["doneAt"] < 8]
        changed = True
    return changed


def fetch_icons(hosts):
    def work(host):
        if host in icons:
            return
        try:
            req = urllib.request.Request(f"https://{host}/favicon.ico", headers={"User-Agent": "Mozilla/5.0"})
            data = urllib.request.urlopen(req, timeout=4).read()
        except Exception:
            return
        if data[:8] == b"\x89PNG\r\n\x1a\n":
            ext = "png"
        elif data[:4] == b"\x00\x00\x01\x00":
            ext = "ico"
        elif b"<svg" in data[:200]:
            ext = "svg"
        else:
            return
        path = os.path.join(ICON_DIR, f"{host}.{ext}")
        with open(path, "wb") as f:
            f.write(data)
        icons[host] = path
        wake_w.send(b"i")

    for h in hosts:
        threading.Thread(target=work, args=(h,), daemon=True).start()


def drop(conn):
    sel.unregister(conn)
    clients.pop(conn, None)
    conn.close()


def render():
    s, t = current()
    out = ["\x1b[2J\x1b[H"]
    out.append("\x1b[2m FAKE OMAWEB — PROTOTYPE. This terminal stands in for the browser window.\x1b[0m\n\n")
    others = " · ".join(f"{sp['name']} {len(sp['pins']) + len(sp['tabs'])}" for sp in spaces if sp["id"] != active_space)
    out.append(f" Space  \x1b[1m{s['name']}\x1b[0m   \x1b[2m({others})\x1b[0m\n")
    if t:
        out.append(f" Page   \x1b[1m{t['title']}\x1b[0m\n        {t['url']}\n")
    else:
        out.append(" Page   (no tab)\n")
    out.append("\n")
    for group in ("pins", "tabs"):
        for x in s[group]:
            mark = "▸" if t and x["id"] == t["id"] else " "
            extra = "".join(
                [
                    " [agent]" if x["agent"] else "",
                    f" [{x['audio']}]" if x["audio"] else "",
                    " [popped out]" if x["poppedOut"] else "",
                ]
            )
            label = "pin " if group == "pins" else "tab "
            out.append(f"  {mark} \x1b[2m{label}\x1b[0m{x['title'][:70]}\x1b[2m{extra}\x1b[0m\n")
    if downloads:
        out.append("\n downloads: " + ", ".join(f"{d['name']} {d['progress']}%" for d in downloads) + "\n")
    out.append(f"\n variant \x1b[1m{variant}\x1b[0m  ({' · '.join(VARIANTS)}; v switches)\n")
    out.append(f" last   {last_action}\n")
    out.append(f" bar    {len(clients)} widget(s) connected   \x1b[2m{SOCKET_PATH}\x1b[0m\n")
    out.append("\n\x1b[2m v variant · n new tab · x close · a Agent · s sound · d download · o pop out · 1-3 Space · q quit\x1b[0m\n")
    sys.stdout.write("".join(out))
    sys.stdout.flush()


os.makedirs(ICON_DIR, mode=0o700, exist_ok=True)
if os.path.exists(SOCKET_PATH):
    os.unlink(SOCKET_PATH)
server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
server.bind(SOCKET_PATH)
os.chmod(SOCKET_PATH, 0o600)
server.listen()
server.setblocking(False)

wake_r, wake_w = socket.socketpair()
sel = selectors.DefaultSelector()
sel.register(server, selectors.EVENT_READ, "accept")
sel.register(wake_r, selectors.EVENT_READ, "wake")
sel.register(sys.stdin, selectors.EVENT_READ, "key")

old = termios.tcgetattr(sys.stdin)
tty.setcbreak(sys.stdin)
sys.stdout.write("\x1b]2;Fake Omaweb\x07\x1b[?25l")
fetch_icons(sorted({x["host"] for s in spaces for g in ("pins", "tabs") for x in s[g]}))
render()
try:
    next_tick = time.time() + 0.5
    while True:
        for k, _ in sel.select(timeout=max(0, next_tick - time.time())):
            if k.data == "accept":
                conn, _ = server.accept()
                conn.setblocking(False)
                clients[conn] = b""
                sel.register(conn, selectors.EVENT_READ, "client")
                note("bar: a widget connected")
                broadcast()
            elif k.data == "wake":
                wake_r.recv(64)
                broadcast()
            elif k.data == "key":
                key(sys.stdin.read(1))
            else:
                conn = k.fileobj
                try:
                    chunk = conn.recv(4096)
                except OSError:
                    chunk = b""
                if not chunk:
                    drop(conn)
                    note("bar: a widget disconnected")
                    render()
                    continue
                clients[conn] += chunk
                while b"\n" in clients[conn]:
                    line, clients[conn] = clients[conn].split(b"\n", 1)
                    try:
                        handle(json.loads(line))
                    except (ValueError, AttributeError):
                        pass
        if time.time() >= next_tick:
            next_tick = time.time() + 0.5
            if tick():
                broadcast()
except (SystemExit, KeyboardInterrupt):
    pass
finally:
    termios.tcsetattr(sys.stdin, termios.TCSADRAIN, old)
    sys.stdout.write("\x1b[?25h\n")
    os.unlink(SOCKET_PATH)
