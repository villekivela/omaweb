# Choose files through the desktop's file chooser portal

Every file and folder dialog Omaweb opens asks `org.freedesktop.portal.FileChooser` first, and opens
Qt's dialog only when no portal answers (#868).

## Why Qt's dialog was the wrong one

Omarchy sets `QT_QPA_PLATFORMTHEME=gtk3` for every application. Under that theme Qt opens GTK3's
`GtkFileChooserDialog` inside the process and never asks the portal. A reader who set a file chooser
for the desktop sets it the standard way, as the portal's `FileChooser` backend in `portals.conf`,
and Chromium and Firefox ask the portal, so they showed it. Omaweb showed GTK's.

## What was not done

- **Changing the platform theme.** `QT_QPA_PLATFORMTHEME=xdgdesktopportal` in Omaweb's own
  environment would route Qt's dialogs through the portal, but it replaces the whole theme: fonts,
  palette and icon lookup come from the theme plugin, and the GTK theme is the one Omarchy chose for
  every Qt application. The issue rules out changing it to get a dialog.
- **Qt's portal dialog helper.** `QXdgDesktopPortalFileDialog` is private Qt, created only by the
  themes that wrap it, and takes no page `accept` list. Reaching it would add a private dependency
  for less than the portal call itself needs.

## What Omaweb does

`FileChooser` (`src/platform/LinuxFileChooser.cpp`) calls `OpenFile` or `SaveFile` and hears the
reader's answer as `Response` on the request it names. It listens on the request's path before it
asks, because the specification makes that path predictable so an answer cannot arrive unheard.

- A folder is `OpenFile` with `directory`, and several files are `OpenFile` with `multiple`.
- A page's `accept` list is the first filter and the one the dialog opens on: MIME types as MIME
  patterns, `image/*` included, and `.ext` suffixes as globs that match either case,
  `*.[pP][dD][fF]` for `.pdf`. "All files" follows it, so the reader can still widen the choice.
  Chrome does both.
- Omaweb's own dialogs keep Qt's filter strings, such as `Text files (*.txt)`, and the portal is
  given the same filters.
- A save starts in the folder and under the name the shell suggests, through `current_folder` and
  `current_name`.
- The dialog is parented to the window that asked, by the name `PortalWindow.h` already gives the
  print portal: `wayland:<handle>` from Qt's own xdg-foreign export, or `x11:<xid>`.
- Cancelling the dialog cancels the page's request. A page that goes away while its dialog is open
  closes the request through `org.freedesktop.portal.Request.Close`, and nothing is reported.

`FilePicker.qml` is the one component the shell uses for a dialog. It asks `FileChooser`, and on
`false` opens Qt's `FileDialog` or `FolderDialog` with the same title, mode and Qt filter strings. A
page's `accept` list reaches only the portal, because Qt's filters take globs and not MIME types.
False means the call itself failed: no session bus, no portal, or a portal with no `FileChooser`
backend. The fallback dialog is the one Qt's theme gives, which on macOS is the system's own panel,
so the macOS build asks no portal at all.

The call blocks until the portal has the request, for at most five seconds, as printing's does. The
portal returns before the reader answers, so the wait is the portal starting, once. A portal that
has not answered by then may still show its dialog, so the call counts as open rather than failed,
and the answer is heard on the request's path. Opening Qt's dialog then would put two in front of
the reader.

## Consequences

- The portal is reached with `Qt6::DBus`, which the platform layer already links. Nothing new is
  added.
- `omaweb-file-chooser-portal` stands up a stub portal on a bus started from
  `tests/platform/fixtures/bare-session-bus.conf`, which activates no service. A standard session
  bus would start the real portal once the stub is gone, and the real portal opens a real dialog.
- The UI suites run without a session bus, so they cover the fallback.
