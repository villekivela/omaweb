# Security policy

Omaweb is pre-alpha browser software. Do not use it for sensitive browsing yet.

## Approved engine baseline

`security/baseline.json` names the QtWebEngine a supported build runs on, the
Chromium release it is built from, and the Chromium release whose security fixes
that engine carries. The build reads the same file, so Settings says at runtime
whether the engine it is running meets the baseline; a build below it is an
unsupported preview and says so rather than looking like every other build.

`.github/workflows/security-baseline.yml` compares the file against the
packaged QtWebEngine and Chromium's own stable release every Tuesday and leaves
one open issue behind while the baseline is behind. It never raises the
baseline: reading a Qt release's security notes and qualifying a build against
it is a person's work.

## Response to a security-bearing Qt patch

A Qt patch release that carries security fixes produces a tested Omaweb update
within seven days of the baseline issue being opened. The issue carries the date
it is due by, and the work is:

1. Read the Qt release notes for the fixes the release carries.
2. Build against the new engine and run `ctest --preset ci` in full.
3. Raise `security/baseline.json` — `qtwebengine`, `chromium`,
   `chromiumSecurityPatch`, and `reviewed`.
4. Tag a release, so the Linux package carries the new engine. Release and
   update delivery belong to the package; Omaweb ships no application updater.

Until step 3 lands, every build reports itself as below the approved baseline.

## Renderer isolation

Each page runs in its own renderer process, sandboxed by the operating system.
Omaweb refuses to start when the sandbox is explicitly disabled through either
its command line or `QTWEBENGINE_CHROMIUM_FLAGS`, and refuses to start on a
Linux host whose kernel cannot meet the sandbox's prerequisites rather than
falling back silently.

QtWebEngine handles the network inside the browser process. That network service
is not a sandboxed process of its own, as it is in Chromium's own builds, and
Omaweb does not describe it as isolated.

## Downloads

A file the desktop would run, install or mount on a double-click — an
executable, a script, an installer, a disk image, or an archive that may carry
one — is a question before it is a download. The kind is named to the reader
from the name that will land on disk, falling back to the server's declared type
only where the name says nothing, and nothing is written while the question
stands. A page that downloads without being touched, or starts a second download
while its first runs, takes a Site permission the reader can see and take back.

A finished download is marked with the address it came from in the metadata the
operating system already reads, and left without its execute bits. Omaweb never
opens a download and never offers to: revealing one shows the directory it
landed in.

Omaweb checks no download against a phishing, malware, or software-reputation
provider, and says so rather than implying coverage it does not have
(ADR 0032). What it can say about a file is what the file is, not whether it is
safe.

## Report a vulnerability

Report suspected vulnerabilities through GitHub's
[private vulnerability reporting form](https://github.com/villekivela/omaweb/security/advisories/new).
Do not open a public issue for an unpatched vulnerability.

Include the affected commit or version, steps to reproduce the problem, its
security impact, and any known workaround. Please allow time for a fix before
publishing details.
