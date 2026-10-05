# Build the x86_64 engine with clang and ThinLTO

Amends [0049](0049-ship-omawebs-own-engine-build.md), which ships Omaweb's own build of QtWebEngine
and left the compiler to Qt's default, GCC.

The engine is built the way Chrome is built where that was measured to be faster: clang and LLD,
with ThinLTO, Chrome's PGO profile for the engine's Chromium, and V8's profile for its builtins.
That is x86_64. aarch64, which has no Chrome profile, measured no faster and keeps GCC. The series
repository's `scripts/engine-toolchain.sh` names it, the local and rented builds read that one
answer, and `OMAWEB_ENGINE_TOOLCHAIN` overrides it for a single build. `refresh.sh` run by hand
still configures Qt's own choice, GCC, unless asked for clang. Measured and decided in
[#575](https://github.com/villekivela/omaweb/issues/575).

## What was measured

Each architecture was measured with the same Omaweb binary and harness, against Chromium 140, three
runs of each suite, on 6.11.2 with V8's write barriers on: patch 0018, or in #356's rows the same
two changes as a local backport.

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
with clang: −2.6% and +0.1%, and +1.2% and +0.7%. Omaweb's own three runs spread by 0.8% to 4.6%
within a session, so both differences are within the spread and aarch64 keeps GCC. The second clang
session's 0.973 on JetStream is one low run of Chromium 140, 359.09 against 385.71 and 397.68, not a
faster Omaweb, so Omaweb's own scores are the comparison. aarch64 is measured again when Chrome
publishes an aarch64 profile or the series moves to a new Chromium.

## How the PGO profile follows the Chromium

Chrome's PGO profile is only good for the Chromium it was taken from, so it is never chosen by hand.
The build reads the Chromium version from the tree's `chrome/VERSION`, reads the PGO profile's name
from `chrome/build/linux.pgo.txt` at that Chromium tag, and takes that object from Chrome's profile
bucket, held to the MD5 the bucket publishes. Qt's copy of Chromium leaves the name file out, so it
is read from Chromium's own tag and put back where Chromium's build expects it. V8's builtins
profile is taken the same way for the V8 version in `v8/include/v8-version.h`, and V8 applies its
x64 profile on arm64. A Chromium bump therefore brings its own profile with no change to the recipe.

A function Qt or the series changed has no profile that fits and is compiled without one. V8 rejects
its PGO profile for some builtins for the same reason, because Qt's V8 is not built quite as
Chrome's, and the patch that turns PGO on makes V8 build those without the PGO profile rather than
stop. So the engine gets less from PGO than Chrome does.

## What it costs

A clang build needs a build volume of its own, because a build directory configured for one compiler
refuses the other, and a patch outside the series, because Qt turns ThinLTO on only when Qt itself
was built for LLD and turns PGO on never. That patch changes how the engine is compiled, not what
its code does, and the package's `MODIFICATIONS.md` says so.

The link is not what limits a build's memory. Sampled every five seconds as the container's
anonymous memory, what its processes hold without the page cache, the ThinLTO link of
`libQt6WebEngineCore.so` peaked at 3.5 GB with sixteen threads on x86_64 and 2.5 GB with six on
aarch64, with an empty ThinLTO cache. The aarch64 compile peaked at 13.0 GB on the same measure at
six jobs; the x86_64 build was incremental, so its compile was not measured. The local build's
`OMAWEB_ENGINE_MEMORY` caps the container and sizes the compile's jobs, so a machine with less
memory sets it lower. Both builds ran locally, so whether Hetzner's builders complete the link is
unverified.

Moving the series to 6.140.0 is measured again on both architectures, because the PGO profiles and
the toolchain's gain move with the Chromium.
