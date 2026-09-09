"""A browser under test on a live Wayland session, and the tools to drive it.

Three checks share this: the session sweep, the theme repaint, and the default
browser. Each drives a browser nobody is reading, on its own session bus and
its own throwaway directories, so it browses nothing of the reader's and does
not hand its arguments to the browser they already have open.

What goes in is the compositor's own dispatcher: `hl.dsp.send_shortcut` for a
keystroke, `hl.dsp.cursor.move` for the cursor, and `ydotool` for the one thing
Hyprland has no dispatcher for, a pointer button. What comes back is read from
`hyprctl clients`, `wl-paste`, the window's title, and where a check says so,
the Wayland protocol the browser spoke.

Reading the screen is left to the callers that cannot avoid it. A check that
looks at pixels has to capture a window, and a window belongs to whoever is
sitting at the desk, so the scripts that do it say why and capture no more than
the window under test.

Hyprland tiles. A tiled window is placed by the layout, does not follow a
pointer, and answers `xdg_toplevel.move` as the layout sees fit, so a check
about a window's own geometry has to float it first and hand it back after.
"""

from __future__ import annotations

import io
import json
import os
import shutil
import signal
import subprocess
import time

# Long enough for the shell to answer a command and the compositor to settle,
# short enough that eighty of them is a wait rather than a coffee break.
SETTLE = 0.8


# A page load crosses a process boundary and reads a file, so it gets its own.
LOAD_SETTLE = 3.0


# A pointer step is one compositor event rather than a command, so it settles
# faster than a keystroke does.
POINTER_SETTLE = 0.15


# ydotool spells a left button press and release as these bits of one code.
BUTTON_LEFT_DOWN = "0x40"


BUTTON_LEFT_UP = "0x80"


PROBE_PAGE = """<!doctype html>
<meta charset="utf-8">
<title>Omaweb session probe</title>
<h1>session probe</h1>
<p>omaweb-round-trip-token</p>
"""


# What the keybindings file writes against what the compositor calls the same
# key. Everything not named here is already the keysym.
KEYSYMS = {
    "]": "bracketright",
    "[": "bracketleft",
    "=": "equal",
    "-": "minus",
    ".": "period",
    "/": "slash",
    ",": "comma",
    ":": "colon",
    "?": "question",
}


class SessionError(RuntimeError):
    """The desktop cannot run these checks, which is not a failed check."""


def hyprctl(*arguments: str) -> str:
    result = subprocess.run(
        ["hyprctl", *arguments], capture_output=True, text=True, check=False
    )
    return result.stdout.strip()


def dispatch(expression: str) -> None:
    hyprctl("dispatch", expression)


def clients() -> list[dict]:
    return json.loads(hyprctl("clients", "-j") or "[]")


def window_of(pid: int) -> dict | None:
    for client in clients():
        if client.get("pid") == pid:
            return client
    return None


def require_session() -> None:
    if os.environ.get("XDG_SESSION_TYPE") != "wayland":
        raise SessionError("this drives a live Wayland session, and this is not one")
    for tool in ("hyprctl", "wl-copy", "wl-paste"):
        if shutil.which(tool) is None:
            raise SessionError(f"{tool} is not installed")
    if not hyprctl("version"):
        raise SessionError("Hyprland is not answering, and its dispatcher is the keyboard here")


def clipboard(primary: bool = False) -> str:
    arguments = ["wl-paste", "-n"] + (["--primary"] if primary else [])
    result = subprocess.run(arguments, capture_output=True, text=True, check=False)
    return result.stdout


def set_clipboard(value: str) -> None:
    # `wl-copy` daemonises, because a Wayland selection is served by the
    # process that owns it rather than stored by the compositor. It inherits
    # this script's own streams doing so, and a script whose output is piped
    # anywhere then never reaches end of file: the reader waits on a writer
    # that intends to outlive it. So it is handed streams of its own.
    subprocess.run(
        ["wl-copy", value],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        check=False,
    )


