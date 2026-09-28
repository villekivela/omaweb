## Spike answer

Prototype on
[`prototype/376-agent-tab-rendering-and-input`](https://github.com/villekivela/omaweb/tree/prototype/376-agent-tab-rendering-and-input/prototypes/376-agent-tab).
It is a standalone Qt Quick program, not Omaweb. One window holds two `WebEngineView`s filling the
same page area: the reader's page on top, the Agent tab under it, each on its own off-the-record
profile so each has its own renderer. Raw results are in `results/`.

Targets:

- Linux: the Omarchy VM. Arch aarch64, Hyprland 0.56.2, Qt 6.11.2 and Omaweb's engine
  (`omaweb-qtwebengine 6.11.2-3`), run with the session's
  `QTWEBENGINE_CHROMIUM_FLAGS=--use-gl=egl --disable-gpu-compositing`. The GPU is virgl, so the
  costs are a VM's, not hardware's.
- macOS dev build: M2 Max, macOS 26.6.2, Homebrew Qt 6.11.1 with stock QtWebEngine, on Metal.

Questions 1 to 3 came out the same on both.

### 1. Rendering behind the page: yes

| Agent tab           | rAF per second     | 10 ms timer per second | `visibilityState` | Window pixel at the Agent's button |
| ------------------- | ------------------ | ---------------------- | ----------------- | ---------------------------------- |
| `visible: false`    | 0                  | 1.0 macOS, 1.9 Linux   | `hidden`          | reader's white                     |
| Visible, behind     | 60 macOS, 57 Linux | 100 macOS, 89 Linux    | `visible`         | reader's white                     |
| Behind at opacity 0 | 60 macOS, 56 Linux | 100 macOS, 84 Linux    | `visible`         | reader's white                     |

A view that is visible but stacked behind renders and runs its timers at full rate, and the reader
sees only their own page. The window grab reads the reader's white at the Agent's button and at the
centre of the page area.

This holds only while Omaweb's window is itself drawing. On Hyprland I moved the window to a
workspace the reader was not on. The Agent page went on running (rAF 53 to 59 a second, timers 82 to
94), but `grabToImage` did not finish within 3 s until the window came back
(`results/linux-unseen.jsonl`). On macOS, the rounds where another window covered the probe had
every renderer at 0% CPU, the Agent tab's included. Omaweb ships only for Linux (ADR 0029), so the
Hyprland behaviour is the one that counts.

### 2. `grabToImage`: yes, while the window draws

A script set the Agent page's background to three colours in turn, the view was grabbed 150 ms
later, and the pixel was checked:

| Agent tab           | Correct | Time per grab                       |
| ------------------- | ------- | ----------------------------------- |
| Visible, behind     | 3 of 3  | 12 to 80 ms macOS, 110 to 250 ms VM |
| Behind at opacity 0 | 3 of 3  | 22 to 90 ms macOS, 265 to 420 ms VM |
| `visible: false`    | 0 of 3  | returns the previous frame          |

The image is in device pixels. While the window is not drawing the grab waits (the Hyprland run
above, and one macOS grab that took 35 s while the window was covered). `shot` therefore needs a
timeout and an error saying the window is not on screen.

### 3. Trusted input through the delegate: yes, and a click takes focus

Every event sent with `QCoreApplication::sendEvent` to the `RenderWidgetHostViewQtDelegateItem`
arrived with `isTrusted: true`: `pointerdown`, `mousedown`, `mouseup`, `click`, `keydown`, `input`,
`keyup`. Typed text reaches the focused field. This works behind, at opacity 0, and even with the
view hidden.

The mouse press is the problem. `RenderWidgetHostViewQtDelegateItem::mousePressEvent` calls
`forceActiveFocus()` when `activeFocusOnPress` is on, and the delegate client calls
`RenderWidgetHostViewQt::Focus()` on every press. With `activeFocusOnPress` off, the delegate drops
the press and no event reaches the page. So a plain click takes Qt's keyboard focus away from the
reader, and the reader's page gets `blur`.

| Variant                                                   | Agent page                                | Reader's page         | Qt focus after |
| --------------------------------------------------------- | ----------------------------------------- | --------------------- | -------------- |
| Plain click                                               | trusted pointer, mouse and click events   | `blur` x2             | Agent tab      |
| Click, then give the reader's focus back in the same turn | same                                      | `blur` x2, `focus` x2 | reader         |
| Quiet click (below)                                       | same                                      | nothing               | reader         |
| Keys to a field focused by script, Qt focus never moved   | trusted keys, text arrives                | nothing               | reader         |
| Keys after a `FocusIn` sent to the Agent's delegate only  | trusted keys, text arrives, `focus` fires | nothing               | reader         |
| Return or Space to a button focused by script             | trusted `click`, no pointer events        | nothing               | reader         |
| The reader types right after a quiet click                | nothing                                   | their text, trusted   | reader         |

The quiet click installs an event filter on every delegate in the window that swallows `FocusIn` and
`FocusOut`, sends the press and release, gives the previous focus item `forceActiveFocus()` again,
and removes the filter, all in one turn of the event loop. Neither page's Chromium hears that focus
moved, and no reader input can arrive in between. Afterwards Chromium believes both pages have
focus. Nothing in the probe broke because of that.

It hides the move from pages, not from QML. With the reader's focus in a `TextInput` standing in for
the Omnibar, a quiet click changed that item's `activeFocus` twice, false and then true, and
anything on `onActiveFocusChanged` would react. So a page verb must not send a mouse press while the
reader's focus is in the interface. It waits, as `do` already waits while the reader's focus is in
the Agent tab, or it activates the element from the keyboard. Removing the focus move altogether
would take an engine change: a press the delegate forwards without `forceActiveFocus()` and
`Focus()`. Version one does not need it.

### 4. Cost

Each state ran for 5 s after a 2 s settle, in three interleaved rounds. CPU is the process tree's,
as a percentage of one core. QtWebEngine runs Chromium's GPU and viz work inside the browser process
on both platforms, so "Browser" includes it along with Qt's scene graph. The reader's page is static
with a focused field, and its caret blinking at about 2 frames a second is the baseline. The Agent
page's loads are: static; a CSS spinner, a compositor animation of the kind a loading page shows;
and a `requestAnimationFrame` canvas that redraws 200 rectangles a frame. "On show" is the same page
in front of the reader's, for comparison.

macOS, M2 Max, window 5028 x 2738 px. Rounds where the window was covered and drew nothing are left
out; `n` is the rounds kept.

| Agent tab              | n   | Frames/s | Agent renderer | Browser | Total  | Qt GPU ms/frame |
| ---------------------- | --- | -------- | -------------- | ------- | ------ | --------------- |
| Frozen, static (today) | 2   | 2        | 0%             | 1%      | 2%     | 0.6-1.1         |
| Hidden, static         | 2   | 2        | 0%             | 1%      | 2%     | 0.5-0.7         |
| Behind, static         | 2   | 2        | 0%             | 1%      | 2%     | 0.5-0.6         |
| Behind, spinner        | 2   | 59-60    | 6-7%           | 18-20%  | 24-28% | 0.5-0.6         |
| Behind, rAF canvas     | 3   | 53-60    | 12-14%         | 22-27%  | 34-41% | 0.6             |
| Opacity 0, static      | 3   | 2-3      | 0%             | 1-2%    | 1-3%   | 0.5-1.7         |
| Opacity 0, spinner     | 3   | 59-60    | 4-8%           | 14-22%  | 18-30% | 0.5-0.7         |
| Opacity 0, rAF canvas  | 3   | 56-60    | 12-13%         | 21-23%  | 34-36% | 0.4-0.5         |
| On show, spinner       | 3   | 48-60    | 6%             | 17-19%  | 24-26% | 0.5-0.9         |
| On show, rAF canvas    | 2   | 60       | 12-14%         | 21-26%  | 34-40% | 0.5             |

Linux VM, 12 vCPUs, virgl, window 2522 x 1372 px, 3 rounds each:

| Agent tab              | Frames/s | Agent renderer | Browser | Total  |
| ---------------------- | -------- | -------------- | ------- | ------ |
| Frozen, static (today) | 2-3      | 0%             | 6-11%   | 7-12%  |
| Hidden, static         | 2-3      | 0%             | 5-11%   | 7-13%  |
| Behind, static         | 2        | 0%             | 6-11%   | 7-12%  |
| Behind, spinner        | 12-18    | 3-10%          | 46-57%  | 53-61% |
| Behind, rAF canvas     | 11-14    | 14-16%         | 40-46%  | 56-63% |
| Opacity 0, static      | 2-3      | 0%             | 3-5%    | 4-6%   |
| Opacity 0, spinner     | 20-26    | 5%             | 37-41%  | 45-46% |
| Opacity 0, rAF canvas  | 16-23    | 19-21%         | 38-46%  | 58-67% |
| On show, spinner       | 12-16    | 3-6%           | 46-50%  | 51-56% |
| On show, rAF canvas    | 10-22    | 14-15%         | 38-50%  | 54-65% |

What the numbers say:

- A static Agent tab costs nothing measurable over today's frozen tab on either platform.
- An animating Agent tab costs about what the same page costs on show. Behind with a spinner is
  24-28% of a core on macOS against 24-26% on show, and 53-61% against 51-56% on the VM.
- It sets the reader's window to the Agent page's frame rate. On macOS the window drew 60 frames a
  second behind a static reader's page, at about 0.5 ms of GPU a frame for the whole window. That is
  Qt's pass only; these tools cannot separate Chromium's own GPU work from the browser process.
- Opacity 0 changes little on Metal. On the VM it cut the browser process from 46-57% to 37-41% with
  a spinner, and from 6-11% to 3-5% when static, because the scene graph no longer composites the
  hidden layer. Grabs and input behave the same at opacity 0, so I would stack the Agent tab behind
  at opacity 0.
- The VM tops out at 12-26 frames a second under load. That is virgl, not the design.

Not measured: Linux hardware, a real site instead of the synthetic loads, and the compositor's share
of the extra frames on Wayland.

### For version one

- A connected Agent tab stays visible to the engine, sized like the page area and stacked under the
  page on show at opacity 0, and is exempt from freezing, as ADR 0051 says.
- Input is trusted Qt events. Keys and wheel go straight to the delegate. Clicks use the quiet click
  and wait while the reader's focus is in the interface.
- `shot` uses `grabToImage` with a timeout for when Omaweb's window is not drawing.
- The marker can tell the reader that an animating Agent tab costs about what that page costs on
  show and makes the window draw at its frame rate.

Trusted input did not fail, so the ADR's fallback to script events does not apply.
