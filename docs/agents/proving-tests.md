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
