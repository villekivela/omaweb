# Open a project's Space with omaweb dev

Amends [0058](0058-start-the-readers-agent-on-request.md), which starts the reader's agent from
their home directory with the one agent command from Settings.

A reader building a web app runs their dev server, their coding agent and a browser, and joins the
three by hand: find the Space they test in, type the address, and start the agent in the right
folder. `omaweb dev [address]`, run in the project's folder, does the browser's part, and `:ask` in
that Space does the agent's.

## A project is a property of a Space

A Space may have a Project directory: the folder on this machine it is for, the address its app is
served at, and optionally an agent command of its own. It is not a new kind of Space, so the Space
keeps its own logins, cookies and history, and syncs as any Space does. The project does not. It is
kept beside the Space records, as a Space grant is, so nothing that copies a Space can carry a path
that means nothing on another machine.

Run with an address, `omaweb dev` makes the folder it runs in a project, in a new Space named after
the folder, or gives an existing project in that same folder the new address. Run without one, it
finds the nearest project directory at or above the folder. Folders are compared whole, so a
monorepo's root and each of its apps can be a Space each. With no address given and none remembered,
it fails and says how to give one. Omaweb never reads the project's files to guess.

## Omaweb opens and never runs

`omaweb dev` switches to the Space and raises the window, as `focus --raise` does, and selects a tab
already on the address's origin. Otherwise the Space waits for the address to answer, while the
Start page's road drives. Omaweb sends a HEAD request, with no cookies and no redirects followed,
every half second until the server answers with any HTTP status or presents a certificate. A
connection alone is not an answer: a port forward into a container or to another machine takes the
connection before the server behind it is up. Then the address loads in the Space's blank tab. The
CLI returns at once. Omaweb never starts, stops or watches the dev server, and a server that never
comes up leaves the road driving until the reader presses Escape, opens something else from the
Start page, or forgets the project.

It is a browser command, open with Allow agents off, because any process can run it and none of it
reads a page. For the same reason it grants nothing: no Space grant, no Agent tab. An agent that
then works in the Space is asked once over the page, as anywhere.

## Where :ask starts the agent

In a project's Space, `:ask` runs the project's agent command, or the global one when it has none,
in the project directory: `xdg-terminal-exec --dir=<directory>` with the same working directory, so
an agent on this machine reads the project's `CLAUDE.md` and every one above it. A folder recorded
inside a container that is not on this machine opens the terminal at home instead.

`{dir}` in the command is replaced with the project directory, after the command is split as a shell
splits a line and inside each argument. `ssh -t devbox "cd {dir} && claude"` keeps a path with a
space as one argument, and no shell on this machine ever reads it. Quoting the path for the far
side's shell is the command's to do. In any other Space the command runs as 0058 describes and
`{dir}` means nothing.

## What this costs

Any process the reader runs can make a Space and record the agent command `:ask` runs in it, which
is no more than it could already do by editing `privacy.json`. Settings shows each project's folder,
address and agent command read-only, marks a folder that is not on this machine, and offers Forget
project. It never edits them: the folder is where `omaweb dev` ran, and the place to change it.
