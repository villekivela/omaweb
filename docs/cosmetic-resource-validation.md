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

## Native Wayland measurements

Baseline `b1b79de` and implementation `702ef92`, measured on one Omarchy virtual machine in a native
Wayland session. These are virtual machine measurements. They do not describe what the same
revisions do on bare metal.

### The machine

- Arch Linux ARM, aarch64, kernel 7.2.0-2, under QEMU.
- Hyprland 0.56.1, `XDG_SESSION_TYPE=wayland`. The harness runs with `QT_QPA_PLATFORM=wayland`, so
  neither XWayland nor an offscreen platform is involved.
- 8 virtual processors, 3.8 GiB of memory.
- Qt 6.11.2, QtWebEngine 6.11.2, Chromium 140.0.7339.225, rustc 1.98.0, adblock 0.12.5.
- No hardware graphics acceleration. Chromium reports
  `eglCreateContext: Requested version is not supported` and falls back to software rendering. That
  raises paint cost on both revisions and compresses whatever difference a rendering change could
  show.
- The host never suspended during the run.

### Reproducing

```sh
python3 scripts/benchmark_cosmetic_resources.py \
    --baseline b1b79de --implementation 702ef92 \
    --lists .cache/benchmark/lists --out .cache/benchmark
```

The driver builds each revision in a worktree of its own, with the same Release preset and the same
build options, copies in `tests/benchmarks`, and applies `tests/benchmarks/instrument.py`. It
alternates batches between the two revisions so drift in the machine falls on both.

### Instrumentation

Instrumentation is kept apart from the behaviour being compared. `instrument.py` adds two counters
to `EngineView.qml` on both revisions: one for the scripts the cosmetic path sends into the page,
one for the survey replies it takes back. Neither changes a decision.

The implementation already counts assembled URL resources. The baseline does not, so the same script
adds a counter to its Rust blocker that increments immediately before each of the four
`url_cosmetic_resources` calls. It counts the calls the baseline makes. It does not give the
baseline the implementation's cache.

`tests/benchmarks/cosmetic_benchmark.cpp` drives the real `EngineView.qml` over a real compiled
matcher and never stands in for either.

### Lists and fixtures

Both revisions compile one EasyList snapshot, one EasyPrivacy snapshot, and the fixture's rules.

| Snapshot    | SHA-256                                                            |
| ----------- | ------------------------------------------------------------------ |
| EasyList    | `b35e134ede08f268a38dca3b606ba030e23a2322a167f807503e34bcd27c94ac` |
| EasyPrivacy | `a7e6d8078d99b27602094a5ee051374b3291e21d49bea1fc5b78f09e362dbf9d` |

| Fixture    | Page bytes | Elements | Classes | Ids  | Site CSS | Scriptlet source |
| ---------- | ---------- | -------- | ------- | ---- | -------- | ---------------- |
| site-css   | 54449      | 800      | 575     | 400  | 32237    | 16060            |
| scriptlets | 54449      | 800      | 575     | 400  | 32237    | 120342           |
| large-dom  | 797512     | 12000    | 8041    | 6000 | 32237    | 16060            |

Each fixture answers on a hostname of its own under `.localhost`, which Chromium resolves to the
loopback address without touching name resolution. A bare loopback address cannot be used: EasyList
carries `@@://127.0.0.1$generichide`, which turns the generic survey off and removes half of what is
measured. The generic class and id names come out of the pinned lists in file order, so the pages
stay valid when the pins move and are identical for both revisions.

The scriptlet fixture carries 38 `##+js(...)` rules from the vendored library and assembles 120342
bytes of scriptlet source against 16060 for a fixture with one.

### Conditions

A cold navigation is the first navigation of a fresh process: a freshly compiled matcher with an
empty cosmetic cache, a fresh profile directory, and no renderer yet. Each cold sample is its own
process.

A warm navigation is a repeat navigation inside one process. The process, the renderer, the compiled
matcher, and its cosmetic cache persist. The HTTP cache does not serve the fixture: it is sent with
`Cache-Control: no-store`, so the document is fetched again every time. A blank page replaces the
document between measured navigations, and the harness waits for that page's own cosmetic work to
finish before opening the measured window. Warm-up navigations are discarded.

### Metric definitions

- URL resource assemblies: calls reaching `url_cosmetic_resources` inside the Rust blocker.
- Cosmetic questions at the blocker: the four cosmetic calls `EngineView.qml` makes across the QML
  boundary. This counts questions asked, not the assemblies they cost.