class Browser:
    """A second browser, its own session bus, its own throwaway directories."""

    def __init__(
        self,
        executable: str,
        root: str,
        protocol_log: str | None = None,
        private_bus: bool = True,
    ) -> None:
        self.executable = executable
        self.root = root
        self.protocol_log = protocol_log
        # A private bus is what keeps a check off the browser the reader has
        # open. The one exception is the check about handing a link over, which
        # happens on the session bus: on a private one the browser under test
        # claims a name nothing else can see, so a second launch finds the name
        # free, becomes the browser that answers, and the handover being tested
        # cannot happen at all.
        self.private_bus = private_bus
        self.sink: io.TextIOWrapper | None = None
        self.process: subprocess.Popen | None = None
        self.pid = 0

    def start(self, url: str) -> None:
        environment = dict(os.environ)
        environment["OMAWEB_DATA_ROOT"] = os.path.join(self.root, "data")
        environment["OMAWEB_CONFIG_ROOT"] = os.path.join(self.root, "config")
        if self.protocol_log:
            environment["WAYLAND_DEBUG"] = "1"
        # A private bus is what stops this handing its argument to the browser
        # the reader already has open and exiting, which is the whole of what
        # `RunningBrowser` is for and exactly wrong here.
        sink = open(self.protocol_log, "w", encoding="utf-8") if self.protocol_log else None
        self.sink = sink
        launcher = ["dbus-run-session", "--"] if self.private_bus else []
        self.process = subprocess.Popen(
            [*launcher, self.executable, url],
            env=environment,
            stdout=sink or subprocess.DEVNULL,
            stderr=subprocess.STDOUT if sink else subprocess.DEVNULL,
            start_new_session=True,
        )
        for _ in range(60):
            time.sleep(0.5)
            for client in clients():
                if client.get("class") == "omaweb" and self._owned(client.get("pid", 0)):
                    self.pid = client["pid"]
                    time.sleep(LOAD_SETTLE)
                    return
        raise SessionError("the browser under test never mapped a window")

    def _owned(self, pid: int) -> bool:
        # The reader's own browser is an `omaweb` window too, and driving that
        # one would be driving their session rather than a test.
        assert self.process is not None
        if pid == self.process.pid:
            return True
        try:
            with open(f"/proc/{pid}/stat", encoding="utf-8") as handle:
                parent = int(handle.read().rsplit(")", 1)[1].split()[1])
        except (OSError, IndexError, ValueError):
            return False
        return parent == self.process.pid

    def focus(self) -> None:
        # Wayland lets a client set the selection only while it holds keyboard
        # focus, so every clipboard check needs this and not only the pointer.
        dispatch(f'hl.dsp.focus{{window="pid:{self.pid}"}}')
        time.sleep(SETTLE)

    def send(self, modifiers: str, key: str) -> None:
        dispatch(
            f'hl.dsp.send_shortcut{{mods="{modifiers}", '
            f'key="{key}", window="pid:{self.pid}"}}'
        )
        time.sleep(SETTLE)

    def press(self, binding: str) -> None:
        parts = binding.split("+")
        key = parts[-1]
        modifiers = " ".join(
            {"Primary": "CTRL", "Shift": "SHIFT", "Alt": "ALT"}.get(part, part.upper())
            for part in parts[:-1]
        )
        self.send(modifiers, KEYSYMS.get(key, key))

    def title(self) -> str:
        window = window_of(self.pid)
        return window["title"] if window else ""

    def size(self) -> list[int]:
        window = window_of(self.pid)
        return window["size"] if window else []

    def position(self) -> list[int]:
        window = window_of(self.pid)
        return window["at"] if window else []

    def floating(self) -> bool:
        window = window_of(self.pid)
        return bool(window.get("floating")) if window else False

    def float_window(self) -> None:
        dispatch(f'hl.dsp.window.float{{window="pid:{self.pid}"}}')
        time.sleep(SETTLE)
        self.settle()

    def reshape(self, width: int, height: int) -> bool:
        """Puts the window at an exact size, centred, and says whether it took.

        The edges cannot be judged from whatever the layout happened to hand
        the window. Floated, it tends to arrive at Omaweb's own minimum size,
        where a drag inward is refused, and a window tiled across the output
        arrives with its right and bottom edges on the screen's boundary,
        where a drag outward is clamped. Neither reads as a failed resize.
        """
        window = f'window="pid:{self.pid}"'
        dispatch(f"hl.dsp.window.resize{{x={width},y={height},exact=true,{window}}}")
        dispatch(f"hl.dsp.window.center{{{window}}}")
        time.sleep(SETTLE)
        return self.settle() == [width, height]

    def settle(self, tries: int = 20) -> list[int]:
        """Waits for the geometry to stop changing, and returns it.

        Leaving the layout is animated, so a size read straight afterwards is
        a frame of that animation. An edge check that subtracted one of those
        from the next would report the animation as its own result.
        """
        previous = self.size()
        for _ in range(tries):
            time.sleep(POINTER_SETTLE)
            current = self.size()
            if current and current == previous:
                return current
            previous = current
        return previous

    def fullscreen(self) -> int:
        window = window_of(self.pid)
        return window.get("fullscreen", 0) if window else 0

    def requests(self, name: str) -> int:
        if not self.protocol_log or not os.path.exists(self.protocol_log):
            return 0
        with open(self.protocol_log, encoding="utf-8", errors="replace") as handle:
            return sum(1 for line in handle if f"-> xdg_toplevel" in line and f".{name}(" in line)

    def alive(self) -> bool:
        return window_of(self.pid) is not None

    def stop(self) -> None:
        """Ends the whole process group and waits for the window to go.

        Terminating the launcher alone leaves the engine's own zygote and
        renderer processes running, holding their memory and their window. On
        a machine with a few gigabytes that is enough for the next browser
        this script starts to be killed by the kernel instead of by us, so
        this reaps the group and does not return while a window remains.
        """
        if self.process:
            for stage in (signal.SIGTERM, signal.SIGKILL):
                if self.process.poll() is not None and not self.alive():
                    break
                try:
                    os.killpg(os.getpgid(self.process.pid), stage)
                except (ProcessLookupError, PermissionError):
                    pass
                try:
                    self.process.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    pass
                for _ in range(20):
                    if not self.alive():
                        break
                    time.sleep(0.25)
        if self.sink:
            self.sink.close()
            self.sink = None


