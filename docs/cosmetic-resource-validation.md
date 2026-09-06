# Cosmetic resource reuse validation

Issue #100 starts from revision `b1b79dedda65746ac1c58985eab4b32fe90b76be`.

## Deterministic checks

The matcher test performs the stable-navigation sequence: site CSS, scriptlet source, site CSS
again, survey eligibility, and generic CSS. Its lookup counter reported five assemblies before
caching and one afterward. The cache holds up to 32 full-URL results per compiled matcher and evicts
the least recently used entry. Recompilation creates a new matcher and cache. Site decisions remain
outside the cache, so disabling a site bypasses cached results.

The Chromium fixture observes the site style node with a MutationObserver. Ordinary completion
reported one text mutation before the change and zero afterward. Missing and changed stylesheets are
repaired. The fixture also covers redirects to a hostname with different CSS and disabling and
re-enabling blocking. Separate tests cover obsolete surveys after same-URL reloads, rule changes,
and site configuration changes.

For a successful load with site CSS and matching generic selectors, the source now issues two
cosmetic script calls after loading instead of three. Site-style verification shares the DOM survey
call; the second call installs generic CSS. This is a source-level count, not a browser trace
measurement.

These checks ran on macOS 26.6.2, arm64, using the CI Debug preset, Qt 6.11.1, and the release-built
Rust matcher with adblock 0.12.5:

```sh
scripts/bootstrap_content_blocker.sh
cmake --build --preset ci
ctest --preset ci -R 'omaweb-content-blocker'
build/ci/omaweb-qt-engine-contract-tests qtHidesCosmeticRulesBeforeThePageRuns \
  qtRejectsObsoleteCosmeticSurveys qtRunsScriptletsBeforeThePageRuns
```

## Pending native Wayland measurements

Rendering gains remain unproven. This macOS run does not satisfy the issue's native Wayland
benchmark requirement.

Compare the baseline and changed revisions in Release builds on the same native Wayland host. Record
hardware, compositor, Qt, Chromium, Rust, and adblock versions. Use identical, pinned EasyList and
EasyPrivacy snapshots, a fixture with many applicable scriptlets, and a large DOM. Record list
hashes and fixture sizes with the results.

For each fixture, record cold and warm URL-resource lookup counts, CPU duration, cosmetic script
round trips, style recalculation, first paint, and LCP. Repeat the same navigation sequence on both
revisions and report the sample count and variation. Use `ContentMatcher::cosmeticLookupCount()` for
resource assemblies and Chromium tracing for rendering and script timing. Do not infer paint or LCP
improvements from the deterministic lookup and mutation counts.