- Cosmetic scripts into the page: `runJavaScript` calls the cosmetic path issues.
- Survey callbacks: replies to the generic survey that reach QML.
- Site stylesheet text mutations: mutations of the injected site style node, observed by the page
  with a MutationObserver from its own first script.
- CPU: user plus system time from `/proc`, summed over the browser process and every descendant,
  which includes Chromium's renderer, GPU, and utility processes. It covers the measured window,
  from just before the navigation until the page reports itself settled. Wall-clock time is never
  substituted for it.
- First paint and largest contentful paint: the page's own `PerformanceObserver` entries.
- Style recalculations and their CPU: `UpdateLayoutTree` events from a Chromium JSON trace,
  attributed to a navigation by timestamp. Chromium stamps trace events with `CLOCK_MONOTONIC`
  microseconds, the clock the harness records its navigation boundaries on. The duration is the
  event's thread CPU time.

Tracing changes what a run costs, so it runs as a separate pass. The counting pass carries the
lookup, call, CPU, and paint figures and runs untraced. The tracing pass is read only for style
recalculation, at 10 samples per revision rather than 20.

### Deterministic results

Every figure in this table had a standard deviation of zero across 20 measured navigations per
revision, on every fixture and both conditions. They are counts of work done, not timings.

| Metric                            | Cold baseline | Cold implementation | Warm baseline | Warm implementation |
| --------------------------------- | ------------- | ------------------- | ------------- | ------------------- |
| URL resource assemblies           | 5             | 1                   | 5             | 0                   |
| Cosmetic questions at the blocker | 5             | 5                   | 5             | 5                   |
| Cosmetic scripts into the page    | 3             | 2                   | 3             | 2                   |
| Survey callbacks                  | 1             | 1                   | 1             | 1                   |
| Site stylesheet text mutations    | 1             | 0                   | 1             | 0                   |

The blocker boundary count is unchanged at five. The view still asks the same five questions; the
saving is inside the matcher, which now assembles the answer once instead of five times. Warm
navigations assemble nothing at all, because the cache outlives the document.

Installed stylesheet sizes and the count of hidden elements agreed between the two revisions on
every navigation, so the comparison is between two builds doing the same blocking.

### Timing results

Median / p95 (standard deviation). CPU and paint come from 20 navigations per revision, style
recalculation from 10.

#### site-css

| Metric                  | Condition | Baseline                      | Implementation                |
| ----------------------- | --------- | ----------------------------- | ----------------------------- |
| CPU                     | cold      | 150 ms / 160 ms (sd 12)       | 140 ms / 170 ms (sd 16)       |
| CPU                     | warm      | 70 ms / 80 ms (sd 11)         | 70 ms / 80 ms (sd 9.4)        |
| Style recalculations    | cold      | 5 / 6 (sd 1.2)                | 4 / 5 (sd 0.74)               |
| Style recalculations    | warm      | 2 / 3 (sd 0.48)               | 2 / 2 (sd 0)                  |
| Style recalculation CPU | cold      | 4.178 ms / 4.58 ms (sd 0.41)  | 3.458 ms / 3.743 ms (sd 0.2)  |
| Style recalculation CPU | warm      | 4.473 ms / 5.559 ms (sd 0.48) | 4.063 ms / 4.703 ms (sd 0.29) |

#### scriptlets

| Metric                  | Condition | Baseline                      | Implementation                |
| ----------------------- | --------- | ----------------------------- | ----------------------------- |
| CPU                     | cold      | 170 ms / 180 ms (sd 7.6)      | 170 ms / 190 ms (sd 15)       |
| CPU                     | warm      | 100 ms / 110 ms (sd 8.8)      | 90 ms / 100 ms (sd 8.9)       |
| Style recalculations    | cold      | 5 / 6 (sd 0.79)               | 5 / 5 (sd 0.52)               |
| Style recalculations    | warm      | 2 / 3 (sd 0.48)               | 2 / 2 (sd 0)                  |
| Style recalculation CPU | cold      | 3.603 ms / 4.052 ms (sd 0.24) | 3.036 ms / 3.181 ms (sd 0.12) |
| Style recalculation CPU | warm      | 4.03 ms / 4.401 ms (sd 0.27)  | 3.522 ms / 4.396 ms (sd 0.42) |

#### large-dom

