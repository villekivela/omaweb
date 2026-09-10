# Page loading indicator research

This note compares loading indicators in Zen, Arc, and Helium. It informs
[issue #198](https://github.com/villekivela/omaweb/issues/198) but is not an implementation
contract.

## Zen

Zen shows an indeterminate pill at the top center of the page viewport for the selected tab. The
indicator starts as a five-rem pill that pulses between 85% and 95% scale. After a load lasts three
seconds, it settles at ten rem and animates a highlight across the pill. It fades out when loading
ends. Zen disables the effect when the page enters DOM fullscreen or the reader turns on reduced
motion, and a preference enables the indicator by default.
[controller](https://github.com/zen-browser/desktop/blob/2e9aecd41b42e7a34ec3be96bc2a650e45884999/src/zen/common/sys/ui/ZenProgressBar.sys.mjs),
[styles](https://github.com/zen-browser/desktop/blob/2e9aecd41b42e7a34ec3be96bc2a650e45884999/src/zen/common/styles/zen-single-components.css),
[animations](https://github.com/zen-browser/desktop/blob/2e9aecd41b42e7a34ec3be96bc2a650e45884999/src/zen/common/styles/zen-animations.css),
[default preference](https://github.com/zen-browser/desktop/blob/2e9aecd41b42e7a34ec3be96bc2a650e45884999/prefs/zen/view.yaml)

Zen hides Firefox's tab throbber by default. Its own progress indicator appears only for the
selected tab, so background loads do not animate in the tab list.
[tab preference](https://github.com/zen-browser/desktop/blob/2e9aecd41b42e7a34ec3be96bc2a650e45884999/prefs/zen/theme.yaml),
[tab styles](https://github.com/zen-browser/desktop/blob/2e9aecd41b42e7a34ec3be96bc2a650e45884999/src/zen/tabs/zen-tabs/vertical-tabs.css)

## Arc

Arc first moved its loading indicator into the address bar in December 2021. In February 2023 it
replaced that design with an animation at the top center of the window. Arc's release notes describe
the latter as showing page loading progress and later record a fix that made it visible in browser
fullscreen. The Browser Company does not publish the implementation, so the release notes establish
placement but do not establish whether the animation tracks a measured percentage.
[2021 release notes](https://resources.arc.net/hc/en-us/articles/20498463803799-Arc-for-macOS-2021-Release-Notes),
[2023 release notes](https://resources.arc.net/hc/en-us/articles/20498377604887-Arc-for-macOS-2023-Release-Notes)

## Helium

Helium draws a determinate bar over the bottom edge of its location bar. It follows Chromium's
reported load progress, starts at no less than 5%, animates each increase over 180 milliseconds,
pulses while loading, and fills and fades when loading ends. The line uses the current theme's
primary color, with a glow at its leading edge. It appears only for HTTP and HTTPS pages and hides
while the location bar has focus. A setting controls it and defaults to on.
[progress-bar patch](https://github.com/imputnet/helium/blob/45a4e3b84d6f17e103af7b38f3ee5a15d7ae7d95/patches/helium/ui/progress-bar.patch)

Helium also disables Chromium's animated network state in the tab icon. Loading feedback therefore
stays with the location bar rather than replacing each tab's favicon.
[tab-icon patch](https://github.com/imputnet/helium/blob/45a4e3b84d6f17e103af7b38f3ee5a15d7ae7d95/patches/helium/ui/tab-icon.patch)

## Direction for Omaweb

Zen and Arc both reserve the top center of the page area for a small indicator. That position works
when the browser has no persistent location bar. Helium's measured bar is useful evidence for
animation timing and completion behavior, but its placement depends on chrome Omaweb does not have.

Omaweb's engine contract currently exposes `loading` as a boolean and no load percentage. An
indeterminate top-center indicator can use that contract unchanged across the development engine and
target engine. It should belong to the active page, stay out of the sidebar and tab favicons, stop
when `loading` becomes false, and respect the existing reduced-motion behavior. It should not appear
on the Start page because the Start page has no engine load.
