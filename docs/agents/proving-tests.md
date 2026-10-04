# Proving a test

A test proves a criterion only if it goes **red** when the code it covers is broken. A pull request
shows that with two things:

- A table that maps each acceptance criterion of the issue to the test or check that proves it.
- A break proof: for each test, the code broken on purpose, the test failing, then the code restored
  and the test passing again. Run each test once with nothing broken as a control, in the same
  script.

## Caches that keep a restored break running

A script that breaks a file, runs a test and restores it can leave the broken version running:

- **The QML disk cache.** Builds that read QML from `src/ui` reuse the compiled `.qmlc` of the
  broken file when the restored file's modification time goes backwards, as copying a backup back
  does. Run the UI tests with `QML_DISABLE_DISK_CACHE=1`.
- **Ninja.** A restored C++ file older than its object is not rebuilt, so the libraries and test
  binaries stay built from the break. `touch` every restored file.

The symptom is a test that passed before the break run and fails after it, or a core test failing on
the last break's assertion.

A break that makes the app recurse, such as a Space switch inside a handler for Space switches,
hangs the test binary. Give every run a timeout.

## Tests of eased movement

The `BrowserChrome` tests in `tests/ui/tst_main.qml` check an eased movement by watching it, not by
polling it. Start `watchChanges(item, prop)` before the input, then assert with
`passedBetween(watch, from, to)`. `watchFrames(read)` reads what a frame draws, consistently across
items. `init()` stops any watch a failed test left behind and waits for the sidebar to rest.

`wait()` and `tryVerify` miss movements on a loaded machine, where one poll can outlast a 120 ms
slide. Qt also credits a starting animation with up to 50 ms from before it started, so a starved
120 ms animation can end 70 ms after the input; `passedBetween` accepts a movement seen only at its
end after 60 ms.

To soak them, run the plain and themed UI suites side by side, 100 times, in an arm64 container
limited to four CPUs (`--cpus 4`) with `stress-ng --cpu 4` beside them. The binary reads QML live
from `src/ui`, so nothing may edit `src/ui` or run a break proof while a soak runs.
