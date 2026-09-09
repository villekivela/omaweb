# Sync configuration through a git remote

Sync carries the non-secret configuration named in [ADR 0006](0006-limit-the-first-sync-contract.md)
through a git remote the reader already owns. The module shells out to `git` rather than linking a
library, which keeps a licensing question out of the module and matches the isolation
[ADR 0005](0005-keep-optional-features-out-of-the-core.md) asks of a Feature module. No host API is
called, so a repository on GitHub, on Codeberg, on a self-hosted Forgejo, or bare on a file share is
one provider rather than four. Git is also offline-first: a change is recorded locally and pushed
when a network exists, so Sync never stands between the reader and the browser.

Each syncable record is one file, so last-writer-wins resolves per record rather than per document.
A Space's name and colour, a Space's Pinned tab set, the synced Settings keys, the filter list
subscriptions, the keybindings, and the theme selection are separate files, and a machine registry
alongside them carries the contract version. Each record states its own modification time and
resolution reads that, not the commit time, which is not trustworthy across machines. A concurrent
push produces a non-fast-forward, and Sync fetches, resolves file by file, and commits the result;
git merge drivers are not used. A record whose contract version the local module cannot read is left
in place rather than dropped or rewritten, because otherwise upgrading one machine destroys state on
the machine still behind.

Settings sync per key rather than as a document. Some are machine-shaped and would damage the
receiving machine: the download directory is a path that need not exist there, and hardware video
decode is a per-GPU decision. Which keys sync and which stay machine-local is declared in one place
and belongs to the record schema from its first version, because adding the distinction later means
migrating every synced document that already exists.

Content-blocking configuration means filter list subscriptions alone. The stored document holds both
the subscriptions and the list of sites where the reader turned blocking off. Subscriptions are
public addresses. The per-site list names sites the reader visited, which is history, and ADR 0006
excludes history. The per-site list therefore stays on the machine that made it.

Sync carries the theme selection and never a palette.
[ADR 0016](0016-separate-user-configuration-from-application-data.md) resolves the palette in a
fixed order in which a hand-written `theme.json` outranks a desktop-rendered theme, so a module that
wrote a palette would silently stop Omarchy theme switching from reaching Omaweb on the receiving
machine, with nothing on screen to explain why.

No Account module ships with this contract. An SSH remote is authenticated by the agent the reader
already runs, so Omaweb holds no credential at all, and that is the recommended path. An HTTPS
remote is the fallback and takes a token scoped to the one repository, kept in the desktop secret
service rather than in the configuration directory or a Space database, which leaves
[ADR 0008](0008-isolate-space-storage-on-disk.md)'s absence of a password store intact. A token
carrying full repository scope is refused, because it would grant Omaweb read and write access to
every repository the reader owns. An OAuth device flow is refused outright: it requires an
Omaweb-operated client identifier, which is the hosted service ADR 0006 says the core must not
assume. The reader creates the empty repository and supplies its address, because creating one from
the browser would require a host API and the scopes that come with it.

Records are not encrypted, and Settings states what the remote holds: Space names, Pinned tab
addresses, keybindings, subscriptions, and the synced Settings keys. A private repository is private
from other people, not from the host that stores it. Encryption is a later decision, and it is
cheaper to add afterwards than the per-key schema is.

The module's checkout and staging area live under the application data root, not the configuration
directory. ADR 0016 draws that line at whether a reader hand-edits the files, and a checkout is
machine-local and rebuildable, so the existing data root override relocates it for tests. It is not
placed inside any Space's directory.

Sync is the first consumer of ADR 0005's module contract rather than a later one. Its Settings pane,
its status indicator, and its headless behaviour are the surface kinds any wider extension contract
would need, and building them for a first-party module is what keeps that contract honest. The
contract stays internal here; nothing in this decision publishes a third-party binary API.

The status indicator's artwork is derived locally by default. Two letters and a colour come from the
remote address, the way a tab's tile already derives its letters from a name. The SSH path holds no
credential and therefore cannot learn an account name, so a fetched picture would make the
authentication path that keeps no secret the one that looks unfinished. A reader may instead place
an image in the configuration directory, and a Settings checkbox, off by default, fetches the
account's own picture once, caches it under the data root, and deletes it when turned off. That
fetch is the first network request Omaweb's own chrome makes to a third party, so
[`docs/network-requests.md`](../network-requests.md) gains its destination, trigger, data sent,
default state, and disable control when the fetch ships. Startup fetches nothing. A Private window
shows no indicator, because displaying an account identity there contradicts what the window is for.

Three things are deliberately left out. Open-tab sessions are excluded because two machines with
live tabs conflict by definition and the failure to avoid is one machine's tab set silently
replacing another's; the intended design offers another machine's snapshot rather than applying it,
and it brings unbounded history growth that needs its own answer. Syncing an installed extension
roster is excluded because a record that makes another machine fetch and run code is a different
class of record from a keybinding and needs the discipline
[ADR 0025](0025-run-only-vendored-scriptlets.md) applies to code a list may name but not supply.
Record encryption is excluded as above. Each is a follow-up, not an oversight.

Tracked by [#173](https://github.com/villekivela/omaweb/issues/173).
