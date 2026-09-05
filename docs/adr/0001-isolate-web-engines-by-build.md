# Isolate web engines by build

Omaweb ships separate application variants for QtWebEngine and Ladybird. `omaweb-qt` is the default
development build, while `omaweb-ladybird` uses a pinned Ladybird build configured outside the
default CMake graph. Both variants share the browser model, settings, QML, themes, and
engine-contract tests, but choose their engine adapter and linked dependencies at build time. Omaweb
does not migrate live tabs between engines.

This avoids a runtime C++ plugin ABI against Ladybird and avoids non-portable cross-process window
embedding on macOS and Wayland. Ladybird's own web-content and service processes remain intact. A
bitmap-painted QML bridge may prove the first Ladybird adapter, but daily-driver status requires a
Qt Quick texture path and frame-time validation.

The engine contract reports capabilities instead of pretending every engine behaves alike. Omaweb
does not expose third-party extensions unless both engines can support the same extension system.
Engine-specific extensions remain outside the product feature set, so core features such as content
blocking and keyboard navigation cannot depend on them.
