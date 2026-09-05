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
- QML uses `.qmlformat.ini`. Qt's `qmllint` rejects syntax errors and the high-signal warning
  categories configured in `.qmllint.ini`.
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
test suite with compiler warnings enabled. A change is ready to merge only when every applicable
command passes.
