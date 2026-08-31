# uBlock Origin scriptlet library (vendored)

`src/js` is a byte-for-byte copy of uBlock Origin's scriptlet library —
`src/js/resources` plus the three modules it imports from beside it — pinned to a
commit by `MANIFEST.json`. uBlock Origin is licensed under GPL-3.0-or-later; see
`THIRD_PARTY_NOTICES.md` at the repository root.

A `##+js(...)` filter rule names one of these functions and supplies its arguments.
The rule never supplies code: the set of code that can run in a page is the set in
this directory. `docs/adr/0025-run-only-vendored-scriptlets.md` records why, and
why the scriptlets uBO gates behind trust are refused outright here.

## Nothing here is edited

Every file is byte-identical to upstream, which is what makes a sync a review of
upstream's diff rather than a merge. `ctest` fails on a local edit, an untracked
file under `src/`, or a digest that does not match the manifest
(`tanto-vendored-ubo-scriptlets`), and so does the sync script.

## scriptlets.json

`scriptlets.json` is the same library expressed as `adblock-rust` resource
descriptors, which is the form `Engine::use_resources` accepts. The content blocker
builds it into its own binary with `include_str!`, so a build needs neither Node nor
the network.

It is generated, not written. uBO's scriptlets are ES modules that register
themselves at import, so the only honest way to read the set is to import them and
ask — `scripts/build_ubo_scriptlets.mjs` does that under Node, and its output's
digest is pinned in `MANIFEST.json` alongside the copies. Node is a vendoring tool
here, not a build dependency.

The generator refuses a library it cannot express: a resource a filter can name has
to be a plain function declaration, because that is how `adblock-rust` calls it, and
every declared dependency has to resolve to something registered.

## Syncing

```sh
scripts/sync_ubo_scriptlets.py --verify           # the local tree matches the manifest
scripts/sync_ubo_scriptlets.py --check-upstream   # what changed since the pin
scripts/sync_ubo_scriptlets.py --sync --ref 1.70.0
```

A sync re-fetches the copies and regenerates `scriptlets.json` from them. Review the
upstream diff before taking it: this is the one dependency whose contents run inside
the pages the browser loads.
