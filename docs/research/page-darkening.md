# Page darkening research

This note records the research for #973 into whether Omaweb can draw a light-only page dark the way
Safari's Adaptive Dark Mode and Dark Reader's Dynamic mode do, rather than with Chromium's forced
dark mode that Page darkening uses today. It is not an implementation contract. The behaviour #916
settled is in the [requirements](../product/requirements.md), and the follow-up this note proposes
is [at the end](#follow-up-issue-draft).

The engine is `omaweb-qtwebengine` 6.140.0, built from QtWebEngine tag `v6.140.0`. That tag pins the
`qtwebengine-chromium` submodule at commit `e2b413e`, which is Chromium 140.0.7339.264
([VERSION](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/chrome/VERSION?id=e2b413ea3e2dc3c533335e12297abfda95b26cfa)),
called the pin below. None of the 32 patches in
[the series at commit `8c2c5cd`](https://github.com/villekivela/omaweb-qtwebengine-patches/tree/8c2c5cd92be27c31fc6241382be802df2f54b31d/patches)
touches the dark mode code, so the Chromium claims hold for the shipped engine.

Source links point at pinned commits so line numbers stay valid: the Chromium pin, QtWebEngine
`v6.140.0`, Chromium's own tag `140.0.7339.264` for `chrome/` code Qt does not ship, WebKit `main`
at `29a3b07` (2026-11-24) and Dark Reader `main` at `586f4b9` (2026-11-24, version 4.9.135). Web
pages were read on 2026-11-24. The claims come from source, documentation or an issue tracker, and
the [screenshots](#screenshots) were taken afterwards to check them against what is drawn.

## Summary

- Safari's Adaptive Dark Mode recolours at paint time inside WebKit. Scripts read the authored
  colours, and the page is analysed once it first draws to decide whether it is already dark. The
  colour math and the detection live in Apple's closed WebKitAdditions, so only the hooks around
  them can be read.
- Dark Reader's Dynamic mode rewrites every colour in the page's CSS by role: background, text,
  border. Each role maps lightness into its own range between a dark pole and a light pole. It does
  this in a content script that reads every style sheet, inline style, CSS variable, shadow root and
  adopted sheet, and keeps watching for changes. That costs work on every load and makes it visible
  to the page.
- Chromium's forced dark already has the roles Dark Reader uses. It applies one lightness inversion
  to all of them, chosen by brightness thresholds. Its knobs are process-wide command-line values
  read once per renderer, and none of them changes that model, so no setting gets materially closer
  to Safari.
- Recommendation: patch the engine. Add a role-aware colour transform to Blink's dark mode filter,
  modelled on Dark Reader's colour functions, behind a new inversion algorithm value. Keep
  `darkenPage`, the ground-reading script and every rule of #916.

## Safari Adaptive Dark Mode

### What the announcement says

WebKit announced Adaptive Dark Mode in Safari 27.3 beta on 2026-11-03.
[Introducing Adaptive Dark Mode](https://webkit.org/blog/18541/introducing-adaptive-dark-mode/) is
the primary source for its behaviour. It says:

- Backgrounds and text: it uses "a color matrix to invert the luminosity of colors while preserving
  hue". Light backgrounds turn dark, dark text turns light, and text aims for the WCAG 4.5:1
  contrast ratio.
- Gradients: "Light CSS gradients are also transformed to dark."
- Buttons, menus and text over images: it makes "careful judgments" so buttons contrast with their
  background, text stays visible over images and hover menus read clearly.
- SVG: it "inspects SVG images and transforms colors when needed".
- Bitmaps: most are left alone. It "analyzes the pixel histograms and inverts simple black-and-white
  logos".
- Masks and blend modes: "in some cases Adaptive Dark Mode turns such effects off".
- Canvas: `<canvas>` content is left unchanged, and images drawn into a canvas draw in their
  original form.
- Detection: Safari "prevents websites from detecting when Adaptive Dark Mode has been applied", and
  "JavaScript that queries CSS colors will get the original authored colors, not the transformed
  ones".
- A page that already has a dark theme: a page that declares `color-scheme: light dark`, in CSS or
  in the `color-scheme` meta element, is left alone. For other pages it "quickly analyzes the page,
  looking at colors and estimating whether it's predominantly light or dark", and changes nothing on
  a page that has "a dark background with light text".
- Opting out: `color-scheme: light only`, in the meta element or in CSS on any element, keeps the
  page or that element light. The `only` keyword arrived in Safari 27.3 beta 2.
- Turning it on needs Safari 27.3 beta and a dark system appearance. Safari then offers a preview on
  a bright site.

The post does not mention borders, box shadows, form controls or video, and does not say what
`prefers-color-scheme` reports. Since the feature requires a dark system appearance, a page's
`@media (prefers-color-scheme: dark)` rules already match. The post does not say whether that is how
such pages count as dark.
[Safari Technology Preview 257](https://webkit.org/blog/18652/release-notes-for-safari-technology-preview-257/)
(2026-11-18) adds one line: it "Fixed slow rendering and laggy scrolling on pages with many
gradients affected by Adaptive Dark Mode color filters".

### What WebKit's open source shows

Inside WebKit the feature is called AX Custom Color Mode. It is compiled under
`ENABLE(AX_CUSTOM_COLOR_MODE)` and switched by the embedder preference `AXCustomColorModeEnabled`,
off by default
([UnifiedWebPreferences.yaml](https://github.com/WebKit/WebKit/blob/29a3b076140c4fb61d8921f7ca1c6c3ed3678c73/Source/WTF/Scripts/Preferences/UnifiedWebPreferences.yaml#L131-L138)).
WebKit's own API test names it Adaptive Dark Mode and turns it on with that preference
([AdaptiveDarkModeTests.mm](https://github.com/WebKit/WebKit/blob/29a3b076140c4fb61d8921f7ca1c6c3ed3678c73/Tools/TestWebKitAPI/Tests/WebCore/AdaptiveDarkModeTests.mm)).
The open tree holds the hooks. The controller that computes colours, `AXCustomColorModeController`,
and the backdrop tracker, `AXCustomColorBackdropContext`, are included from `<WebKitAdditions/...>`,
Apple's closed sources
([Document.cpp](https://github.com/WebKit/WebKit/blob/29a3b076140c4fb61d8921f7ca1c6c3ed3678c73/Source/WebCore/dom/Document.cpp#L491-L492),
[LocalFrameView.cpp](https://github.com/WebKit/WebKit/blob/29a3b076140c4fb61d8921f7ca1c6c3ed3678c73/Source/WebCore/page/LocalFrameView.cpp#L163-L164)).
The commit messages describe what the hooks are for:

- Backdrops: a two-phase "fake paint" first collects the areas covered by background colours, then
  computes what paints behind each thing, "because of CSS positioning etc."
  ([f2468d1](https://github.com/WebKit/WebKit/commit/f2468d106a0c88d7db9288acd26f4b5bbe44d07b)).
  Text colours are adjusted at paint time
  ([ef0952e](https://github.com/WebKit/WebKit/commit/ef0952e2516a36a36d28e6de8317601a017abe4f)), and
  table row backgrounds record a text backdrop
  ([6233469](https://github.com/WebKit/WebKit/commit/6233469583). This is how text can be held at a
  contrast ratio against what is actually behind it.
- Shadows and borders: the box-shadow hook receives the whole shadow, so it can tell a drop shadow
  from a zero-blur, zero-offset shadow that stands in for a border
  ([c5c4584](https://github.com/WebKit/WebKit/commit/c5c4584117e4f975b57b25dc8fb181e1366e81fa)).
  Outline colours are chosen to suit the surface they are painted over
  ([47bc28e](https://github.com/WebKit/WebKit/commit/47bc28e76bee093644b54bac231a254db6eca278)), and
  hairlines are painted by WebKitAdditions
  ([c7346f7](https://github.com/WebKit/WebKit/commit/c7346f75327d3c46e932358263523c3127f0bb5a)).
- SVG: the colours SVGs are drawn with are adjusted
  ([7a8ebb6](https://github.com/WebKit/WebKit/commit/7a8ebb6f9589feb786b6c6041c47d8e2e4b2b699)), and
  whether an SVG image is changed is decided per use rather than once per file
  ([73969fe](https://github.com/WebKit/WebKit/commit/73969fece772d7963b3d69daf55740d29a15a194)).
- Images: content images are classified and transformed at paint time, but not images drawn into a
  canvas
  ([a2c934a](https://github.com/WebKit/WebKit/commit/a2c934a413221ed0615ea73ba9060ef513187493)).
  Only the drawn part of a bitmap is classified, so icons cut from a sprite sheet can be inverted
  ([b5d0911](https://github.com/WebKit/WebKit/commit/b5d0911616c296f509173682ac6239d153b54c47)).
- Hiding from the page: system colours resolve one way for computed style and another for painting,
  so `getComputedStyle` keeps the value it would have without the mode
  ([369f5fb](https://github.com/WebKit/WebKit/commit/369f5fbfa3ebb83aa4667c6316596af3894d6feb),
  [e2cb413](https://github.com/WebKit/WebKit/commit/e2cb413f647ef42ba26c14a141a9369d5fff6856)). A
  focus ring drawn into a canvas uses the computed-style colour, because canvas pixels can be read
  back
  ([9dad621](https://github.com/WebKit/WebKit/commit/9dad6213605990ca8295048666618060cfaef959)).
  Form controls get the same split
  ([bab6383](https://github.com/WebKit/WebKit/commit/bab63831f8b5fe5d33a35777df94af266509018c)).
  WebKit tests this as fingerprinting: a layout test compares every computed style property of every
  input type before and after the mode is turned on
  ([widgets-appearance-auto.html](https://github.com/WebKit/WebKit/blob/29a3b076140c4fb61d8921f7ca1c6c3ed3678c73/LayoutTests/adaptive-dark-mode/fingerprint/widgets-appearance-auto.html)).
- Deciding a page is dark: the mode "runs some detection logic at `didReachVisuallyNonEmptyState`
  time", the moment the page first has something to show, and resets between page loads. The result
  goes to the UI process through a message that lives in WebKitAdditions
  ([5a1e7b7](https://github.com/WebKit/WebKit/commit/5a1e7b740f3886f508738d393d48b868ae33b795)). A
  page restored from the back-forward cache keeps its state for continuity
  ([69e2d75](https://github.com/WebKit/WebKit/commit/69e2d75a083dd381dfff344425bb9adf0ad4050a)).
- Turning it off: the embedder can force the mode off for a site, main frame and subframes, through
  `_setForceDisableAXCustomColorMode` on `WKWebpagePreferences`; WebKit leaves the choice of sites
  to the embedder
  ([f8d416f](https://github.com/WebKit/WebKit/commit/f8d416f118202cfdb6b41a131df206d031c78381),
  [Document.cpp](https://github.com/WebKit/WebKit/blob/29a3b076140c4fb61d8921f7ca1c6c3ed3678c73/Source/WebCore/dom/Document.cpp#L10530-L10545)).
  Quirks can force the filter on or off for single elements
  ([8747e68](https://github.com/WebKit/WebKit/commit/8747e6861209c1a5a75b57b52c27bf1b8a8702fb)). The
  mode is off while a frame prints.

The paint-time colours go through the same machinery as WebKit's non-standard `-apple-color-filter`.
Getters that apply the colour filter resolve with the rendering purpose and so are affected by the
mode, while plain getters are not
([e2cb413](https://github.com/WebKit/WebKit/commit/e2cb413f647ef42ba26c14a141a9369d5fff6856),
[6f60a88](https://github.com/WebKit/WebKit/commit/6f60a8837e2b82c4864140a472baf0929ca69e6c)). The
filter `apple-invert-lightness()`, added in 2018, is "based on a lightness invert in HSL space, with
some adjustments to improve the contrast of some colors on dark backgrounds"
([ada8492](https://github.com/WebKit/WebKit/commit/ada849288d)). Whether Adaptive Dark Mode's matrix
is that filter is not in the open source.

### Not found in a primary source

- The colour transform itself: the matrix, the target lightness of a dark ground, and how contrast
  is raised to 4.5:1. It is in WebKitAdditions.
- The detection rule that decides a page is "predominantly light or dark": sample points,
  thresholds, what counts. Also in WebKitAdditions.
- What `prefers-color-scheme` and `color-scheme` report while the mode is active, beyond the system
  appearance being dark.
- Whether a white frame shows before the first analysis. No source says how a page looks between
  load start and `didReachVisuallyNonEmptyState`.
- Cost on page load or memory. WebKit says only that working inside the engine is fast and power
  efficient.

## Dark Reader Dynamic mode

Dark Reader is MIT licensed. Its Dynamic mode is a content script that rewrites the page's CSS.

### Colour functions

The colour functions are in
[`src/inject/dynamic-theme/modify-colors.ts`](https://github.com/darkreader/darkreader/blob/586f4b900d92e6b3cb14076462b1a53a7760aa46/src/inject/dynamic-theme/modify-colors.ts),
not `src/generators/` as older references say. Every function works in HSL against two poles, the
dark theme's background `#181a1b` and text `#e8e6e3` by default
([defaults.ts](https://github.com/darkreader/darkreader/blob/586f4b900d92e6b3cb14076462b1a53a7760aa46/src/defaults.ts#L11-L20)).
A colour is "neutral" when its saturation is low. A neutral colour takes the pole's hue and
saturation, so greys turn into the theme's tint rather than pure grey.

- `modifyBackgroundColor`
  ([L143-L208](https://github.com/darkreader/darkreader/blob/586f4b900d92e6b3cb14076462b1a53a7760aa46/src/inject/dynamic-theme/modify-colors.ts#L143-L208)):
  lightness 0 to 0.5 maps onto 0 to 0.4, and 0.5 to 1 maps onto 0.4 down to the background pole's
  lightness. White lands on the pole and a dark colour stays dark. Neutral means saturation below
  0.12, or a light blue. Yellows and greens have their hue moved and yellows their lightness cut by
  a quarter, because a dark yellow reads as brown.
- `modifyForegroundColor`
  ([L210-L274](https://github.com/darkreader/darkreader/blob/586f4b900d92e6b3cb14076462b1a53a7760aa46/src/inject/dynamic-theme/modify-colors.ts#L210-L274)):
  dark text, lightness 0 to 0.5, maps onto the text pole's lightness down to 0.55, and light text
  maps onto 0.55 up to the pole. Every text colour ends at lightness 0.55 or above. Blues are moved
  toward hue 205 to 220 so links stay readable.
- `modifyBorderColor`
  ([L276-L312](https://github.com/darkreader/darkreader/blob/586f4b900d92e6b3cb14076462b1a53a7760aa46/src/inject/dynamic-theme/modify-colors.ts#L276-L312)):
  any lightness maps onto 0.5 down to 0.2, so borders become mid to dark lines.
- `modifyShadowColor` and `modifyGradientColor`
  ([L314-L320](https://github.com/darkreader/darkreader/blob/586f4b900d92e6b3cb14076462b1a53a7760aa46/src/inject/dynamic-theme/modify-colors.ts#L314-L320))
  use the background function.

Results are cached per colour, theme and pole, and the reader's brightness, contrast, sepia and
grayscale are applied after as a colour matrix
([L59-L86](https://github.com/darkreader/darkreader/blob/586f4b900d92e6b3cb14076462b1a53a7760aa46/src/inject/dynamic-theme/modify-colors.ts#L59-L86)).
Each transformed colour is also registered as a custom property such as `--darkreader-bg-ffffff`
([palette.ts](https://github.com/darkreader/darkreader/blob/586f4b900d92e6b3cb14076462b1a53a7760aa46/src/inject/dynamic-theme/palette.ts#L80-L97)).

Dark Reader maps one colour by role: the same input colour gets a different output by role. White as
a background becomes `#181a1b`, white as text stays near `#e8e6e3`, and a white border becomes a
dark line. Chromium's forced dark sends white through one inversion whatever its role, and only
decides whether to invert it.

### What it reads and how

- Style sheets: every `<style>` and `<link>` sheet is read through the CSSOM and rewritten into an
  override sheet. A cross-origin sheet's rules cannot be read from the page, so the extension
  fetches its text from its background page and builds a copy
  ([style-manager.ts](https://github.com/darkreader/darkreader/blob/586f4b900d92e6b3cb14076462b1a53a7760aa46/src/inject/dynamic-theme/style-manager.ts#L123-L230)).
- Late and script-built sheets: `insertRule`, `deleteRule`, `addRule` and `replaceSync` on
  `CSSStyleSheet` are overridden in the page's own world so changes made through the CSSOM are seen
  ([stylesheet-proxy.ts](https://github.com/darkreader/darkreader/blob/586f4b900d92e6b3cb14076462b1a53a7760aa46/src/inject/dynamic-theme/stylesheet-proxy.ts#L95-L124)).
  Adopted style sheets are managed for the document and for each shadow root
  ([adopted-style-manger.ts](https://github.com/darkreader/darkreader/blob/586f4b900d92e6b3cb14076462b1a53a7760aa46/src/inject/dynamic-theme/adopted-style-manger.ts#L51-L85)).
- Inline styles: a `MutationObserver` watches `style`, `fill`, `stroke`, `stop-color`, `bgcolor`,
  `color` and `background` attributes. A colour found there is rewritten through a
  `--darkreader-inline-*` custom property and a `data-darkreader-inline-*` attribute
  ([inline-style.ts](https://github.com/darkreader/darkreader/blob/586f4b900d92e6b3cb14076462b1a53a7760aa46/src/inject/dynamic-theme/inline-style.ts#L27-L132),
  [L244-L273](https://github.com/darkreader/darkreader/blob/586f4b900d92e6b3cb14076462b1a53a7760aa46/src/inject/dynamic-theme/inline-style.ts#L244-L273)).
- CSS variables: a variable's role is inferred from where it is used, as a background, text, border
  or background image, and it gets one rewritten copy per role
  ([variables.ts](https://github.com/darkreader/darkreader/blob/586f4b900d92e6b3cb14076462b1a53a7760aa46/src/inject/dynamic-theme/variables.ts#L24-L27)).
- Shadow DOM: open shadow roots get their own override sheets
  ([index.ts](https://github.com/darkreader/darkreader/blob/586f4b900d92e6b3cb14076462b1a53a7760aa46/src/inject/dynamic-theme/index.ts#L331-L367)).
  It finds them through `host.shadowRoot`
  ([L445-L451](https://github.com/darkreader/darkreader/blob/586f4b900d92e6b3cb14076462b1a53a7760aa46/src/inject/dynamic-theme/index.ts#L445-L451)),
  which is null for a closed root, so closed shadow roots are left as written. That last point is
  read from the code, not from Dark Reader's documentation.
- Images: only CSS background images are analysed, not `<img>` elements. An image is drawn into a
  canvas of at most 32 by 32 samples, and counted as dark, light or transparent by thresholds of 0.4
  and 0.7 lightness and 70% of pixels
  ([image.ts](https://github.com/darkreader/darkreader/blob/586f4b900d92e6b3cb14076462b1a53a7760aa46/src/inject/dynamic-theme/image.ts#L130-L263)).
  A large light opaque image is hidden, a dark transparent one, such as an icon, is inverted through
  an SVG filter, and a light opaque one is inverted or replaced by its solid colour darkened
  ([modify-css.ts](https://github.com/darkreader/darkreader/blob/586f4b900d92e6b3cb14076462b1a53a7760aa46/src/inject/dynamic-theme/modify-css.ts#L592-L634)).

### Deciding a page is already dark

The detector, used when the reader turns on automatic detection, first trusts a declaration: a
`color-scheme` meta of `dark` or `light`, or a `dark` class or `data-theme="dark"` on the root or
body
([detector.ts](https://github.com/darkreader/darkreader/blob/586f4b900d92e6b3cb14076462b1a53a7760aa46/src/inject/detector.ts#L82-L101)).
Otherwise it disables its own sheets and samples up to 4 by 4 points with `elementFromPoint`. Any
opaque background above 0.6 lightness, or dark text on a transparent one, means the page is not
dark. It then blends the root and body backgrounds and calls the page dark below 0.5
([L7-L80](https://github.com/darkreader/darkreader/blob/586f4b900d92e6b3cb14076462b1a53a7760aa46/src/inject/detector.ts#L7-L80)).
Omaweb's ground script reads 25 points and takes the median, so a few light points do not decide it.

### Flash, visibility and cost

- Flash: until the page's sheets are read, a fallback sheet forces every element's background to the
  dark pole and its text to the light one with `!important`
  ([modify-css.ts](https://github.com/darkreader/darkreader/blob/586f4b900d92e6b3cb14076462b1a53a7760aa46/src/inject/dynamic-theme/modify-css.ts#L253-L266)).
  A bright flash on each new page in Dynamic mode is still reported in
  [#469](https://github.com/darkreader/darkreader/issues/469), open since 2018, and
  [#3523](https://github.com/darkreader/darkreader/issues/3523).
- Visibility: it sets `color-scheme: dark !important` on `html` and on every `iframe`
  ([modify-css.ts](https://github.com/darkreader/darkreader/blob/586f4b900d92e6b3cb14076462b1a53a7760aa46/src/inject/dynamic-theme/modify-css.ts#L136-L150)),
  and `data-darkreader-mode` and `data-darkreader-scheme` attributes on the root
  ([index.ts](https://github.com/darkreader/darkreader/blob/586f4b900d92e6b3cb14076462b1a53a7760aa46/src/inject/dynamic-theme/index.ts#L871-L872)).
  A page can read all of these. A site can also opt out with a `darkreader-lock` meta element
  ([L623-L639](https://github.com/darkreader/darkreader/blob/586f4b900d92e6b3cb14076462b1a53a7760aa46/src/inject/dynamic-theme/index.ts#L623-L639)).
- Breakage needs per-site fixes: the fixes file has 2,951 separator lines, one per site entry, in
  42,885 lines
  ([dynamic-theme-fixes.config](https://github.com/darkreader/darkreader/blob/586f4b900d92e6b3cb14076462b1a53a7760aa46/src/config/dynamic-theme-fixes.config)).
- Cost: Dark Reader publishes no load time or memory figures. Users report these, each on their own
  machine and version: Firefox at 1.8 GB for one tab with Dark Reader using "500+MB"
  ([#7728](https://github.com/darkreader/darkreader/issues/7728), open); the Stripe dashboard taking
  "30+ seconds to load" with "5+GB" of RAM on 4.9.34
  ([#6799](https://github.com/darkreader/darkreader/issues/6799)); memory that grows while editing
  on office.com on 4.9.81 ([#12571](https://github.com/darkreader/darkreader/issues/12571), open);
  CPU spikes and freezes on every site on 4.9.125
  ([#15285](https://github.com/darkreader/darkreader/issues/15285)).

## Chromium forced dark at the pin

### How it works

`QWebEngineSettings::ForceDarkMode` sets `force_dark_mode_enabled` in the page's `WebPreferences`
([web_engine_settings.cpp](https://code.qt.io/cgit/qt/qtwebengine.git/tree/src/core/web_engine_settings.cpp?h=v6.140.0#n401)).
Blink copies it into the `forceDarkModeEnabled` setting
([web_view_impl.cc](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/third_party/blink/renderer/core/exported/web_view_impl.cc?id=e2b413ea3e2dc3c533335e12297abfda95b26cfa#n1853)),
which invalidates colour scheme, style and paint when it changes
([settings.json5](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/third_party/blink/renderer/core/frame/settings.json5?id=e2b413ea3e2dc3c533335e12297abfda95b26cfa#n982)).
That is why Omaweb can switch it per page at run time with no reload.

Under a dark preferred scheme, which Omaweb hands the engine with a dark Browser theme, each element
gets a forced dark colour scheme unless its `color-scheme` has `dark` or carries `only`. Under a
light preferred scheme every element is forced
([computed_style.cc](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/third_party/blink/renderer/core/style/computed_style.cc?id=e2b413ea3e2dc3c533335e12297abfda95b26cfa#n3153)).
So with a dark Browser theme, `color-scheme: only light` opts an element out in Chromium as it does
in Safari.

Colours change at paint time, not in style. `prefers-color-scheme` still follows the preferred
scheme, which the style engine reads separately from the forced dark setting
([style_engine.cc](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/third_party/blink/renderer/core/css/style_engine.cc?id=e2b413ea3e2dc3c533335e12297abfda95b26cfa#n4308)).
`getComputedStyle` reads the authored colour, as the requirements already record for the ground.

Every paint call gives the filter a role: foreground, list symbol, background, SVG, border or
selection
([dark_mode_filter.h](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/third_party/blink/renderer/platform/graphics/dark_mode_filter.h?id=e2b413ea3e2dc3c533335e12297abfda95b26cfa#n37)).
The role only picks a classifier:

- Foreground, list symbol, SVG and border colours are inverted when their brightness,
  `(299 R + 587 G + 114 B) / 1000`, is below the foreground threshold. Background and selection
  colours are inverted when it is above the background threshold
  ([dark_mode_filter.cc](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/third_party/blink/renderer/platform/graphics/dark_mode_filter.cc?id=e2b413ea3e2dc3c533335e12297abfda95b26cfa#n278),
  [dark_mode_color_classifier.cc](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/third_party/blink/renderer/platform/graphics/dark_mode_color_classifier.cc?id=e2b413ea3e2dc3c533335e12297abfda95b26cfa#n75)).
  The defaults are 150 and 205
  ([dark_mode_settings_builder.cc](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/third_party/blink/renderer/platform/graphics/dark_mode_settings_builder.cc?id=e2b413ea3e2dc3c533335e12297abfda95b26cfa#n22)).
- Whatever the role, an inverted colour goes through the same function. The default, CIELAB, sets
  `L* = min(110 - L*, 100)` and keeps `a*` and `b*`, then moves a grey that lands between 18 and 32
  of 255 to `#121212`
  ([dark_mode_color_filter.cc](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/third_party/blink/renderer/platform/graphics/dark_mode_color_filter.cc?id=e2b413ea3e2dc3c533335e12297abfda95b26cfa#n57)).
  This is where white becomes `#121212` and where #936's epsilon comparison sits.
- Only borders and selection look at what is behind them. A border that turns too dark against the
  inverted background is darkened no further, and a selection is lightened until it is visible
  ([dark_mode_filter.cc](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/third_party/blink/renderer/platform/graphics/dark_mode_filter.cc?id=e2b413ea3e2dc3c533335e12297abfda95b26cfa#n144)).
- Gradient stops are treated as backgrounds
  ([gradient.cc](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/third_party/blink/renderer/platform/graphics/gradient.cc?id=e2b413ea3e2dc3c533335e12297abfda95b26cfa#n329)).
  A paint that already carries a shader is left alone
  ([dark_mode_filter.cc](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/third_party/blink/renderer/platform/graphics/dark_mode_filter.cc?id=e2b413ea3e2dc3c533335e12297abfda95b26cfa#n258)).
- Images: only images drawn as icons or separators are considered, never photos
  ([dark_mode_filter.cc](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/third_party/blink/renderer/platform/graphics/dark_mode_filter.cc?id=e2b413ea3e2dc3c533335e12297abfda95b26cfa#n226)).
  They are classified by sampling up to 1,000 pixels in 10 by 10 blocks and counting colours, with a
  generated neural network as fallback
  ([dark_mode_image_classifier.cc](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/third_party/blink/renderer/platform/graphics/dark_mode_image_classifier.cc?id=e2b413ea3e2dc3c533335e12297abfda95b26cfa#n20),
  [darkmode/README.md](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/third_party/blink/renderer/platform/graphics/darkmode/README.md?id=e2b413ea3e2dc3c533335e12297abfda95b26cfa)).
- Each `GraphicsContext` builds its filter from the process's settings, and inverted colours are
  kept in an LRU cache of 1,024
  ([graphics_context.cc](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/third_party/blink/renderer/platform/graphics/graphics_context.cc?id=e2b413ea3e2dc3c533335e12297abfda95b26cfa#n149),
  [dark_mode_filter.cc](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/third_party/blink/renderer/platform/graphics/dark_mode_filter.cc?id=e2b413ea3e2dc3c533335e12297abfda95b26cfa#n34)).

### The knobs

`--blink-settings` does not carry the tuning at the pin. The dark mode block of `settings.json5` has
only `forceDarkModeEnabled`. The comment in `forcedark_switches.h` still says `--blink_settings`,
but the code reads a separate switch, `--dark-mode-settings`, a comma-separated list of `name=value`
([switches.cc](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/third_party/blink/common/switches.cc?id=e2b413ea3e2dc3c533335e12297abfda95b26cfa#n21),
[dark_mode_settings_builder.cc](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/third_party/blink/renderer/platform/graphics/dark_mode_settings_builder.cc?id=e2b413ea3e2dc3c533335e12297abfda95b26cfa#n36)).
The values are the enums in
[dark_mode_settings.h](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/third_party/blink/renderer/platform/graphics/dark_mode_settings.h?id=e2b413ea3e2dc3c533335e12297abfda95b26cfa#n10):

| Parameter                       | Values                                                                             | Default |
| ------------------------------- | ---------------------------------------------------------------------------------- | ------- |
| `InversionAlgorithm`            | 0 simple RGB invert for testing, 1 brightness, 2 HSL lightness, 3 CIELAB lightness | 3       |
| `ImagePolicy`                   | 0 classify icons and separators, 1 never                                           | 0       |
| `ImageClassifierPolicy`         | 0 colour count with neural network fallback, 1 transparency and colour count       | 0       |
| `ForegroundBrightnessThreshold` | 0 to 255, below it a foreground is inverted, 255 inverts all, 0 none               | 150     |
| `BackgroundBrightnessThreshold` | 0 to 255, above it a background is inverted                                        | 205     |
| `ContrastPercent`               | -1.0 to 1.0                                                                        | 0       |

`ContrastPercent` reaches only algorithms 1 and 2, which use Skia's high contrast filter. The CIELAB
filter hard-codes a contrast of 0 and inverts colours with its own math, so the default algorithm
ignores it
([dark_mode_color_filter.cc](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/third_party/blink/renderer/platform/graphics/dark_mode_color_filter.cc?id=e2b413ea3e2dc3c533335e12297abfda95b26cfa#n32)).

The same values can come from the feature `WebContentsForceDark` and its parameters
`inversion_method` (`hsl_based`, `cielab_based`, `rgb_based`), `image_behavior` (`none`,
`selective`), `foreground_lightness_threshold`, `background_lightness_threshold` and
`classifier_policy`. A parameter left at its default defers to `--dark-mode-settings`
([features.cc](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/third_party/blink/common/features.cc?id=e2b413ea3e2dc3c533335e12297abfda95b26cfa#n1082),
[dark_mode_settings_builder.cc](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/third_party/blink/renderer/platform/graphics/dark_mode_settings_builder.cc?id=e2b413ea3e2dc3c533335e12297abfda95b26cfa#n83)).
Chrome's `#enable-force-dark` flag is that feature with preset parameters. At 140 its variants are
"with simple HSL-based inversion", "with simple CIELAB-based inversion", "with simple RGB-based
inversion", "with selective image inversion", "with selective inversion of non-image elements",
"with selective inversion of everything" and "with selective image inversion based on transparency
and number of colors". Each but the last sets thresholds of 150 and 205
([about_flags.cc](https://chromium.googlesource.com/chromium/src/+/refs/tags/140.0.7339.264/chrome/browser/about_flags.cc#815)).
There is no "increase text contrast" variant at this version. Qt does not ship `chrome://flags`, so
in Omaweb these are command-line values only.

### Can Omaweb pass them, and how often

- Omaweb builds the engine's command line from `QTWEBENGINE_CHROMIUM_FLAGS` in `main()` before the
  engine starts ([main.cpp](../../src/main.cpp)), and Qt reads that variable once when it creates
  its context
  ([web_engine_context.cpp](https://code.qt.io/cgit/qt/qtwebengine.git/tree/src/core/web_engine_context.cpp?h=v6.140.0#n940)).
  `--dark-mode-settings` is in the list of switches the browser process copies to each renderer
  ([render_process_host_impl.cc](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/content/browser/renderer_host/render_process_host_impl.cc?id=e2b413ea3e2dc3c533335e12297abfda95b26cfa#n3565)).
  So Omaweb can pass it with no engine change.
- It is process-wide. The renderer builds the settings once, into a function-local static
  ([dark_mode_settings_builder.cc](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/third_party/blink/renderer/platform/graphics/dark_mode_settings_builder.cc?id=e2b413ea3e2dc3c533335e12297abfda95b26cfa#n176)).
  Every Space and Private window gets the same values, and changing them needs a restart.
- Only the on and off switch is per page and live, through `ForceDarkMode`.

### Can a setting get closer to Safari

No setting changes the model, so none gets materially closer:

- Every algorithm inverts. Safari and Dark Reader both preserve hue, and CIELAB already keeps `a*`
  and `b*`, so it is the closest of the four. HSL inversion with a positive `ContrastPercent` raises
  contrast across the whole page, not against what is behind the text.
- No setting picks the dark ground. White lands near `L* = 10`, then `#121212`, whatever the theme.
- A lower `BackgroundBrightnessThreshold` would invert backgrounds that are already dark. A page's
  own dark ground left as written is a #916 rule, and the ground script's 205 is the engine's own
  limit, so the threshold should stay.
- A different `ForegroundBrightnessThreshold` only moves which text is flipped. Text on a light
  element under 205, such as `#cccccc`, keeps its ground and has its text inverted either way.
- `ImagePolicy=0`, selective icon inversion, is already the default.

Tuning would trade one inverted look for another, as the [screenshots](#screenshots) under each
algorithm show.

## Options for Omaweb

### Tune Chromium's forced dark

No engine change, one flag in `main.cpp`, but it cannot reach the look the issue asks for and could
never follow a setting without a restart.

### Recolour in a page script, as Dark Reader does

- Look: as good as Dark Reader with its fixes, since its functions are MIT and could be reused.
  Without its 2,951 site entries, Omaweb would meet the breakage that list exists for.
- Cost on every load: every sheet is parsed and rewritten in JavaScript, observers stay on, and
  users report hundreds of megabytes of memory and loads many seconds slower on heavy apps.
- Cross-origin sheets cannot be read from a page script. The shell would have to fetch their text,
  as Dark Reader's background page does, and that is a second request for every sheet.
- Flash: the script cannot recolour a sheet before it has arrived and been parsed. A cover sheet
  like Dark Reader's fallback would be needed until then, and Dark Reader's own users still report
  the flash. Today's engine darkening shows no white, a #916 rule.
- CSSOM changes are seen only by overriding `CSSStyleSheet` in the page's main world, where the page
  can see the override. The overrides, the `color-scheme: dark` it needs and the rewritten sheets
  are all visible to the page. Closed shadow roots stay out of reach.
- Engine contract: the script is engine-neutral, so Ladybird could run it and gain Page darkening.
  That is the one point in its favour.

### Patch the colour transform in the engine

- Look: Chromium's filter already gets the role of every colour it paints, the same roles Dark
  Reader rewrites. A role-aware transform can give a background, a text and a border three different
  outputs from one input, toward a ground and a text colour Omaweb picks.
- Cost: none per load. It runs where the inversion runs now, one function per colour behind the
  existing 1,024-entry cache. It reaches inline styles, variables, late and CSSOM-built sheets,
  shadow roots, closed or open, and cross-origin sheets with no extra work, because it acts on what
  is painted.
- Flash: none new. Darkening still starts before the document exists, as today.
- Visibility: as today. `getComputedStyle` and `prefers-color-scheme` read what the page wrote,
  which is Safari's own rule, and Safari also works at paint time.
- Upkeep: one patch to `dark_mode_color_filter.cc`, `dark_mode_filter.cc` and
  `dark_mode_settings*.{h,cc}`. It has to be carried across Chromium updates. The patch series
  already carries 32, and the [constraints](../agents/constraints.md) allow patches to the engine's
  drawing path.
- Engine contract: `darkenPage` and `PageDarkening` stay as they are. What changes is what the Qt
  engine promises a darkened page looks like. Ladybird still lacks the capability until LibWeb gets
  a paint-time hook of its own.

### What the transform would look like

Add `kRoleAware` to `DarkModeInversionAlgorithm`, as `InversionAlgorithm=4`, so the patch changes
nothing unless Omaweb asks for it:

- `DarkModeColorFilter::InvertColor` takes the role. The inverted colour cache is keyed on the
  colour and the role, since the same colour now has an output per role.
- `kBackground`, `kSelection` and gradient stops use Dark Reader's background mapping: lightness 0
  to 0.5 onto 0 to 0.4, 0.5 to 1 onto 0.4 down to the ground colour's lightness, with neutral
  colours taking the ground's hue and saturation. Under this algorithm every background goes through
  the mapping, with no per-element threshold. The mapping keeps a dark colour dark, so the
  threshold's job of leaving dark parts alone is done by the curve, and the 205 cliff the
  [screenshots](#screenshots) show, where `#d0d0d0` turns dark and `#cccccc` stays light, goes. A
  page whose own ground is dark is still left as written, because the ground script turns darkening
  off for the whole page at its limit of 205. That limit is the page's and stays.
- `kForeground` and `kListSymbol` use the text mapping, with every result at lightness 0.55 or
  above, and then raise the result against `contrast_background` to at least 4.5:1, as Safari aims.
  Blink already passes that colour for borders and selection. Without this step, text on a
  light-grey element would land light on light.
- `kSVG` uses the background mapping, as Dark Reader treats an SVG fill, so a large light shape is
  drawn dark rather than glaring. Blink gives fills and strokes one role, so a dark stroke or SVG
  text drawn over a light fill turns dark on dark. The patch has to measure that on real logos and
  icons before the choice is final; the contrast step against the fill is the likely fix.
- `kBorder` uses the border mapping, lightness onto 0.5 down to 0.2.
- The ground and text colours arrive as two new `--dark-mode-settings` parameters, so the patch
  needs no new Qt API. They are fixed for the run. Following a Browser theme switch live would need
  them in `WebPreferences` and a Qt setting, a larger patch, and #916 says Page darkening does not
  follow the Browser theme anyway.
- Images keep today's classifier. That matches Safari's own advice: leave most bitmaps and invert
  simple logos.

## Recommendation

Patch the engine with a role-aware transform as above, and keep the rest of Page darkening as it is.
It is the only option that gives the designed-dark look and keeps every rule of #916: off by
default, a page's own dark ground left as written, per-site off, and no white while loading. It also
costs nothing per load. The page script would match the look, but at a per-load cost, with a flash,
and visibly to the page, and Omaweb would have to keep up with the per-site fixes Dark Reader
maintains. Tuning cannot reach the look.

The ground script stays: it is what keeps a page's own dark theme as written, and Chromium's
per-element check only sees `color-scheme`. The cover gets simpler. A darkened white ground is the
chosen ground colour exactly, rather than an inversion that differs by architecture (#936).

## Screenshots

Captured on 2026-11-24 on x86_64, in headless cage at 1280 by 720 with pixman. The pages are a local
test page, `roles.html`, that puts the roles side by side (a brand header, a white and a tinted
card, a warning box, inline green text, a light gradient, a box shadow, an inline SVG, a canvas
photo and swatches from `#ffffff` to `#18181b`), Hacker News, the GNU home page and Wikipedia's
"Lichen" article. All four are light-only. The captures are kept for review and are not published
yet.

| Browser                      | How                                                                                                                   |
| ---------------------------- | --------------------------------------------------------------------------------------------------------------------- |
| Omaweb, off and darkened     | the worktree's build on the `omaweb-qtwebengine` 6.140.0 engine, a throwaway profile, `"darken-pages"` false and true |
| Omaweb, forced dark variants | the same, darkened, with `--dark-mode-settings` in `QTWEBENGINE_CHROMIUM_FLAGS`, on `roles.html` only                 |
| Dark Reader                  | Chromium 155.0.8059.84 (Arch), a throwaway profile, Dark Reader 4.9.135's Chrome MV3 release with its defaults        |
| Safari                       | not captured                                                                                                          |

Dark Reader ran in Chromium, not Omaweb, because Omaweb loads only a Known extension, Bitwarden or
1Password, by its store id ([KnownExtensions.cpp](../../src/core/KnownExtensions.cpp)). Dark Reader
is a page script, so its output depends on the page and its CSS, not on the Chromium version, but
Chromium's window was 1280 wide against Omaweb's page area of 988, so the layouts differ. Its
install hook opens a help tab over the page, which was removed from the unpacked copy for the
captures. Nothing else in it was changed.

Safari could not be captured. Safari 27.3 runs only on Apple's systems, and no Mac took part. The
WebKit ports that run on Linux, GTK and WPE, cannot stand in: the colour transform and the detection
are in Apple's closed WebKitAdditions, as [above](#not-found-in-a-primary-source).

What the captures show:

- Current Page darkening, `roles.html`: white and `#f3f4f6` grounds turn near black, and body text
  turns near white. The brand header `#0b63ce` and the orange buttons are under 205 and keep their
  colour. The swatches show the 205 cliff: `#f5f5f5` and `#d0d0d0` turn dark while `#cccccc` stays
  light grey, and `#6b7280` stays as written, so the row reads light, dark, dark, light, grey, dark.
  The SVG square, a foreground, turns from `#0b63ce` to a pale blue, and the circle's dark stroke
  turns white. The light gradient turns dark at its white end and stays light blue at the other. The
  box shadows are lost on the dark ground. The canvas photo is left alone.
- Current Page darkening, real pages: Hacker News' `#f6f6ef` ground turns a dark olive and its
  orange bar stays. GNU's navy banner and red menu stay, its white ground turns dark, and the black
  line drawing of the GNU head, an image, is inverted to white. Wikipedia's text and links read
  well, its photos are left alone, and its grey toolbar icons, which are images, are dimmed.
- Dark Reader, `roles.html`: every ground lands on its tinted `#181a1b` and the swatches step
  smoothly from dark to darker, with no cliff at 205, because the background mapping is continuous.
  Brand colours are darkened rather than kept: the header and the orange buttons are a step darker.
  The SVG square keeps a dark blue, but the circle's pale yellow fill turns olive, since an SVG fill
  is treated as a background. The gradient is dark end to end. The warning box keeps an amber text
  on a brown ground, close to how a designed dark theme draws one.
- Dark Reader, real pages: Hacker News and GNU are close to the engine's result, with a warmer
  ground and slightly dimmer text. On Wikipedia the page and its sidebars are all dark and the
  photos are left alone.
- Forced dark variants, `roles.html`: `InversionAlgorithm=2` (HSL) puts the ground on pure black and
  turns `#f3f4f6` a mid grey and the warning box a mustard yellow. `InversionAlgorithm=1`
  (brightness) turns yellows blue and blues yellow: the warning box goes blue and the SVG square
  pale yellow. Both are worse than the default CIELAB. `BackgroundBrightnessThreshold=150` also
  inverts `#cccccc` and the gradient's light end, which closes the cliff on this page but would
  invert a page's own mid-grey grounds. `ImagePolicy=0` and `ImagePolicy=1` drew the same pixels as
  the default, since the only image here is a photo, which the classifier leaves alone either way.

The variants confirm [the reading of the code](#can-a-setting-get-closer-to-safari): no setting
gives the roles separate outputs or keeps a brand colour while darkening the ground.

## Follow-up issue draft

Title: Draw darkened pages with a role-aware colour transform in the engine

Body:

```markdown
## What

Page darkening inverts the lightness of every colour Chromium's forced dark picks, whatever its
role, so a darkened page looks inverted rather than designed dark. `docs/research/page-darkening.md`
(#973) found that Blink's dark mode filter already knows each colour's role (background, text,
border, SVG, selection) and that no setting changes the inversion. This adds a role-aware transform
to the engine, modelled on Dark Reader's colour functions, and turns it on in Omaweb.

## Engine (omaweb-qtwebengine-patches)

- Add `DarkModeInversionAlgorithm::kRoleAware` (`InversionAlgorithm=4`) to `dark_mode_settings.h`
  and `dark_mode_color_filter.cc`. Backgrounds, selection and gradient stops map toward a ground
  colour, text and list markers toward a text colour, and borders onto a mid-to-dark range. SVG
  takes the background mapping, measured on real logos and icons. Neutral colours take the ground's
  hue.
- Map every background under this algorithm, with no per-element threshold, so the 205 cliff goes.
  The ground script's page limit of 205 stays.
- Raise text and list markers against `contrast_background` to at least 4.5:1.
- Pass the role to `DarkModeColorFilter::InvertColor`, and key the inverted colour cache on colour
  and role.
- Read the ground and text colours from two new `--dark-mode-settings` parameters.
- Keep the image classifier as it is.
- Record the patch in `MODIFICATIONS.md` and the series README.

## Omaweb

- `src/main.cpp` adds `--dark-mode-settings=InversionAlgorithm=4,...` to the engine's command line,
  as it adds the video decode flags.
- The cover for a darkened page is the reported ground through the same background mapping, not the
  inverted lightness with a 7% floor (`coverGroundColor` in `src/engine/qt/EngineView.qml`).

## Requirements changes (`docs/product/requirements.md`)

- Page darkening: replace "Chromium's forced dark mode inverts the lightness of a background whose
  brightness is above 205 of 255, so white becomes `#121212` ..." and the foreground examples with
  what the role-aware transform draws, measured on the same 17 grounds and the `#222`, `#eee` and
  `#d4d4d8` probes.
- Drop the aarch64 paragraph about `#1b1b1b` (#936) if the new transform no longer goes through
  Chromium's grey snap, after measuring both architectures.
- The cover paragraphs ("the reported ground inverted while the page is darkened" and "that colour
  with its lightness inverted, down to a floor of 7%") describe the new mapping.
- Keep unchanged: off by default, not following the Browser theme, `"darken-pages"`, the ground read
  from 25 points with 205 as the limit, per-site off in Site information, darkening from load start
  so no white shows, and Auxiliary windows following the same rule.

## Engine contract changes

- `src/engine/api/EngineCapabilities.h` and `src/engine/api/EngineViewContract.cpp`: the comments on
  `PageDarkening` and `darkenPage` stop naming Chromium's forced dark mode and say what a darkened
  page promises in engine-neutral terms: light grounds drawn dark, text drawn light, borders kept
  visible, text at least 4.5:1 against what is behind it, a ground that is already dark left as
  written, computed styles reading what the page wrote, and `prefers-color-scheme` reporting the
  scheme the browser prefers, unchanged by darkening.
- `src/engine/ladybird/README.md`: Ladybird still lacks `PageDarkening`, and the README says what a
  LibWeb paint-time hook would need to offer to gain it.

## Done when

- The patch is in the series and the engine package that carries it is required by Omaweb.
- Screenshots of the pages compared in #973 show the new transform beside the old one.
- Page darkening's tests pass with the requirements' new measured values.
```
