# Pop a tab out instead of opening windows

Omaweb has one main window. The requirements said so from the start, without a reason, and later
decisions assume it: a tab belongs to a Space and not to a window
([0040](0040-show-a-second-tab-beside-the-active-one.md)), only the active Space keeps its pages
running ([0033](0033-stop-an-away-spaces-pages-instead-of-taking-them.md)), and a window is either
the main window or a Private one ([0036](0036-name-what-a-window-may-do.md)). The reason is that a
Space does the job a window does in Chrome. A reader keeps work apart by Space, with logins kept
apart too, rather than by arranging windows.

A reader with two monitors still wants a page on the second one, logged in with the same Space
(#653). The Split shows two tabs side by side, but inside the one window, so it cannot span
monitors.

## A Tab window

A tab can pop out into a Tab window: a window of its own that shows that one tab instead of the main
window, with no sidebar. The tab stays an ordinary tab of its Space. It is listed in the Space's
sidebar with a mark, saved with the session and synced as any tab is. Being popped out belongs to
this machine and is not synced.

- Its page keeps running whichever Space the main window shows, as a Keep active tab's does, and
  Omaweb names it among the retained tabs.
- Popping out moves the live page, so its scroll, form input and media carry over. Glance already
  moves a live page within one window.
- Closing the window puts the tab back in the sidebar, because closing a window does not lose a tab.
  Close-tab closes it. Deleting its Space closes the window.
- It comes back popped out after a restart. Omaweb saves no position or size; the compositor's
  window rules place it.
- A link that asks for a new tab follows the main window's rule inside the Tab window: a Glance over
  the page, a new Tab window when the Glance is kept or turned off, and a background tab in the main
  window's sidebar without a switch of Space or focus.
- A slim strip shows the address, the site information and the Space. The reader can hide it per
  window, and it returns on a change of site and when a prompt needs it, so a page never changes
  site out of sight.
- The window opens as `omaweb-tab` for the compositor, so a Hyprland rule can send it to another
  monitor. Qt sends one Wayland app id for every window, so this goes through Qt's private Wayland
  window interface behind an exact Qt version check, as the portal window does. Where that check
  fails, the window opens titled "Omaweb tab window", which a rule can match as its initial title.

The main window answers for everything else. Desktop links, `omaweb` commands and Agents keep their
one target, Sync keeps tabs per Space, and the Split, Glance, downloads and the announced media
player keep their one-window answers. A Tab window's tab is never the main window's active tab, its
prompts show in the Tab window, and its downloads count in the main window's Download mark
([0042](0042-own-a-windows-downloads-in-core.md)).

## What was rejected

- **More main windows, as in Chrome.** Every window would list the same Spaces with tabs of its own.
  The session store would need a window for every tab and a migration of every saved session. Two
  windows on two Spaces would mean two active Spaces. Sync would have to say what a tab in a second
  window means on another machine. Desktop links, the CLI and Agents would each need a rule for
  choosing a window. The Split, Glance, downloads and the media player would each need an answer per
  window. The reporter's use, one page on a second monitor, needs none of it.
- **A second main window held to one Space.** It brings back the choice of a target window and two
  active Spaces for a smaller gain than a Tab window.
- **Loading the address again in the new window.** It loses what the page holds. If moving the live
  page between windows proves unsafe, the design comes back for a decision rather than falling back
  to this.

See #653. Web apps launched with `--app` would build on Tab windows (#655).
