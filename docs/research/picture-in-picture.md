# Picture-in-picture on the Qt engine

This note records what the engine Omaweb ships does when a page asks for picture-in-picture. It
answers [issue #327](https://github.com/villekivela/omaweb/issues/327). The sources are QtWebEngine
6.11.2 and the Chromium 140 tree Qt pins for it, commit `5170777d28be` of `qtwebengine-chromium`.
Omaweb's patch series does not touch any of the code below.

## What the engine does

QtWebEngine turns picture-in-picture off in every page it draws. Its settings code writes
`prefs->picture_in_picture_enabled = false` under the comment "Not supported" each time it applies
its preferences to a page.
[web_engine_settings.cpp](https://github.com/qt/qtwebengine/blob/v6.11.2/src/core/web_engine_settings.cpp#L355)

Blink reads that preference before anything else about the request. With it off, the document is
refused as `kDisabledBySystem` ahead of the checks for a loaded video, a video track, the
`disablepictureinpicture` attribute, and a user gesture.
[picture_in_picture_controller_impl.cc](https://github.com/qt/qtwebengine-chromium/blob/5170777d28bee1ce92cc693a0dbf2ad01492e5cf/chromium/third_party/blink/renderer/modules/document_picture_in_picture/picture_in_picture_controller_impl.cc#L80-L84)

A page therefore sees three things:

- `document.pictureInPictureEnabled` is `false`.
- `HTMLVideoElement.requestPictureInPicture()` returns a promise rejected with a `NotSupportedError`
  whose message is "Picture-in-Picture is not available."
  [html_video_element_picture_in_picture.cc](https://github.com/qt/qtwebengine-chromium/blob/5170777d28bee1ce92cc693a0dbf2ad01492e5cf/chromium/third_party/blink/renderer/modules/picture_in_picture/html_video_element_picture_in_picture.cc#L119-L122)
- The video's own controls show no picture-in-picture button, because the controls ask the same
  question before drawing it.

The request never leaves the renderer, and nothing reaches Omaweb: no signal, no new-window request,
no window. The Document Picture-in-Picture API is gone altogether, because Qt disables the Blink
feature at startup.
[web_engine_context.cpp](https://github.com/qt/qtwebengine/blob/v6.11.2/src/core/web_engine_context.cpp#L680)

## Why a patch would not be small

Turning the preference back on is one line, and the request would then reach the browser process and
stop there. Chromium asks the embedder's `WebContentsDelegate::EnterPictureInPicture`, whose default
answers `kNotSupported`, and Qt's delegate does not override it. The page would get the same
rejection, one step later.
[web_contents_delegate.cc](https://github.com/qt/qtwebengine-chromium/blob/5170777d28bee1ce92cc693a0dbf2ad01492e5cf/chromium/content/public/browser/web_contents_delegate.cc#L395-L398)

Past the delegate, Chromium expects the embedder to supply the floating window: something that draws
the video's compositor surface outside the page and sends play, pause, and close back to the player.
Chrome builds that window in its own `chrome/` layer, which Qt does not ship, and QtWebEngine has no
API that hands a video surface to the application. A picture-in-picture patch would therefore be a
new embedder surface in the engine, of the kind
[ADR 0049](../adr/0049-ship-omawebs-own-engine-build.md) takes on for extensions, and not a fix.

## What Omaweb does with it

Omaweb reports picture-in-picture as unavailable on this engine and leaves the engine's rejection as
the page's answer. `qtRefusesPictureInPictureWhereThePageCanSeeIt` in the Qt engine contract test
holds that answer, so an engine update that changes it fails the test and has to be decided on. The
floating window waits for an engine that surfaces the request.
