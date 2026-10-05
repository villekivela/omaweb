# Build the engine with clang and ThinLTO

Amends [0049](0049-ship-omawebs-own-engine-build.md), which ships Omaweb's own build of QtWebEngine
and left the compiler to Qt's default, GCC.

The engine is built the way Chrome is built where that was measured to be faster: clang and LLD,
with ThinLTO, Chrome's PGO profile for the engine's Chromium, and V8's profile for its builtins.
That is x86_64. aarch64, which has no Chrome profile, measured no faster and keeps GCC. The series
repository's `scripts/engine-toolchain.sh` names it, every build reads that one answer, and
`OMAWEB_ENGINE_TOOLCHAIN` overrides it for a single build.

## What was measured

Each architecture was measured with the same Omaweb binary and harness, against Chromium 140, three
runs of each suite, on 6.11.2 with V8's write barriers on (patch 0018).

| Architecture | Engine                      | Speedometer 3.1 ÷ Chromium 140 | JetStream 2.2 ÷ Chromium 140 |
| ------------ | --------------------------- | ------------------------------ | ---------------------------- |
| x86_64       | GCC                         | 0.727                          | 0.917                        |
| x86_64       | clang, LLD, ThinLTO, PGO    | 0.774                          | 0.924                        |
| x86_64       | the same, measured again    | 0.777                          | 0.942                        |
| aarch64      | GCC                         | 0.789                          | 0.943                        |
| aarch64      | clang, LLD, ThinLTO, no PGO | 0.756 and 0.785                | 0.959 and 0.973              |

x86_64 is a Ryzen 7 PRO 7840HS composited on its GPU: the first two rows are
[#356](https://github.com/villekivela/omaweb/issues/356), the third is #575. Speedometer gains about
6% on Omaweb's own score there, which is outside Chromium's spread of under 3%, so x86_64 builds
with clang.

aarch64 is an Apple M2 Max in an arm64 Linux container under headless cage, all rows from one
evening, GCC between the two clang sessions. Chrome publishes no Linux PGO profile for aarch64, so
clang there gets ThinLTO and V8's builtins profile only. Omaweb's own Speedometer score was 24.74
with GCC and 24.10 and 24.77 with clang, and its JetStream score 368.2 with GCC and 372.6 and 370.7
with clang. Both differences are within the run-to-run spread of 1% to 3%, so aarch64 keeps GCC. It
is measured again when Chrome publishes an aarch64 profile or the series moves to a new Chromium.

## How the profile follows the Chromium

Chrome's PGO profile is only good for the Chromium it was taken from, so it is never chosen by hand.
The build reads the Chromium version from the tree's `chrome/VERSION`, reads the profile's name from
`chrome/build/linux.pgo.txt` at that Chromium tag, and takes that object from Chrome's profile
bucket, held to the MD5 the bucket publishes. Qt's copy of Chromium leaves the name file out, so it
is read from Chromium's own tag and put back where Chromium's build expects it. V8's builtins
profile is taken the same way for the V8 version in `v8/include/v8-version.h`, and V8 applies its
x64 profile on arm64. A Chromium bump therefore brings its own profile with no change to the recipe.

A function Qt or the series changed has no profile that fits and is compiled without one. V8 rejects
its profile for some builtins for the same reason, because Qt's V8 is not built quite as Chrome's,
and the patch that turns PGO on makes V8 build those without the profile rather than stop. So the
engine gets less from PGO than Chrome does.

## What it costs

A clang build needs a work space of its own, because a build directory configured for one compiler
refuses the other, and a patch outside the series, because Qt turns ThinLTO on only when Qt itself
was built for LLD and turns PGO on never. That patch changes how the engine is compiled, not what
its code does, and the package's `MODIFICATIONS.md` says so.

The link is not what limits a build's memory. Sampled every five seconds with an empty cache, the
ThinLTO link of `libQt6WebEngineCore.so` peaked at about 5 GB with sixteen threads on x86_64 and 4.3
GB with six on aarch64, page cache included, against 13 GB for the compile at six jobs. The local
build's `OMAWEB_ENGINE_MEMORY` caps the container and sizes the compile's jobs, so a machine with
less memory sets it lower. Whether Hetzner's builders fit is checked when the engine is next
published, since both builds here ran locally.

Moving the series to 6.140.0 is measured again on both architectures, because the profiles and the
toolchain's gain move with the Chromium.
