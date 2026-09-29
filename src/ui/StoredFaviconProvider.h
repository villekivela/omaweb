#pragma once

class QQmlEngine;

namespace omaweb {

// Makes the favicons a window's store keeps drawable on `engine`, at the
// addresses `storedfavicons::address` names, and lets a window read the icon
// a page reported, through `readIcon`, so its store can keep it. A lookup is
// carried to the window and answered where its store reads: for a Space, on
// the store's thread. The image loader never waits for it, and nothing in
// either direction makes a network request. A page with nothing stored is answered
// as one transparent pixel, which `SiteTile` draws as the host code. Call once
// per engine, before loading QML that draws a tab.
void installStoredFavicons(QQmlEngine &engine);

} // namespace omaweb
