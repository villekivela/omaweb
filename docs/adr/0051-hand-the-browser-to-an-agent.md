# Hand the browser to an Agent

Supersedes the part of [0027](0027-use-engine-provided-developer-tools.md) that ruled out a CLI, an
MCP server and any agent-access authority model. Developer tools stay as 0027 describes them. Amends
[0033](0033-stop-an-away-spaces-pages-instead-of-taking-them.md) with a third exemption from
freezing.

A reader who runs a coding agent in a project wants it to use the browser: to open the site the
project serves and find out why something is as it is, and to act for the reader on sites they use.
The agent tools that exist for Chromium are slow. chrome-devtools-mcp spends about 17,000 tokens on
its tool schema, returns accessibility snapshots half made of ignored nodes, and waits after every
action. They also attach to the engine's remote debugging server, which in QtWebEngine 6.11 serves
every page of every Engine profile, Private windows included, to anything that connects.

ADRs 0019 to 0024 answered with Diagnostics: a Rust helper, an engine-neutral protocol, a terminal
interface, remote Development targets and five authorities, built around debugging a broken page.
The job was too narrow, the machinery came before a single click, and the browser became a broker
for a protocol of its own. 0027 withdrew it and left nothing in its place. This decision keeps what
0021 had right, that the reader brings their own agent and Omaweb holds no model, no provider
credentials and no page data bound for an AI service, and replaces the rest.

## One socket, two front ends

The browser listens on a Unix socket at `$XDG_RUNTIME_DIR/omaweb/control.sock` with mode 0600. Two
front ends speak to it: `omaweb mcp`, a stdio MCP server that starts the browser if it is not
running, and the `omaweb` CLI. A skill shipped with the package teaches an Agent the CLI, because a
CLI costs an Agent far fewer tokens than a large tool schema.

The socket is the boundary, and it is the operating-system user. Nothing on one machine can tell
Claude Code from the reader's own script, as
[0019](0019-broker-engine-neutral-diagnostics-sessions.md) already accepted, so a connection's name
is for the log and the markers and never an identity. What a connection may do therefore depends on
what a verb reaches, never on who asks.

Browser commands are always open: listing Spaces and their tabs, never a Private window's, opening
an address in a Space, switching Space, selecting a tab, and the command panel's public commands.
They are what a keybind or a shell script needs, they are what the desktop's `Open` handover already
lets any process do with an address, and none of them reads a page or acts inside one. The page
verbs and Agent Spaces are behind the **Allow agents** setting, off by default, and the Space grants
below. Turning the setting off detaches every connection from the pages it was driving; the socket
stays up for browser commands.

## The client and a sandbox

`omaweb` on the PATH is a small client, built from the same code as the browser's verbs and linked
only to Qt's Core and Network. Given a verb or `mcp` it talks to the socket; given anything else, a
launch or an address from `xdg-open`, it replaces itself with the browser, which is installed at
`/usr/lib/omaweb/omaweb-browser`. It is its own package, `omaweb-cli`, which carries the skill and
which the browser's package depends on, so an Agent in a container, a Distrobox, a VM or on another
host has the same command and the same skill as one on the desktop. Where no browser is installed, a
launch says so, and `omaweb mcp` says it has nothing to start and exits.

A sandbox often has no login session and so no runtime directory, so the client reads
`OMAWEB_CONTROL_SOCKET` before the default path. The client and the browser may come from different
releases, so the client opens each connection with `hello`, naming the protocol it speaks, and the
browser answers with its own. On a mismatch the client warns once, naming both versions, and carries
on. A browser refuses a verb it does not know as unknown rather than reading it as another.

Forwarding the socket extends the user boundary to whatever receives it. The socket is the user, as
above, so a container, a VM or a host given the socket acts as the reader in the browser, exactly as
a process of theirs on the desktop does. No authentication is added for it: browser commands are
open to it, and the page verbs and Agent Spaces still need Allow agents and a Space grant.
Forwarding the socket is the reader's decision about the sandbox, and the README says what it hands
over.

## What the page verbs may reach

An Agent may create Agent Spaces and has full use of them. `--temporary` makes one that is deleted,
with its Engine profile and Browsing data, when its connection closes. Any other Space needs a Space
grant: the first time a connection reaches it, a prompt bar asks the reader once, and the answer
lasts until the reader revokes it in Settings. A grant belongs to the Space, not to a connection. In
a granted Space an Agent may use any tab, including the one on show and Pinned tabs, but it may not
change a pin and may close only the tabs it opened. Private windows are never reachable. Grants, the
setting and the Agent Space label stay on the machine and are outside the Sync projection, so a
grant given on one machine does not open another.

