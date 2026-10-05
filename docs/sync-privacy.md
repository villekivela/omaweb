# What is synced, and what is not

This is the reader-facing account of Omaweb Sync: what it copies, what it encrypts, what stays
readable, what never leaves your machine, and which GitHub permissions it asks for. It is the GitHub
App's homepage, reached as <https://omaweb.app/sync>. The [Sync adapter notes](sync.md) cover how it
is built.

Sync is optional and does nothing until you connect it. It copies a fixed list of browser state
between your machines through a private Git repository that you own. There's no Omaweb account and
no Omaweb server, and nothing from a Private window is ever included.

## Your GitHub login is your identity

Omaweb doesn't create or run a browser account.

### No account to sign up for

Signing in to GitHub is what pairs your machines. Omaweb runs no account service and keeps no user
database, and nothing about you reaches the Omaweb project.

### A repository Omaweb maintains

Omaweb creates a private repository called `omaweb-sync`, or reuses one it can tell it made. It
never adopts an unrelated repository with the same name. It picks a numbered name instead. Commits
are authored under your GitHub login.

## Encrypted, readable, or never sent

Every piece of browser state falls into exactly one of these three lists.

### Encrypted before upload

Space names and colours, ordinary tabs and Pinned tabs. Each is its own record, encrypted on your
machine with XChaCha20-Poly1305 under your recovery key. GitHub stores only ciphertext and can't
read them.

### Readable in the repository

Your whole `keybindings.json`, your filter subscription addresses, and three settings:
`floating-controls`, `use-favicons` and `tint-favicons`. These are stored as plain text, so anyone
who can read the repository, GitHub included, can read them. A repository an earlier version wrote
`ease-sidebar` into loses that file on the next sync, since the setting no longer exists.

### Never synced

Passwords, cookies and active logins. Payment cards, which stay in your desktop's keyring. Browsing
history. Form history, what you typed into forms. Saved addresses. Downloads. Site permissions.
Per-site content-blocking exceptions and your own rules. Anything in a Private window. Which Space
and tab you have open stays on the machine, and so does your theme.

## What encryption covers, and what it doesn't

The records are encrypted and authenticated. The repository around them isn't.

### Covered

The contents of every Space and tab record, and any tampering with them. Each record authenticates
its schema version, kind and identifier, so a modified record is rejected instead of applied.

### Not covered

That the repository exists, how big it is, and when it changes. The readable settings above. Your
login on each commit. The fact that you use Sync at all.

## What the GitHub App asks for

### The two permissions

**Repository administration: write**, so Omaweb can create the private Sync repository. **Repository
contents: read and write**, so Omaweb can recognise that repository and keep its files in sync.

### The scope you actually grant

GitHub grants both permissions on every repository you select when installing the App. Omaweb asks
you to choose _Only select repositories_ and pick just `omaweb-sync`, which limits the grant to that
one repository. Inside it, administration write is GitHub's full repository-administration
permission, which covers much more than creating it.

### Why you confirm the repository yourself

GitHub won't let an App create a personal repository before it's installed, and its installation
form won't accept zero repositories. So Omaweb opens GitHub's new-repository form with the name,
owner, description and Private visibility already filled in. You confirm it, then install the App
for that repository.

## The recovery key

The key that decrypts your Space and tab records never reaches GitHub or Omaweb.

### Shown once

Your first machine generates a random 256-bit key and stores it in the Linux Secret Service. Omaweb
shows it to you once, grouped and checksummed, so you can copy it or save it to a file only you can
read. You carry it to your second machine yourself.

### Lost means lost

Neither Omaweb nor GitHub can recover the key. If every copy is gone, the encrypted Space and tab
records can't be read again. A wrong or missing key applies nothing and pauses Sync, so a typo can't
change either machine. You can't change the key in place. That takes a new repository and a
deliberate migration.

## Local first, synced after

Your browser never waits on Sync. Every change is saved on your machine first and synced afterwards.

### When it contacts GitHub

Thirty seconds after your last change, every five minutes while Sync is on, after reconnecting, and
whenever you press Sync now. An expired access token gets refreshed first. Your avatar is downloaded
once, when you log in.

### Pause and disconnect

Pause stops the requests but keeps your credentials, local checkout and repository. Disconnect
deletes the local Sync credentials, checkout and avatar. Disconnecting doesn't delete the repository
on GitHub. Delete it there if you want it gone.

## Repository size and history

### Compaction

After 256 commits or 30 days, Omaweb replaces the history with a fresh snapshot and moves the
repository to a new epoch. Deleting a tab or Space leaves an encrypted marker for 90 days, so a
machine that was offline can't bring back something you closed.

### Not secure erasure

Compaction limits normal growth. It doesn't erase what GitHub already has. GitHub can keep backups
and unreachable objects after the history is replaced.

## Setting it up

Sync works on Linux only, and isn't available in a Private window.

### The first machine

Open Settings and choose Connect. Authorize on GitHub with the code shown, confirm the prefilled
private repository, and install the App for that repository only. Save the recovery key when it
appears.

### Every machine after

Sign in with the same GitHub account and enter your saved recovery key. The
[Sync adapter notes](sync.md) describe the key and the setup flow in more detail.

## GitHub is the first provider

Signing in, creating the repository and syncing it are separate parts, so a Forgejo or Gitea adapter
could take GitHub's place without Omaweb growing an account system of its own. This release has no
field for an arbitrary server, because each forge needs its own App registration and its own
explanation of what it can reach.
[The decision record](adr/0039-sync-configuration-through-a-git-remote.md) has the reasoning, and
the [network request ledger](network-requests.md) lists every request the browser makes on its own.