| Metric                  | Condition | Baseline                    | Implementation                |
| ----------------------- | --------- | --------------------------- | ----------------------------- |
| CPU                     | cold      | 440 ms / 460 ms (sd 20)     | 430 ms / 500 ms (sd 29)       |
| CPU                     | warm      | 340 ms / 360 ms (sd 15)     | 320 ms / 360 ms (sd 18)       |
| Style recalculations    | cold      | 6.5 / 8 (sd 1.2)            | 5 / 6 (sd 1.2)                |
| Style recalculations    | warm      | 4 / 5 (sd 0.88)             | 2 / 4 (sd 0.71)               |
| Style recalculation CPU | cold      | 49.91 ms / 59.3 ms (sd 3.6) | 40.23 ms / 41.13 ms (sd 0.86) |
| Style recalculation CPU | warm      | 52.1 ms / 57.84 ms (sd 2.6) | 41.83 ms / 43.83 ms (sd 1.5)  |

### What the timings do and do not show

Style recalculation is the one place where a rendering difference is unambiguous, and only on the
large DOM. There the two sets of samples do not overlap at all:

| Condition | Baseline min / max  | Implementation min / max |
| --------- | ------------------- | ------------------------ |
| cold      | 45.71 ms / 59.30 ms | 38.81 ms / 41.13 ms      |
| warm      | 48.94 ms / 57.84 ms | 39.08 ms / 43.83 ms      |

The baseline's cheapest recalculation is dearer than the implementation's dearest, in both
conditions, across ten samples each. That is a real reduction of roughly a fifth, and it is what
skipping the load-completion stylesheet rewrite buys: the rewrite invalidated style for a document
with 12000 elements.

On the two small fixtures the same measurement separates nothing. Style recalculation CPU there is a
few milliseconds either way, and the difference sits inside the spread.

Whole-process CPU never separates on any fixture. The largest gap is the large DOM warm case at 340
ms against 320 ms, with standard deviations of 15 and 18. The measured window includes a fixed
settling interval and the renderer's own startup, so a ten millisecond saving in style recalculation
is not visible against it.

First paint and largest contentful paint show no reliable difference, and the median is a poor
summary of them here because the distribution is bimodal. The large DOM warm samples cluster in two
groups about 30 ms apart:

```text
baseline        [48, 52, 52, 52, 52, 52, 52, 52, 52, 56, 80, 80, 80, 80, 84, 84, 84, 84, 88, 88]
                10 samples below 70 ms, 10 above
implementation  [48, 52, 52, 52, 52, 52, 52, 80, 80, 84, 84, 84, 84, 84, 84, 88, 88, 88, 88, 88]
                7 samples below 70 ms, 13 above
```

The medians land in the empty gap between the clusters, at values no sample took. The split differs,
ten against seven in the lower cluster, which at twenty samples is not a difference. No paint or
largest contentful paint claim is made in either direction.

### Limits

- These are virtual machine measurements on software rendering. Paint costs are inflated and a
  rendering change has less room to show. Bare metal is not measured and no claim is made for it.
- Style recalculation is measured on ten navigations per revision, not twenty, because tracing
  changes what a run costs and runs as its own pass.
- The site rules in each fixture are written by the fixture, against a host the pinned lists have
  never seen. The generic rules, which are the expensive half, come from the lists themselves.
- The harness attaches no network request interceptor, so network blocking is outside these numbers.
  The issue is about cosmetic resources.
- Raw samples and traces are kept outside the repository, under `.cache/benchmark/samples` and
  `.cache/benchmark/traces` on the measuring host: 210 sample files and 66 traces for this run.

## Late surveys

Issue #317 moves the generic survey into a script of each frame's own, run when its DOM is parsed,
and keeps a watch on the document afterwards that reports names the frame has not been asked about.
The stable-page figures above must hold, and a page that keeps adding to itself must pay a bounded
cost. The `churn` fixture is the `site-css` page plus a script that adds ten bursts after load, 80
ms apart, each carrying ten classes the pinned lists hide that the page did not carry at load, and
ten it did.

Baseline `0a594b1` against implementation `4a32bf0`, on the same virtual machine as above, Qt
6.11.2, in a native Wayland session. The root filesystem had 330 MB free, which rules out the two
release worktrees `scripts/benchmark_cosmetic_resources.py` builds, so this run used one harness
binary from the `ci` preset, a Debug build, and swapped `src/engine/qt/EngineView.qml` between the
two revisions, instrumented by `tests/benchmarks/instrument.py`. The harness loads that file from
the source tree at start, and no C++ the harness links differs between the revisions. Warm batches
of five navigations alternated between the revisions four times, cold navigations five times each.
The Debug build inflates every timing against the tables above; the two columns of one table are
comparable with each other and with nothing else. Each cell reads median / p95 (standard deviation,
sample count).

