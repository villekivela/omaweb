# Sync browser state through a forge-owned git repository

Sync is an optional Linux Feature module. The browser loads it only when a reader connects Sync or
an enabled Sync marker already exists. Local browsing never waits for it: changes enter the normal
session and configuration stores immediately, then the module reconciles after 30 quiet seconds,
every five minutes while enabled, and when the reader asks for Sync now. Git and network work run
away from the shell thread.

The initial forge adapter is a project-owned GitHub App. Connect starts GitHub's device flow and
opens its authorization page. After authorization, Omaweb creates or identifies the private Sync
repository. When installation is needed, the forge adapter builds an installation URL scoped to the
authorized account and that repository. This avoids GitHub's requirement to select at least one
repository without requesting access to an unrelated repository. Omaweb polls until the App can
access the Sync repository, then starts Sync without a manual continuation step. The GitHub login
becomes the Sync identity; Omaweb has no Account service or account database.

The adapter asks for repository Administration write and Contents read/write, creates a private
`omaweb-sync` repository, and reuses one it can identify as its own. An unrelated repository with
that name is never adopted: the adapter tries a numbered name instead. The official App's public
client ID and slug are build defaults and remain overridable for development. User and refresh
tokens live only in Linux Secret Service. While installation is pending, the authorized session
exists only in memory. A short-lived access token reaches git through an inherited pipe and askpass
helper, never an argument, file, remote address, or general environment variable.

Forge authorization, repository provisioning, installation URL construction, secret storage, and git
reconciliation are separate interfaces. A later Forgejo or Gitea adapter can replace GitHub without
changing the record model or creating an Omaweb account system. The first release intentionally
offers no arbitrary server field: server compatibility, App registration, API variation, and trust
messaging need their own delivery.

Each Space and tab is an independently encrypted record. The module uses libsodium
XChaCha20-Poly1305 with a fresh nonce and authenticates the schema version, record kind, and stable
identifier as associated data. A random 256-bit recovery key lives in Secret Service and is shown
once as grouped, checksummed text for copying or saving with owner-only permissions. Another machine
must be given that key manually. A wrong or missing key pauses Sync before any remote state is
applied. QR pairing is deferred: pointing a laptop camera at another screen is awkward, and an
authenticated short-code exchange over the local network is a better candidate. Secure recovery-key
rotation needs a new repository and explicit migration; in-place rotation is not claimed. That work
is tracked by [#206](https://github.com/villekivela/omaweb/issues/206).

The readable records are the complete keybindings file, filter subscription addresses, and these
Settings keys only: `floating-controls`, `ease-sidebar`, `use-favicons`, and `tint-favicons`. Each
setting remains a separate record. Space names, colours, ordinary tabs, and Pinned tabs are
encrypted. Active Space and active tab selection remain local transients. Theme selection is
deferred because the operating system normally owns it.

Cookies, passwords, active logins, downloads, site permissions, browsing history, the per-site
content-blocking disable list, user rules, and all Private-window information never enter Sync. The
module additionally refuses any session store that does not record state, which makes the Private
boundary a core contract rather than a UI convention. The avatar is fetched once during explicit
login, capped, cached under the data root with owner-only permissions, and removed on disconnect.
Startup does not fetch it and Private windows show neither avatar nor monogram.

Records resolve independently by modification time and machine identifier. Deletions become
encrypted tombstones so an offline machine cannot immediately resurrect a closed tab or deleted
Space. Tombstones remain for 90 days. An unknown newer encrypted-record version is preserved and not
applied. Plain configuration conflicts use a deterministic content digest, which guarantees every
machine chooses the same bytes without treating a git commit clock as authoritative. Remote values
replace untouched local defaults; machine-only settings remain untouched.

Git history is bounded. After 256 commits or 30 days, the module creates an orphan snapshot,
advances the repository epoch, and updates `main` with force-with-lease. Tombstones old enough to
expire are removed only while making that snapshot. The local checkout expires reflogs and prunes
unreachable objects. This bounds ordinary repository growth; it is not secure erasure, because a
forge may retain backups or unreachable objects. An old machine that missed an epoch must take the
snapshot before it can publish older history again.

The threat model protects encrypted records from the repository host and detects alteration with
authenticated encryption. It does not hide update timing, repository size, readable configuration,
or the fact that Sync is used. Git history and an epoch help an already-paired machine notice
rollback, but a fresh install with only a recovery key has no trusted remembered head and cannot
prove the host supplied the newest snapshot.

Pause stops reconciliation and keeps the local checkout, credentials, and remote repository.
Disconnect removes the local checkout, cached avatar, tokens, key, and marker; it deliberately
leaves the private repository in the reader's GitHub account. Git history compaction likewise never
means deleting that remote repository.

Tracked by [#173](https://github.com/villekivela/omaweb/issues/173).
