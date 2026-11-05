# Ship proprietary codecs and use a local Widevine

Omaweb ships the engine's proprietary media codecs and does not ship or download Widevine. A
Widevine library already on the machine is found and used by the engine's own lookup (#914).

Supersedes the last sentence of [0014](0014-license-omaweb-under-mpl-2.md), which kept proprietary
media codecs off public builds until a legal review.

## What the engine does

`omaweb-qtwebengine` is built with `QT_FEATURE_webengine_proprietary_codecs` on, as Arch's stock
`qt6-webengine` is, and `libQt6WebEngineCore` carries the H.264 and HEVC decoders. The requirements
said otherwise, and no build step switched the codecs off. The engine contract test
`qtDecodesProprietaryCodecs` plays an H.264 MP4 and an AAC M4A on the engine CI builds against, so
the documents and the engine cannot drift apart again.

## Decided

- Omaweb ships proprietary codecs.
- Omaweb does not ship or download Widevine, or any other DRM module. Shipping the library is
  refused without an agreement with Google, and a download has no clause behind it
  ([research](../research/widevine.md#licence-by-delivery-route)).
- The engine finds a library on the machine by its own lookup, and Omaweb adds no code to help or
  stop it. The maintainer played Spotify and Netflix in Omaweb on 2026-11-05 through a library that
  Arch's Chromium had fetched, so no engine change follows.
- Omaweb says this in the requirements and the README and nowhere else. No public licence text
  clearly covers the use, so nothing promotes it, and Omaweb gives no support for a service.

## What was not done

- **Removing the lookup in the engine patch series.** It is engine work, and the playback result is
  what the maintainer wanted to keep.
- **Finding a library unaided on aarch64.** Qt's lookup names `linux_x64` and would need an engine
  patch.
- **Testing with a real CDM.** The codec test uses open fixtures. A Google library is not a test
  input.
