# uBlock Origin resource library (vendored)

Two byte-for-byte copies of uBlock Origin, pinned to a commit by `MANIFEST.json`.
uBlock Origin is licensed under GPL-3.0-or-later; see `THIRD_PARTY_NOTICES.md` at
the repository root.

`src/js/resources`, plus the three modules it imports from beside it, is the
scriptlet library. A `##+js(...)` filter rule names one of these functions and
supplies its arguments. The rule never supplies code: the set of code that can run
in a page is the set in this directory.
`docs/adr/0025-run-only-vendored-scriptlets.md` records why, and why the scriptlets
uBO gates behind trust are refused outright here.

`src/web_accessible_resources` is the substitute bodies. A `$redirect=` rule names
one of them and Tanto serves it in place of the request the lists refuse, so a page
waiting on a tracker finishes loading rather than stalling. Which of these files a
filter may name is `src/js/redirect-resources.js` beside them, not the directory
listing: the directory also holds uBO's own extension pages, which no rule may ask
for. `docs/adr/0026-serve-substitutes-under-a-tanto-scheme.md` records the rest.

## Nothing here is edited

Every file is byte-identical to upstream, which is what makes a sync a review of
upstream's diff rather than a merge. `ctest` fails on a local edit, an untracked
file under `src/`, or a digest that does not match the manifest
(`tanto-vendored-ubo-scriptlets`), and so does the sync script.

## scriptlets.json and redirects.json

Both files are the copies beside them expressed as `adblock-rust` resource
descriptors, which is the form `Engine::use_resources` accepts. The content blocker
builds them into its own binary with `include_str!`, so a build needs neither Node
nor the network.

They are generated, not written. Both sets describe themselves in JavaScript — uBO's
scriptlets are ES modules that register themselves at import, and the redirectable
bodies are listed in an ES module exporting a `Map` — so the only honest way to read
either set is to import it and ask. `scripts/build_ubo_scriptlets.mjs` and
`scripts/build_ubo_redirects.mjs` do that under Node, and each output's digest is
pinned in `MANIFEST.json` alongside the copies. Node is a vendoring tool here, not a
build dependency.

Each generator refuses a library it cannot express. A scriptlet a filter can name
has to be a plain function declaration, because that is how `adblock-rust` calls it,
and every declared dependency has to resolve to something registered. A substitute
body has to carry a MIME type `adblock-rust` can serve, and one whose entry declares
`params` is a template nothing here can fill and is left out.

## Syncing

```sh
scripts/sync_ubo_scriptlets.py --verify           # the local tree matches the manifest
scripts/sync_ubo_scriptlets.py --check-upstream   # what changed since the pin
scripts/sync_ubo_scriptlets.py --sync --ref 1.70.0
```

A sync re-fetches the copies and regenerates both descriptor files from them. Review
the upstream diff before taking it: this is the one dependency whose contents run
inside the pages the browser loads.
