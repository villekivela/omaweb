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

5. **Verify.** This is the gate, and it runs twice for a reason:

   ```sh
   ~/Projects/villekivela/omaweb-qtwebengine-patches/scripts/verify.sh <tree>
   ```

   All 22 must pass on the patched engine. Four must fail or crash on a stock one:
   `serviceWorkerLocalization`, `enableAfterStoragePathChange`, `tabsWindowsAndScripting`,
   `nativeMessaging`. A test that stopped failing on stock Qt has stopped proving anything, and that
   is worth more attention than a test that went red.

6. **Report, and say what is next.** Tell the user what applied, what the build did, what the gate
   said, and what a person now has to decide. If the series changed, commit and push it in the
   patches repository, and add a row to the table in its `README.md`: release, applying, build,
   tests, people's time.

## When it is worth shipping

That decision is the user's, not this skill's. Give them what it costs: whether the release carries
security fixes, whether the series needed work, and what the gate said. Then the Hetzner build runs
from CI, triggered by hand with the version.

## What not to do

- Do not rent a machine to find a problem. This runs where you are.
- Do not package or sign here. That happens after the Linux builds, where the key is.
- Do not update `security/baseline.json` in the Omaweb repository until an Omaweb build has actually
  been qualified against the new engine.
