#!/usr/bin/env python3
"""A stand-in for Omaweb's bar socket (#906): sends one state line, with the
icons in ./icons as favicons, to every widget that connects. Run with the
socket path as its argument; edit STATE and restart it to try other states."""
import json, os, socket, sys, threading

here = os.path.dirname(os.path.abspath(__file__))
icons = sorted(os.path.join(here, "icons", n) for n in os.listdir(os.path.join(here, "icons")))

def tab(i, title, host, **extra):
    t = {"id": f"t{i}", "title": title, "host": host, "icon": icons[i % len(icons)] if i < 100 else "",
         "sound": "", "poppedOut": False, "loading": False, "agent": False, "agentAct": ""}
    t.update(extra)
    return t

names = [("Issue #906: Show tabs in the Omarchy bar", "github.com"), ("Qt Quick Layouts", "doc.qt.io"),
         ("Quickshell docs", "quickshell.org"), ("Hyprland wiki", "wiki.hypr.land"),
         ("Arch Linux news", "archlinux.org"), ("Rust std::sync", "doc.rust-lang.org"),
         ("Hacker News", "news.ycombinator.com"), ("Weather in Helsinki", "yr.no")]
tabs = [tab(i + 2, title, host) for i, (title, host) in enumerate(names)]
tabs[2]["sound"] = "playing"
tabs[4]["agent"] = True
tabs[4]["agentAct"] = "read the page"
tabs[2]["loading"] = True
tabs[6]["sound"] = "muted"
tabs.append(tab(100, "No icon", "lwn.net"))
STATE = {
    "type": "state", "version": 1, "window": "Page 4 — Research — Omaweb", "activeSpace": "research",
    "spaces": [
        {"id": "personal", "name": "Personal", "colour": "#f7768e", "active": "", "count": 1, "pins": [], "tabs": [tab(30, "Bank", "bank.example")]},
        {"id": "home", "name": "Home", "colour": "#9ece6a", "active": "t20", "count": 2, "pins": [],
         "tabs": [tab(20, "Kernel news", "lwn.net"), tab(21, "Recipes", "food.example")]},
        {"id": "research", "name": "Research", "colour": "#e0af68", "active": "t4", "count": len(tabs) + 2,
         "pins": [tab(0, "Inbox", "mail.example"), tab(1, "Calendar", "cal.example")], "tabs": tabs},
    ],
    "location": {"host": "quickshell.org", "title": "Quickshell docs", "secure": True, "canBack": True, "canForward": False},
    "status": {"downloads": {"running": 0, "progress": 0, "files": []}, "release": None, "recording": False},
}

path = sys.argv[1]
os.makedirs(os.path.dirname(path), mode=0o700, exist_ok=True)
if os.path.exists(path):
    os.unlink(path)
server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
server.bind(path)
server.listen()
conns = []
lock = threading.Lock()

def broadcast():
    line = (json.dumps(STATE) + "\n").encode()
    for c in list(conns):
        try:
            c.sendall(line)
        except OSError:
            conns.remove(c)

def find(tab_id):
    for space in STATE["spaces"]:
        for where in ("pins", "tabs"):
            for t in space[where]:
                if t["id"] == tab_id:
                    return space, where, t
    return None, None, None

def act(message):
    what, tab_id = message.get("do"), message.get("tab")
    space, where, t = find(tab_id) if tab_id else (None, None, None)
    if what == "activate" and t:
        STATE["activeSpace"] = space["id"]
        space["active"] = tab_id
        STATE["location"].update(host=t["host"], title=t["title"])
    elif what in ("mute", "unmute") and t:
        t["sound"] = "muted" if what == "mute" else "playing"
    elif what == "close" and t:
        space[where].remove(t)
        space["count"] -= 1
    elif what == "space":
        STATE["activeSpace"] = message.get("space")
    print("action", message, flush=True)

def serve(conn):
    with lock:
        conns.append(conn)
        conn.sendall((json.dumps(STATE) + "\n").encode())
    buffer = b""
    while True:
        data = conn.recv(4096)
        if not data:
            break
        buffer += data
        while b"\n" in buffer:
            line, buffer = buffer.split(b"\n", 1)
            try:
                with lock:
                    act(json.loads(line))
                    broadcast()
            except ValueError:
                pass
while True:
    conn, _ = server.accept()
    threading.Thread(target=serve, args=(conn,), daemon=True).start()