class Pointer:
    """The cursor from Hyprland, the button from ydotool.

    Hyprland's `cursor.move` takes a layout coordinate and lands on it exactly,
    so nothing here has to know the output's scale. What it has no dispatcher
    for is a button, which is the whole reason the frameless regions went
    unchecked until now.
    """

    def __init__(self) -> None:
        if shutil.which("ydotool") is None:
            raise SessionError(
                "ydotool is not installed, and a frameless region needs a button pressed"
            )
        self.to(0, 0)
        if hyprctl("cursorpos") != "0, 0":
            raise SessionError("Hyprland did not move the cursor, so a region cannot be pressed")

    def to(self, x: float, y: float) -> None:
        dispatch(f"hl.dsp.cursor.move{{x={round(x)},y={round(y)}}}")
        time.sleep(POINTER_SETTLE)

    def _button(self, code: str) -> None:
        result = subprocess.run(["ydotool", "click", code], capture_output=True, text=True)
        if result.returncode != 0:
            # A daemon that is not running or a uinput module that is not
            # loaded both land here, and neither is a failed check.
            raise SessionError(f"ydotool could not press a button: {result.stderr.strip()}")
        time.sleep(POINTER_SETTLE)

    def drag(self, start: tuple[float, float], end: tuple[float, float], steps: int = 10) -> None:
        self.to(*start)
        self._button(BUTTON_LEFT_DOWN)
        try:
            # Stepped rather than jumped: one large motion can arrive before
            # the press is handled, and a grab that has not begun ignores it.
            for step in range(1, steps + 1):
                fraction = step / steps
                self.to(
                    start[0] + (end[0] - start[0]) * fraction,
                    start[1] + (end[1] - start[1]) * fraction,
                )
        finally:
            self._button(BUTTON_LEFT_UP)
        time.sleep(SETTLE)


class Report:
    def __init__(self) -> None:
        self.results: list[tuple[bool, str, str]] = []

    def check(self, passed: bool, name: str, detail: str = "") -> None:
        self.results.append((passed, name, detail))
        print(f"{'pass' if passed else 'FAIL'}  {name}{'  — ' + detail if detail else ''}")

    def failed(self) -> int:
        return sum(1 for passed, _, _ in self.results if not passed)


