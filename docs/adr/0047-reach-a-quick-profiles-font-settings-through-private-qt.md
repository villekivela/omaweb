# Reach a Quick profile's font settings through private Qt

A page that names no font gets the engine's defaults, and a page that names a size gets it however
small. The reader's page fonts (#293) are `QWebEngineSettings` values: `StandardFont`, `FixedFont`,
`DefaultFontSize`, `DefaultFixedFontSize` and `MinimumFontSize`. The widgets profile hands that
class out publicly. The Quick profile, which is what `EngineProfile.qml` builds, does not: its
`settings()` is a private method returning `QQuickWebEngineSettings`, a class Qt ships without a
public header and which exposes the engine's boolean and enum attributes as QML properties and none
of its fonts. Nothing public reaches the `QWebEngineSettings` it wraps.

The alternatives were all worse. A Chromium `--blink-settings` flag can set the sizes but not the
families, is read once at startup so a change would need a restart, and is a flag Omaweb would add
itself ([ADR 0034](0034-audit-the-engine-flags-omaweb-adds-itself.md)). A user stylesheet injected
into every frame can only override a page's own declaration, which is Reader mode and a non-goal. A
`QWebEngineView` built from the widgets module for the sake of its public profile is a second engine
integration. Not shipping the setting leaves a reader with a HiDPI display zooming every tab by
hand.

So `src/engine/qt/QtProfileSettings.cpp` reaches past the public API, and it is the second place
Omaweb does, after the portal window handle in `LinuxPortalWindow.cpp` that `docs/development.md`
accounts for under printing. It includes the private header for the settings class and names the two
private members it needs, the profile's `settings()` and the settings' `d_ptr`, each in an explicit
template instantiation, which the language exempts from access checking; a friend declared in the
same instantiation hands the member pointer out. Everything from there on is the public core API:
the values are set, and reset, on every attached profile's `QWebEngineSettings`, and a view's
settings inherit from its profile's, so an open page takes a change at once.

The same terms as the portal call apply. A private member's offset is promised by nothing, so the
adapter reaches into only the Qt it was compiled against: `qVersion()` is compared to
`QT_VERSION_STR`, and a mismatch leaves the engine on its own defaults, prints why once, and reads
as unavailable to Settings, which says so in the group's place. A Qt that renames either member
breaks the build, which is the harmless case, and `omaweb-qt-engine-contract-tests` loads a real
page through a real profile and reads its computed style back, so a Qt that keeps the names and
moves the members fails a test rather than a reader. The package depends on the Qt it was built
against, so the two cannot come apart in a distributed build.

The reach is confined to that one file and to profiles, and every adapter that needs a profile's
settings goes through it: the fonts in `QtPageFonts.cpp`, and since #292 the WebRTC address policy
in `QtWebRtcPolicy.cpp`, which sets the `WebRTCPublicInterfacesOnly` attribute on the same object
under the same terms. The core's `FontSettings` knows no engine: it holds what the reader chose over
what the engine reported, and the adapter reports the engine's own values off the first profile it
attaches to, before writing anything over them. Content blocking's attachment is the one path every
profile takes, so `QtContentBlocker` announces each profile it attaches for the first time and the
fonts follow it, the way Global Privacy Control rides the same attachment, rather than a second
attachment path being threaded through the QML.
