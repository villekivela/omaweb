# Code style and quality gates

These rules apply to first-party source, tests, build files, and automation. Vendored, generated,
cached, and archived files keep their upstream or historical formatting.

## Common rules

- Use UTF-8, LF line endings, and one final newline.
- Remove trailing whitespace and indentation on blank lines.
- Use spaces for indentation. Makefiles may use tabs where the format requires them.
- Target 100 characters per line in first-party code and Markdown prose. Formatter output,
  unbreakable URLs, and generated identifiers may exceed the target.
- Keep changes focused. Do not mix behavior changes with mechanical formatting unless the behavior
  change requires it.

## Languages

- C++ uses C++23 and the repository's `.clang-format`. CMake enables `-Wall`, `-Wextra`, and
  `-Wpedantic` for first-party targets.
- QML uses `.qmlformat.ini`, including semicolons for JavaScript statements. Qt's `qmllint` rejects
  syntax errors and the high-signal warning categories configured in `.qmllint.ini`.
- JavaScript uses the repository's Prettier configuration and strict equality.
- Python follows PEP 8 with four-space indentation. Scripts must run with the supported Python 3
  interpreter and use only declared dependencies.
- CMake uses lowercase commands, two-level four-space indentation, and quoted paths.
- Shell scripts use POSIX shell unless the shebang names Bash. Quote expansions unless splitting is
  the intended behavior.

## Required gates

Format or check the files changed from `origin/main` during normal development:

```sh
scripts/format.sh --changed origin/main
scripts/check_changed.sh origin/main
```

Use the full-tree mode after changing format rules or upgrading a formatter:

```sh
scripts/format.sh --all
scripts/check_format.sh --all
scripts/check_qml.sh --all
```

Run these remaining gates before merge:

```sh
git diff --check
cmake --build --preset ci
ctest --preset ci
```

CI runs the formatters and rejects any resulting diff. It then runs `qmllint`, the build, and the
test suite with compiler warnings enabled, under both clang and GCC, because warnings are `-Werror`
here and the two compilers do not report the same set. No preset names a compiler, so the commands
above use whichever the host has, and either one is a configuration CI also checks. A change is
ready to merge only when every applicable command passes.

## Code that calls something this repository does not run

A change that calls an external service is exercised against that service before it merges, not
after. A stub answers the way the author expected; the service answers the way it answers, and the
difference between those two is where this kind of change fails.

Give it a way to be run against the real thing without publishing anything: a dry-run input, a flag
that writes what it would have written, a single-item mode. Then run it, read the output, and say in
the pull request what came back.

The release notes rewrite is what this rule is made of. It was tested against a stubbed API, merged,
and cut a release that published the generated commit list instead of notes, because the model's
thinking spent a token budget sized for the answer alone. Five further pull requests found the rest
by publishing them: a heading with nothing under it, notes that could not be read back, the
project's own vocabulary, a fenced command, an underlined title. Each was one real call away from
being found before it shipped.
