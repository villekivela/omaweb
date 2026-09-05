# Security policy

Omaweb is pre-alpha browser software. Do not use it for sensitive browsing yet.

## Approved engine baseline

`security/baseline.json` records the supported QtWebEngine version, its Chromium version, and the
latest Chromium security patch it includes. Settings reads this file and reports whether the running
engine meets the baseline. Builds below the baseline are unsupported previews.

Every Tuesday, `.github/workflows/security-baseline.yml` compares the baseline with the packaged
QtWebEngine and Chromium's stable release. It keeps one issue open while the baseline is outdated.
Updating the baseline requires a review of the Qt release notes and a qualified build.

## Response to a security-bearing Qt patch

When a Qt patch release includes security fixes, publish a tested Omaweb update within seven days
after the baseline issue opens. The issue includes its due date. To update the baseline:

1. Read the Qt release notes for the fixes the release carries.
2. Build against the new engine and run `ctest --preset ci` in full.
3. Update `qtwebengine`, `chromium`, `chromiumSecurityPatch`, and `reviewed` in
   `security/baseline.json`.
4. Tag a release so the Linux package includes the new engine. Omaweb has no application updater, so
   the package handles release and update delivery.

Until step 3 lands, every build reports itself as below the approved baseline.

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
