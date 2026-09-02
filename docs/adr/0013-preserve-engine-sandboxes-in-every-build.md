# Preserve engine sandboxes in every build

Omaweb requires Qt 6.11 at its latest approved patch, macOS 13 or newer, and Xcode 15 or newer. The canonical macOS setup uses Qt's Apple Silicon SDK with QtWebEngine, while Homebrew supplies CMake, Ninja, and compiler caching. Development produces a runnable application bundle; signing, notarization, and DMG distribution remain later work. Omaweb does not target the Mac App Store.

The first Linux package is a native Arch package for Omarchy using system `qt6-base`, `qt6-declarative`, `qt6-webengine`, and `qt6-wayland`. AppImage and Flatpak packaging wait until Omaweb can maintain bundled engine security updates. Native Wayland is first-class and X11 remains a fallback supplied by Qt.

Scripts and release builds reject Chromium sandbox-disabling, single-process, and in-process network-service flags. The Ladybird variant retains upstream helper processes and sandbox launch behavior, remains visibly experimental, and is not approved for sensitive browsing while Ladybird is pre-alpha. CI checks weekly for engine updates, reports the runtime Chromium base and security-patch versions, and rejects releases below the repository minimum.
