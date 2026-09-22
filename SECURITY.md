# Security policy

Omaweb is alpha. The sections below state what it verifies and what it leaves to the reader. Two
gaps are deliberate and not scheduled to close: Omaweb ships no phishing, malware, or
download-reputation service ([ADR 0032](docs/adr/0032-ship-without-url-reputation.md)), and
QtWebEngine's network service is not a sandboxed process of its own.

## Approved engine baseline

`security/baseline.json` records the supported QtWebEngine version, its Chromium version, and the
latest Chromium security patch it includes. Settings reads this file and reports whether the running
engine meets the baseline. Builds below the baseline are unsupported previews.

Every day, `.github/workflows/security-baseline.yml` compares the baseline with what Qt has
published and with Chromium's stable release. Daily rather than weekly because the response window
starts when the issue opens, and a check that sleeps for six days spends those days of the window on
not having looked. It keeps one issue open while the baseline is outdated. Updating the baseline
requires a review of the Qt release notes and a qualified build.

One baseline covers both published packages, and both run the engine Omaweb built for them:
`omaweb-qtwebengine`, published in the same pacman repository as the browser
([ADR 0049](docs/adr/0049-ship-omawebs-own-engine-build.md)). There is no distribution between Qt
and the reader any more, so an engine below the baseline means Omaweb has not published the rebuild
yet rather than that a distribution is behind. Settings still reports the engine each build is
running, because a reader may have installed the package by hand and not taken the upgrade.

## Response to a security-bearing Qt patch

When a Qt patch release includes security fixes, qualify it and publish the raised baseline within
two days after the baseline issue opens. The issue includes its due date.

The clock starts when Qt publishes. The two days cover applying the patch series, building the
engine for both architectures, qualifying it, and publishing both packages, because all of that is
now Omaweb's (ADR 0049). It is lag Omaweb owns rather than inherits: Arch was quick, building
QtWebEngine 6.11.2 within an hour of Qt publishing it and having it in the repository two days
later, and Omaweb has taken that job over rather than improved on it.

Where a rebase cannot make the window, the extension patches are dropped and the engine ships
anyway. The bug fixes apply to any build, so the Chromium fix still reaches readers; Known
extensions stop loading until a later engine update restores them. A reader's vault is untouched,
because it belongs to the Space's engine profile.

To update the baseline:

1. Read the Qt release notes for the fixes the release carries.
1. Rebase the patch series onto the release and build the engine for both architectures, which is
   `PROCESS.md` in the patch repository. A conflict here is what a late rebase looks like, and the
   paragraph above says what to do about one.
1. Publish `omaweb-qtwebengine` to the pacman repository, so that the engine reaches readers whose
   next `pacman -Syu` comes before the browser release.
1. Build against the new engine and run `ctest --preset ci` in full.
1. Update `qtwebengine`, `chromium`, `chromiumSecurityPatch`, and `reviewed` in
   `security/baseline.json`.
1. Tag a release so the raised baseline reaches readers, which is what stops a build that meets it
   reporting itself as unsupported. Omaweb has no application updater, so the package handles its
   own release and update delivery.

Until the baseline lands, every build reports itself as below the approved baseline.

## Renderer isolation

Each page runs in its own renderer process, sandboxed by the operating system. Omaweb refuses to
start when the sandbox is explicitly disabled through either its command line or
`QTWEBENGINE_CHROMIUM_FLAGS`, and refuses to start on a Linux host whose kernel cannot meet the
sandbox's prerequisites rather than falling back silently.

QtWebEngine handles the network inside the browser process. That network service is not a sandboxed
process of its own, as it is in Chromium's own builds, and Omaweb does not describe it as isolated.

## Downloads

Omaweb asks for confirmation before downloading an executable, script, installer, disk image, or
archive. It identifies the file type from the proposed filename, or from the server's declared media
type when the filename is inconclusive. It writes nothing until the reader confirms. Automatic
downloads and a site's second concurrent download require a revocable Site permission.

For each finished download, Omaweb records the source address in operating-system metadata and
removes execute permissions. Omaweb never opens downloaded files. The reveal action opens the
containing directory.

Omaweb does not check downloads with a phishing, malware, or software-reputation provider. It
identifies file types but does not claim that files are safe. See ADR 0032.

## Report a vulnerability

Report suspected vulnerabilities through GitHub's
[private vulnerability reporting form](https://github.com/villekivela/omaweb/security/advisories/new).
Do not open a public issue for an unpatched vulnerability.

Include the affected commit or version, steps to reproduce the problem, its security impact, and any
known workaround. Please allow time for a fix before publishing details.
