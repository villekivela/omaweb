# Answer the clipboard as Chrome does

Supersedes the part of [0030](0030-refuse-site-capabilities-omaweb-cannot-name.md) that asks about
clipboard read every time and never writes the answer down.

Copy buttons are on every page, and a password manager copies a password many times a day. Asking
about each one taught the reader to approve without reading, and a copy the reader missed failed
silently (#745). Omaweb answers the clipboard the way Chrome does, because a page written for Chrome
expects those answers and the reader already knows them.

## What Chrome does

Read from Chromium 140.0.7339.264, the version QtWebEngine 6.11 is based on:

- A page has to be focused before it may use the asynchronous clipboard at all
  (`third_party/blink/renderer/modules/clipboard/clipboard_promise.cc`, `ValidatePreconditions`).
- Writing text or a known image format during a click or a key press is the
  `CLIPBOARD_SANITIZED_WRITE` permission, which Chrome allows without asking
  (`components/permissions/contexts/clipboard_sanitized_write_permission_context.cc`).
- Reading, and writing without a gesture or in a format Chrome cannot sanitize, is the
  `CLIPBOARD_READ_WRITE` permission (`third_party/blink/common/permissions/permission_utils.cc`).
  Chrome asks, keeps the answer for the site, and lists it in the site's settings, where it can be
  reset (`components/permissions/contexts/clipboard_read_write_permission_context.cc`).
- `document.execCommand("copy")` and `"cut"` write only during a click or a key press
  (`third_party/blink/renderer/core/editing/commands/clipboard_commands.cc`, `CanWriteClipboard`).
- An extension that declares `clipboardRead` reads without a question. One that declares
  `clipboardWrite`, and any page of an extension's own other than its service worker, writes without
  one (`chrome/renderer/chrome_content_settings_agent_delegate.cc`).

## What Omaweb does

Clipboard read is a rememberable capability, beside camera, microphone, location and notifications.
The question offers allow once, persistent allow and block, the answer is kept for the origin in its
Space, and Site information lists it as Clipboard with the reset every other permission has.

The reason 0030 gave for asking every time was that a read hands over whatever is on the clipboard
at that instant. That is still true, and it is the same for a camera, which hands over whatever is
in front of it and is remembered. What makes a standing answer acceptable is that the reader can see
it and take it back, which Site information provides, and that only the page the reader is looking
at may read.

## Writes wait for the engine

QtWebEngine 6.11 reports both of Chrome's clipboard permissions as one `ClipboardReadWrite`
(`src/core/permission_manager_qt.cpp`, `toQt`), so the embedder cannot tell a copy button from a
read. The engine grants the sanitized write by itself only when `JavascriptCanAccessClipboard` is
on, and that setting also lets `execCommand("copy")` write with no gesture at all, which is how a
page fills the clipboard behind the reader's back. Omaweb leaves it off.

The write side is therefore a patch to Omaweb's engine build
([0049](0049-ship-omawebs-own-engine-build.md)): grant `CLIPBOARD_SANITIZED_WRITE` without asking,
as Chrome does, and answer an extension's `clipboardWrite` and `clipboardRead` the way Chrome's
renderer does. Until it lands, a page's copy still arrives as the clipboard question, and the
reader's standing answer for the site covers its copies as well as its reads.
