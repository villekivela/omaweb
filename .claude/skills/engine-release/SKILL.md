---
name: engine-release
description:
  Qualify the QtWebEngine patch series against a Qt release, on this machine. Use when a baseline or
  series-applies issue says a new Qt release needs answering, or when the user asks to rebase,
  qualify, or check the engine patches.
---

# Qualifying an engine release

Answer one question: does the patch series still produce an engine worth shipping? Everything here
runs on the machine you are at. The rented builders come later and are not for finding problems.

The series lives in `~/Projects/villekivela/omaweb-qtwebengine-patches`. Its `PROCESS.md` is the
source of truth for the routine; this skill runs it and reports.

## What to do

1. **Find the version.** The issue that brought you here names it. Otherwise ask Qt directly:

   ```sh
   curl -fsSL https://download.qt.io/official_releases/qt/6.11/ | grep -o 'href="6\.11\.[0-9]*/"'
   ```

2. **Read what the release carries** before spending hours on it. Qt's release notes say whether it
   has security fixes, which is what decides urgency.

3. **Apply the series.** This is minutes and it is where most trouble appears:

   ```sh
   ~/Projects/villekivela/omaweb-qtwebengine-patches/scripts/refresh.sh <version>
   ```

   A conflict stops it and names the patch. Resolve it in the tree, then `git am --continue`, then
   export the series back with
   `git format-patch -o <patches-repo>/patches v<version>-tarball..HEAD`. Never resolve a conflict
   on a builder.

4. **Build.** Hours, unattended. Run it detached and check on it rather than waiting:

   ```sh
   nohup <tree>/build.sh > <tree>/build.log 2>&1 &
   ```

   Watch for `^exit ` in the log. On a failure, the compiler usually names an API that moved under
   the patches.

   This is the qualifying build, on the machine you are at. Once it and the gate below are clean,
   start the release builds, which are the ones whose artifacts ship:

   ```sh
   gh workflow run "Build the engine" --repo villekivela/omaweb-qtwebengine-patches \
       -f version=<version> -f arch=both
   ```

   Those run on machines rented for the job and destroyed afterwards, apply the series again, run
   the gate again, and refuse to package if it does not pass. They also package what they built, so
   nothing needs packaging by hand afterwards.

5. **Verify.** This is the gate, and it runs twice for a reason:

   ```sh
   ~/Projects/villekivela/omaweb-qtwebengine-patches/scripts/verify.sh <tree>
   ```

   Every case must pass on the patched engine. Do not check the count against a number written down
   here: the gate reads `Totals: N passed, 0 failed` precisely because a gate naming a number
   refuses the day the suite grows, which happened once already.

   The second pass runs four cases against a _stock_ engine, where each must fail or crash:
   `serviceWorkerLocalization`, `enableAfterStoragePathChange`, `tabsWindowsAndScripting`,
   `nativeMessaging`. A test that stopped failing on stock Qt has stopped proving anything, and that
   is worth more attention than a test that went red. This pass is skipped where no stock engine is
   installed, and it says so rather than passing quietly.

   That list is narrower than the patches deserve. `webAccessibleResource`,
   `loadAlreadyLoadedExtension` and `extensionDocumentClosesItsOwnWindow` each cover a patch that a
   stock engine fails, and none is in it. Adding them is worth doing on a machine where the stock
   comparison actually runs.

6. **Package.** The build does this itself now, in the container that built the engine and on a
   machine of the right architecture, so `out/` holds an unsigned `.pkg.tar.*` beside the tarball
   and a `SHA256SUMS` covering both. Where a build predates that, or a tarball has travelled on its
   own:

   ```sh
   ~/Projects/villekivela/omaweb-qtwebengine-patches/scripts/package-locally.sh <version>
   ```

7. **Publish it.** The signing key is in the Omaweb repository, so signing and publishing happen
   there:

   ```sh
   gh release create engine-<version> --repo villekivela/omaweb \
       --title "Engine <version>" out/*.pkg.tar.*
   gh workflow run "Publish the engine" --repo villekivela/omaweb -f tag=engine-<version>
   ```

   Then read the repository rather than the job summary, because the summary reports what the job
   did and not what a client can get:

   ```sh
   curl -fsSL https://github.com/villekivela/omaweb/releases/download/repo-<arch>/omaweb.db \
       | tar tz | sed -n 's|^\([^/]*\)/$|\1|p'
   ```

   The engine must be listed, and every browser package that was there must still be.

8. **Record it.** The table in the patches repository's `README.md` measures what maintaining the
   series costs, and the row is generated from the run rather than remembered:

   ```sh
   ~/Projects/villekivela/omaweb-qtwebengine-patches/scripts/release-row.sh \
       <version> out/apply.txt out
   ```

   It reads the series' own report and the gate's totals out of the artifacts. It warns on stderr
   when a package for one architecture is missing, which means the release is not finished. The one
   column it cannot know is a person's time, and it says `none` unless the series conflicted.
   Correct that if it cost you an afternoon.

   If the series changed, commit and push it in the patches repository.

9. **Report, and say what is next.** Tell the user what applied, what the build did, what the gate
   said, and what a person now has to decide. The baseline is offered as a pull request by the
   publishing workflow, and it deliberately leaves `chromiumSecurityPatch` and `reviewed` empty:
   those come from the release notes you read in step 2, so filling them in and merging is the last
   thing that has to happen before `cut-release`.

## When it is worth shipping

That decision is the user's, not this skill's. Give them what it costs: whether the release carries
security fixes, whether the series needed work, and what the gate said. Then the Hetzner build runs
from CI, triggered by hand with the version.

## What not to do

- Do not rent a machine to find a problem. This runs where you are.
- Do not sign anything on a rented machine. Packaging there is fine and the build does it; the
  signature is made by the publishing workflow, which holds the key (ADR 0049).
- Do not update `security/baseline.json` in the Omaweb repository until an Omaweb build has actually
  been qualified against the new engine.