The prompt is once per Space rather than once per action because the danger is not any one action.
It is prompt injection: a page the reader did not write instructing an Agent that is logged in as
the reader. So the rules that remain apply where injection pays. A file upload is refused outside an
Agent Space, because it is how an injected instruction takes the reader's files, and a High-risk
download still waits for the reader.

## Speed is in the verbs

Omaweb serves its own small set of verbs rather than the engine's debugging protocol, and the
protocol stays an implementation detail. Version one has `spaces`, `space new`, `tabs`, `open`,
`close`, `look`, `read`, `do`, `shot`, `eval` and `console`. Each connection has its own current
tab, which the reader's focus never moves, and no verb takes the reader's focus or keyboard.

The verbs are shaped by what makes the fast tools fast. `look` returns the interactive targets in
the viewport with hint labels, the same idea as Keyboard navigation's, and a short text outline,
which is a few hundred tokens where an accessibility snapshot is thousands. A label names its
element for as long as the document lives, so repeated looks agree and an Agent looks less often.
`do` takes a batch of steps and waits in the browser, until a navigation it started has committed
and the DOM has been quiet for 300 ms, rather than returning to the Agent after every click. It
stops at the first failed step and returns a fresh `look` either way. A screenshot goes to a file.
While the reader's keyboard focus is inside an Agent tab, `do` waits and the reader wins.

Version one needs no engine change. Page work runs in the isolated world, where `eval` also runs,
and the console comes from the engine's console-message signal. Version two adds network inspection
through a patch in the engine series giving Qt a per-page debugging session whose client is not
trusted, which Chromium already refuses `Target.getTargets` and `attachToTarget`. That session is
the scoping the remote debugging server cannot give, and it is written as a general Qt addition so
it can go upstream. `eval` in the page's own world waits for it and is offered in Agent Spaces only,
because running in the main world is the privilege [0025](0025-run-only-vendored-scriptlets.md)
keeps from Web extensions. Tools a site registers through WebMCP can be passed through later, once
sites register them.

## What the network verbs may reach

Version two's `network` and `request` work in any Agent tab, in a granted Space as in an Agent
Space, because a granted Space already lets an Agent read and drive the page, and a page's requests
add little to that. What they add is the reader's credentials, so those are where the rules differ.
In a granted Space the values of `Cookie`, `Set-Cookie`, `Authorization` and `Proxy-Authorization`
are redacted and only their count is shown, and a request's body, what the page sent, is refused, as
a file upload is. In an Agent Space the identity is the Agent's own, so both are shown. A response
body is available in either.

A tab's requests are captured from the moment a connection attaches, in memory only, and start again
with each new document, as the console does. They and the debugging session are dropped when the
Agent detaches, when Allow agents is turned off and when the tab closes. The log names a request by
its id and never records an address, a header or a body, because an address can carry a token. The
session reaches the page and its frames. A service worker is a target of its own, which an untrusted
session cannot attach to, so its requests are not seen.

## Agent tabs keep running

An Agent works in Spaces the reader is not looking at, and 0033 freezes those pages. An Agent tab is
therefore the third exemption, beside the inspected tab and the tab making sound, for as long as a
connection is attached to it. Freezing is not the only obstacle. A hidden view tells Chromium it was
hidden, and then it draws no frames, runs no animation callbacks, throttles its timers and cannot be
captured. So an Agent tab stays visible to the engine, sized like the page area and stacked behind
the page on show, and input reaches it as Qt events sent to the view rather than as script events,
so a page sees them as trusted. Both depend on a spike
([#376](https://github.com/villekivela/omaweb/issues/376)). If trusted input fails, version one
sends script events and says so here.

A prototype on the development listener
([`prototype/372-agent-mcp`](https://github.com/villekivela/omaweb/tree/prototype/372-agent-mcp))
settled that the verbs are worth building. Its tool schema was about 670 tokens and a `look` about
110, one `do` filled and submitted a sign-up form in 1.2 seconds, and a click sent as engine input
reached a tab on show as trusted.

## What the reader sees

An Agent tab, and a Space holding one, is identifiable at a glance, including when the Space is
away. How that looks is designed separately
([#384](https://github.com/villekivela/omaweb/issues/384)). Every verb is logged with its time,
connection, Space, tab and target, never a page's text or a filled value, and kept for 7 days. JS
dialogs in an Agent tab go to the Agent instead of in front of the reader, and an Auxiliary window
the Agent opens is marked like its tab.

## What this costs

The command surface Omaweb promises outside its own interface is new, and a verb once shipped is a
contract. Keeping an Agent tab rendered costs GPU while one is attached, and the marker reports it.
The verbs are Omaweb's, not Chromium's, so each engine answers them itself; Ladybird can implement
the same verbs through its own APIs when it becomes the target engine, and nothing here promises an
engine-neutral page protocol beyond them.
