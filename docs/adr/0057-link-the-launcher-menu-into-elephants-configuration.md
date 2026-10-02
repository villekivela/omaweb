# Link the launcher menu into Elephant's configuration

Extends [0051](0051-hand-the-browser-to-an-agent.md): its browser commands, `tabs` and `focus`, are
what the launcher menu runs.

A reader on Omarchy opens things from Walker, and their tabs are in Omaweb. Walker lists nothing of
a browser, so finding a tab means leaving the launcher, focusing the window and searching there. A
menu that lists every tab of every Space, and takes the reader to the one they pick, closes that gap
with what 0051 already built: `omaweb tabs --all --json` lists them and `omaweb focus <id>` selects
one.

Walker takes its entries from Elephant, a service that reads menus only from
`~/.config/elephant/menus/`. A package cannot put a file there, because a package writes to `/usr`.
Omarchy meets the same wall with its own menus and links them from its install script. A browser
that wants Walker to list its tabs has to place something in another program's configuration
directory, which is a step this repository takes nowhere else, so the rules it keeps there are
recorded here.

## The menu

The package installs a Lua menu under `/usr/share/omaweb/elephant/menus/omawebtabs.lua`. It is Lua
so the tabs are asked for each time the menu opens, as a static menu would list the tabs of the day
it was written. It runs `omaweb tabs --all --json`, which answers from the running browser and never
starts one, and when nothing answers it lists one line saying Omaweb is not running. Choosing a tab
runs `omaweb focus`, which switches to its Space, selects it and brings the window forward.

Each entry's text is the tab's title. Beneath it are the address's host, without any user name or
password the address carries, and the Space's name. The whole address is a search keyword, so a part
of a path finds the tab too. Private windows are never listed: the control socket belongs to the
ordinary window, and `tabs` never answers for a Private one.

The menu runs a program already open to anything running as the reader, and it asks nothing a script
could not. It does widen who sees titles and addresses: Walker and Elephant, which are the reader's
own processes, hold them for as long as the menu is open. Allow agents is not involved, because
`tabs` and `focus` are browser commands.

## The link

When Omaweb starts, and Elephant is installed, it makes `~/.config/elephant/menus/omawebtabs.lua` a
symbolic link to the installed menu. Elephant is installed when `elephant` is on the path. The
rules, each held by a test:

- Nothing is written on a machine without Elephant, so no directory is created in a configuration
  that has no Elephant in it.
- A file the reader put at that path is never replaced, and neither is a link of theirs, dangling or
  not. Omaweb's own link is a symbolic link to a file named `omaweb/elephant/menus/omawebtabs.lua`,
  whichever prefix it names. Nothing else is Omaweb's.
- Its own link is kept as it is when it points at the installed menu, and pointed there when it
  names another or a menu that is gone, as after an upgrade to another prefix.
- Nothing is linked when the package shipped no menu, as in a build tree.
- The setting **Show tabs in the launcher** is on by default and shown only where Elephant is
  installed. Turning it off removes Omaweb's own link and nothing else. It stays on this machine,
  outside the Sync projection, because another machine's launcher says nothing about this one's.

A link is the right size. A copy would be stale after every upgrade until the next start, and the
link follows the package. A package removed leaves a dangling link, which Elephant skips and the
next start of any Omaweb that is installed replaces. One removed for good leaves one inert file in a
directory the reader can see.

## What this does not do

Omaweb writes no Walker configuration. The reader reaches the menu with a prefix they add to
`~/.config/walker/config.toml`, or with a keybinding that opens it directly, and the README gives
both. Omarchy's own configuration already gives `@` to web search, so the prefix is the reader's to
spend.

The menu's two lines of its own text, that Omaweb is not running and that it could not list its
tabs, are English. A Lua menu does not read Qt's catalogues, and the lines are Walker's rather than
the chrome's.
