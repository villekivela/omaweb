# Brave procedural selector matcher (vendored)

A byte-for-byte copy of brave-core's
`components/cosmetic_filters/resources/data/procedural_filters.ts`, with brave-core's `LICENSE`,
pinned to a commit by `MANIFEST.json`. brave-core is licensed under the Mozilla Public License 2.0,
like Omaweb; see `THIRD_PARTY_NOTICES.md` at the repository root.

The matcher finds the elements a procedural cosmetic rule names. It takes the `selector` half of the
JSON `adblock-rust` emits for a rule, a list of `{type, arg}` operators, and returns the matching
elements. It does nothing to them: hiding, `:style()`, `:remove()`, `:remove-attr()` and
`:remove-class()` are Omaweb's own. `docs/adr/0052-apply-procedural-cosmetic-filters.md` records
why this matcher and not another.

## procedural_filters.js

The bundle the engine loads, built from the copy by esbuild as a classic script that defines one
global, `omawebProceduralFilters`, exposing `compileProceduralSelector`, `applyCompiledSelector`
and `compileAndApplyProceduralSelector`. A `WebEngineScript` cannot load an ES module, and `tsc`
cannot emit a script with a global name. The engine loads the bundle into `ApplicationWorld` only,
so the page never sees the global.

The bundle is committed and its digest pinned in `MANIFEST.json`, so a build needs neither Node nor
the network. Node and the esbuild version pinned in the repository's `package.json` are vendoring
tools here, not build dependencies.

## Nothing here is edited

`ctest` fails on a local edit, an untracked file under `src/`, or a bundle that does not match its
digest (`omaweb-vendored-procedural-filters`), and so does the sync script's `--verify`.
`tests/content-blocking/procedural-rules.json` holds one rule per operator and action with the JSON
the pinned parser emits for it, and `omaweb-qt-engine-contract` runs the bundle on each, so a
matcher that renames an operator fails a test rather than a page.

## Syncing

```sh
npm ci                                                # installs the pinned esbuild
scripts/sync_procedural_filters.py --verify           # the local tree matches the manifest
scripts/sync_procedural_filters.py --check-upstream   # what changed since the pin
scripts/sync_procedural_filters.py --sync --ref <brave-core commit>
```

A sync re-fetches the copy and rebuilds the bundle from it. Review the upstream diff before taking
it: the bundle runs inside the pages the browser loads.
