# Start the reader's agent on request

Amends [0051](0051-hand-the-browser-to-an-agent.md), which has the reader start an Agent outside the
browser and point it at Omaweb.

A reader looking at a page who wants an agent to summarize it or fill it in has to leave the
browser, open a terminal, start their agent and tell it which tab to use. `:ask <words>` in the
Omnibar's command scope does those steps for them. Omaweb runs the reader's terminal through
`xdg-terminal-exec`, which runs the agent command from Settings, `claude` by default. The agent gets
one more argument: a prompt that names the tab on show by its id, tells it to use the `omaweb` CLI
skill on that tab, and then gives the reader's words. The answer, and any back-and-forth, stays in
that terminal.

## Omaweb still holds no model

The browser starts a program, the way a keybind or a desktop launcher does. The program is the one
the reader named and is started as the reader, and it finds the browser the same way any Agent does:
over the control socket, under the rules 0051 set. Omaweb sends no page text and no request to any
service. It holds no provider credentials and does not read what the agent answers. Allow agents and
the Space grants are unchanged, so the first time the agent reaches one of the reader's Spaces the
grant bar asks once, as it would for an agent the reader started themselves. While the agent is
attached, the tab is an Agent tab and is marked as one.

Starting the agent is not logged. The agent's connection and every verb it sends are logged as 0051
describes, so the reader's words never enter the activity log.

## How it is started

The agent command is split as a shell splits a line, so `claude --model sonnet` works, but no shell
runs it. The tab id and the reader's words go to the terminal as separate arguments, so quotes,
`$(...)`, backticks and newlines in them reach the agent as typed. A missing agent, a missing
`xdg-terminal-exec`, or a terminal that will not start ends in a page notice naming the program that
failed.

`:ask` is always listed in the command scope of an ordinary window. While Allow agents is off,
running it asks over the page whether to turn the setting on. Turn on enables it and goes on with
what was typed; Not now does nothing. A Private window is never an Agent's, so there `:ask` is not
listed and, run anyway, says it is not available. `ask` is not a public command: an Agent cannot
`omaweb run` it to start another agent, because it starts a program and only the reader may decide
to do that.

## What this costs

Omaweb now starts a process on the reader's command. It runs only the command the reader configured,
only when they run `:ask`, and only through `xdg-terminal-exec`, so a desktop without it gets a
notice instead of a guess at which terminal to use. The prompt is written for the skill shipped with
the package and is English, because the agent reads it, not the chrome.
