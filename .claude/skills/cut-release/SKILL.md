---
name: cut-release
description:
  Tag an Omaweb release and see it published. Use when the user asks to cut, tag, or publish a
  release, or when an engine update has landed and readers are waiting for the browser that runs on
  it.
---

# Cutting a release

A release is a tag on `main`. Everything after that is a workflow: the packages, the inventory, the
notes, and the pacman repository a reader upgrades from. So the work here is the checking that
happens before the tag, because a tag is the one step that cannot be taken back quietly.

## Before the tag

Run all five. Each has produced a broken or unusable release at some point, or would have.

1. **`main`, clean, and current.**

   ```sh
   git switch main && git pull --ff-only
   git status --short
   ```

   The workflow refuses a tag that is not on `main`, and it refuses after building, so a tag on the
   wrong branch costs the build and leaves the tag behind.

   A patch after the next minor's first `feat` has merged is the exception: it is tagged on the
   series' maintenance branch, such as `release/v0.7`, and every step below runs there instead
   ([Maintenance branches](../../../docs/agents/commits.md#maintenance-branches)).

2. **The commit messages parse.** The notes are generated from Conventional Commit subjects in the
   range, so a subject the convention does not cover reaches readers as a line that says nothing.

   ```sh
   scripts/check_commit_messages.sh
   ```

3. **The engine is served for both architectures.** The browser package depends on
   `omaweb-qtwebengine` (ADR 0049), so a reader on an architecture whose engine is missing cannot
   install the release at all. pacman reports it as an unsatisfiable dependency, which looks like a
   broken package rather than a missing one.

   ```sh
   for arch in x86_64 aarch64; do
       printf '%-8s ' "$arch"
       curl -fsSL "https://github.com/villekivela/omaweb/releases/download/repo-$arch/omaweb.db" \
           | tar tz 2>/dev/null | sed -n 's|^\([^/]*\)/$|\1|p' | tr '\n' ' '
       echo
   done
   ```

   Both lines must name an `omaweb-qtwebengine` entry. If one does not, stop: the engine for that
   architecture has to be built and published first, which is the `engine-release` skill and then
   the `Publish the engine` workflow.

4. **The baseline names the engine being served.** `security/baseline.json`'s `qtwebengine` is
   Omaweb's claim about what it is supported on, and Settings marks a build below it as an
   unsupported preview. A release whose baseline is above the engine in the repository reports
   itself unsupported on every reader's machine.

   Read both and compare them by eye. They are two numbers.

5. **The version.** Work it out from the range rather than asking, then put it to the user with the
   reasoning:

   ```sh
   git log "$(git describe --tags --abbrev=0 --match 'v[0-9]*')..HEAD" --oneline
   ```

   A `feat` in the range is a minor bump, `fix` and the rest are a patch bump, and a `!` or a
   `BREAKING CHANGE` in a body is a major one that Omaweb has not reached. Every `v0.*` tag
   publishes as a prerelease, which is deliberate and not something to work around.

## The tag

```sh
git tag v0.7.0
git push origin v0.7.0
```

Nothing else. The workflow reads the tag for the version, because that is where CMake reads it from
too and the two cannot be allowed to disagree (ADR 0028).

## Then watch it

The run has four jobs that matter and they fail in different ways.

```sh
gh run watch "$(gh run list --workflow Release --limit 1 --json databaseId --jq '.[0].databaseId')"
```

- **`package`** builds both architectures. An `aarch64` failure is usually the community Arch Linux
  ARM image rather than the change (ADR 0044).
- **`release`** publishes the notes. It rewrites them with a model, and on a failure it publishes
  the generated notes rather than none.
- **`publish-repo`** is the upgrade path. It ends green while saying nothing was published if the
  signing key secret is missing, so read its summary rather than its colour. If it fails after
  `release` has published, don't re-run it: a re-run uses the tag's copy of the workflow, broken
  step and all. Fix the cause on `main`, then run
  `gh workflow run "Publish the engine" -f tag=<the version tag>`, which takes the browser packages
  off the release and publishes them the same way.

Then check the thing a reader will do, because none of the above proves it:

```sh
curl -fsSL https://github.com/villekivela/omaweb/releases/download/repo-x86_64/omaweb.db \
    | tar tz | sed -n 's|^\([^/]*\)/$|\1|p'
```

The new version has to be listed, and the engine still beside it.

## Report

Tell the user the version, what the notes say, which packages are attached, and what the pacman
repository now serves. If any job failed, say which and what it means for a reader: a failed
`publish-repo` leaves the release downloadable and the upgrade path stale, which is a different
problem from a failed `package`.

## What not to do

- Do not tag to find out whether the pipeline works. `gh workflow run Release --ref <branch>` builds
  both packages and publishes nothing, which is the dry run.
- Do not move or delete a pushed tag. The release notes, the packages and the repository all name
  it, and a moved tag makes those three disagree about what a version is.
- Do not raise `security/baseline.json` here. That is the engine's own procedure and it requires a
  qualified build, not a release.
