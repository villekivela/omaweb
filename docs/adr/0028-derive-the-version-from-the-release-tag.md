# Derive the version from the release tag

Omaweb versions follow semantic versioning, and the git tag is the only place a
version number is written by hand. `cmake/OmawebVersion.cmake` runs
`git describe --tags --match 'v[0-9]*'` before `project()` and feeds the result
to `project(VERSION)`, so the tag, `QCoreApplication::applicationVersion`, and
the macOS bundle cannot drift apart. A tree with no tags, no history, or no git
falls back to `OMAWEB_FALLBACK_VERSION` in that module.

Two version strings leave the build, and they differ on purpose. `PROJECT_VERSION`
is the plain `x.y.z` that `MACOSX_BUNDLE_SHORT_VERSION_STRING` needs, because
macOS rejects anything else. `OMAWEB_VERSION` is the full description, so a
build made fourteen commits past `v0.2.0` reports `0.2.0-14-gabc1234` and a bug
report names the commit. `MACOSX_BUNDLE_BUNDLE_VERSION` is the commit count,
which rises on every commit and never resets, which is what macOS expects of a
build number and what `CFBundleVersion` comparisons rely on.

While the major version is 0 the project is pre-alpha and the minor version
carries breaking changes. A `!` in a commit subject before 1.0.0 therefore bumps
the minor, not the major. The release workflow marks every `v0.*` tag as a
prerelease on GitHub for the same reason.

Releases publish notes and no binaries. `scripts/release_notes.sh` groups the
Conventional Commit subjects since the previous tag, which is why the commit
convention is enforced in CI: the changelog is a product of the commit
messages rather than a file anyone maintains. A Linux package release requires
the full notice inventory that `THIRD_PARTY_NOTICES.md` describes, so a binary
release is a separate decision from a version number. macOS bundles remain
development artifacts and are not attached to releases.

The alternative was a version literal in `CMakeLists.txt` bumped by hand. It
needs a commit per release, and it goes stale silently, because nothing fails
when the literal and the tag disagree. Deriving from the tag makes the
disagreement impossible instead of detectable.