### site-css

| Metric                   | Condition | Baseline                      | Implementation               |
| ------------------------ | --------- | ----------------------------- | ---------------------------- |
| URL resource assemblies  | warm      | 0 / 0 (sd 0, n=20)            | 0 / 0 (sd 0, n=20)           |
| Cosmetic questions       | warm      | 5 / 5 (sd 0, n=20)            | 5 / 5 (sd 0, n=20)           |
| Scripts into the page    | warm      | 2 / 2 (sd 0, n=20)            | 1 / 1 (sd 0, n=20)           |
| Survey replies           | warm      | 1 / 1 (sd 0, n=20)            | 1 / 1 (sd 0, n=20)           |
| Site sheet mutations     | warm      | 1 / 1 (sd 0, n=20)            | 1 / 1 (sd 0, n=20)           |
| Hidden of the 200 sample | warm      | 200 / 200 (sd 0, n=20)        | 200 / 200 (sd 0, n=20)       |
| CPU                      | warm      | 410 ms / 750 ms (sd 206)      | 255 ms / 420 ms (sd 130)     |
| CPU                      | cold      | 210 ms / 490 ms (sd 117, n=5) | 210 ms / 310 ms (sd 42, n=5) |

The blocker answers the same five questions and assembles the same one result. The survey's own
round trip is gone: the page posts its survey rather than being asked, so one script goes in, the
answer, where two did. CPU does not separate in either direction at this spread.

### churn

| Metric                            | Condition | Baseline                        | Implementation                  |
| --------------------------------- | --------- | ------------------------------- | ------------------------------- |
| Cosmetic questions                | warm      | 5 / 5 (sd 0, n=20)              | 23 / 23 (sd 0.44, n=20)         |
| Scripts into the page             | warm      | 2 / 2 (sd 0, n=20)              | 10 / 10 (sd 0.22, n=20)         |
| Survey replies                    | warm      | 1 / 1 (sd 0, n=20)              | 10 / 10 (sd 0.22, n=20)         |
| Elements added after load, hidden | warm      | 0 of 100                        | 100 of 100                      |
| CPU                               | warm      | 385 ms / 630 ms (sd 166)        | 575 ms / 930 ms (sd 276)        |
| CPU                               | cold      | 1270 ms / 2100 ms (sd 376, n=5) | 1870 ms / 2770 ms (sd 763, n=5) |

Ten bursts cost nine late reports, each two questions at the blocker and one script into the page,
and 24 of 25 navigations counted exactly that; one merged two bursts into one report, which the 100
ms hold allows. The baseline hid none of the hundred late elements. The late reports and the style
work for the elements they hide are what the CPU column pays for.

## Scripts in place across a navigation

Issue #321 moves the Content blocking script from the view's script collection to the Engine
profile's, matched to its origin, and stops the view from assigning its own collection whole at each
load ([ADR 0048](adr/0048-install-the-content-blocking-script-on-the-engine-profile.md)). The
stable-page figures above must hold. On the same virtual machine, Qt 6.11.2, offscreen, one `ci`
harness binary with the instrumented `EngineView.qml` of
`fix/321-script-each-document-for-its-own-address` in place, seven warm `site-css` navigations, two
of them warm-up:

| Metric                   | Every navigation |
| ------------------------ | ---------------- |
| Cosmetic questions       | 5                |
| Scripts into the page    | 1                |
| Survey replies           | 1                |
| URL resource assemblies  | 0                |
| Hidden of the 200 sample | 200              |

The two questions the script costs, the site stylesheet and the scriptlet source, are asked at the
navigation request instead of the load start, once per navigation as before.

The contract tests are the rest of the validation. `qtScriptsEveryDocumentForItsOwnAddress` loads 21
documents alternating between the two loopback names, so every load but the first changes renderer
process, through a cross-site redirect, a same-site one, and none, and reads each page's state at
its first script and once settled, subframe included. `qtRunsScriptletsBeforeThePageRuns` reloads 20
times with the site turned off and on by turns. On main, the same matrix runs the script of the page
being left in every navigation to another site, and the set-out address's script in every redirect
target. Both tests passed six runs of the suite in a row here, and the `redirect` row of
`qtHidesCosmeticRulesBeforeThePageRuns` with them.
