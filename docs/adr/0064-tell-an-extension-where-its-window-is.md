# Tell an extension where its window is

Widens what [0049](0049-ship-omawebs-own-engine-build.md) and
[0054](0054-read-a-pages-certificate-from-the-engine.md) say the engine patch series is for.

Chrome's `chrome.windows` reports each window's `left`, `top`, `width` and `height`, and extensions
place windows of their own from them. Bitwarden opens its passkey prompt at
`window.left + window.width - popupWidth - 15`. The series answered `windows.get` and its siblings
with one window carrying none of the four, so that sum was `NaN`, the schema refused the
`windows.create` that followed, and passkeys through Bitwarden failed on every site
([#561](https://github.com/villekivela/omaweb/issues/561),
[#684](https://github.com/villekivela/omaweb/issues/684)).

## The application says where the window is

The engine cannot know which window a page is in. Omaweb draws a tab's view in the main window or
lends it to a Tab window ([0062](0062-pop-a-tab-out-instead-of-opening-windows.md)), and the engine
sees a view, not the window around it. Making the numbers up from the view's size would put
Bitwarden's prompt in the wrong place as soon as the view is not the whole window.

A patch in the series adds `QWebEngineExtensionManager::setWindowGeometryProvider`. The engine calls
it with the page the call came from, a `QWebEnginePage` or a `WebEngineView`, or with null for a
call no page is behind, such as one from an extension's worker. Omaweb's `QtExtensionWindows`
answers with the frame of the window the view is drawn in, and with the main window for no page or
for a view drawn nowhere. Every answer that carries a window carries the four numbers,
`windows.create`'s and populated ones included.

Wayland tells a client nothing about where its window is, so there `left` and `top` are `0` and only
the size is real. Read from Bitwarden's code, it sets no position for a prompt when both are `0`,
and the compositor places it.

## Still one window

The engine still reports a single `chrome.windows` window per profile, as before. A Tab window is
not a window of its own to an extension: a call from a tab popped out into one is answered with that
Tab window's place, under the same window id as the main window. Giving each Omaweb window its own
id, focus and `windows.onFocusChanged` is a larger change no extension has needed yet.

## Without the patch

The build reads the patch from the engine's headers, as it does the DNS aliases and certificate
chain patches. An Omaweb built against an engine without it sets no provider. An engine with the
patch and no provider reports `0, 0` and the size of the asking view, or the active tab's for a
worker, so an extension always gets four integers.
