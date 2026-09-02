# Contributing

Omaweb is pre-alpha. The Qt build on macOS is the only configuration that gets
daily use, Linux and Wayland come next, and the Ladybird build stays
experimental until its embedding and security contracts are ready. Patches for
the macOS and Linux Qt builds are the most useful thing to send.

## Before you write code

Open an issue first for anything beyond a small fix. Issues are where this
project keeps its specs, not just its bug reports, so a short discussion up
front saves a rewrite. See `docs/agents/issue-tracker.md` for the conventions
and `docs/agents/triage-labels.md` for what the labels mean.

Read `README.md` for what the browser deliberately does not do. Developer
tools belong to the tab being inspected, and there is no diagnostics protocol,
agent gateway, terminal, or source editor. A pull request adding one of those
will be declined on scope, however good the code is.

`docs/adr/` holds 27 architecture decisions. Skim the ones near your change.
They record what was already settled and why, which is usually faster than
asking. If your change contradicts one, say so in the issue and expect to
argue the case. A change that settles a new question of its own wants a new
ADR alongside it.

## Building

`docs/development.md` is the full reference. The short version:

```sh
scripts/bootstrap_content_blocker.sh   # once, before the first configure
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

Use the `ui` preset for QML work. It skips the web engine, so it configures and
builds in a few seconds and runs a lab that can open one interface state
directly:

```sh
cmake --preset ui
cmake --build --preset ui
./build/ui/omaweb-ui-lab.app/Contents/MacOS/omaweb-ui-lab
```

On Linux that binary is `./build/ui/omaweb-ui-lab`. Pass `--capture <path>` to
render a frame to a PNG, which works headlessly and is the easiest way to show
a chrome change in a pull request.

`ctest --preset dev` must pass before you open a pull request. CI runs the `ci`
preset on Arch Linux for every change. The macOS job runs on demand only,
because macOS runners bill at ten times the minute rate, so run the tests
locally if you are working on macOS.

## Things that will fail the build

Two dependency trees are vendored byte for byte and pinned by a `MANIFEST.json`:
the Omarchy QML kit under `third_party/omarchy-shell`, and uBlock Origin's
scriptlet and resource library under `third_party/ubo-scriptlets`. Never edit
either. `ctest` compares both against their manifest and fails on any local
change. Adapt the kit from `src/ui` instead, which is what the existing QML
does.

Moving a pin is deliberate work, not part of a feature branch. Read the
upstream diff first, especially for the uBO trees, whose contents run inside
the pages the browser loads.

Never add `--no-sandbox`, `--single-process`, in-process network-service flags,
or `QTWEBENGINE_DISABLE_SANDBOX` to a script or preset. Every build keeps its
engine sandboxes ([ADR 0013](docs/adr/0013-preserve-engine-sandboxes-in-every-build.md)).
Fix the host configuration or the packaging instead.

Do not add Ladybird, Qt source builds, or Rust compilation to the `dev` preset.
Keeping `dev` fast is a measured budget, not a preference
([ADR 0007](docs/adr/0007-keep-dependencies-out-of-the-fast-build.md)).

## Commits and pull requests

Commit messages follow Conventional Commits. `docs/agents/commits.md` has the
format and the allowed types. Keep the subject imperative, lowercase, and
inside 72 characters. CI checks every non-merge commit, so a rebase is cheaper
than a fixup after review.

Say what changed and why in the pull request body. For anything that alters the
interface, attach a capture from the UI lab.

## Reporting a vulnerability

Do not open a public issue. `SECURITY.md` has the private reporting form.
Omaweb is pre-alpha browser software, so treat it as unsafe for sensitive
browsing regardless.

## License

Omaweb is under MPL 2.0 ([ADR 0014](docs/adr/0014-license-omaweb-under-mpl-2.md)).
Code you contribute goes out under the same license. The license covers the
code and grants no rights in the project's name or branding.
