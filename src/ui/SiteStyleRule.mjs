// The reader's site styles (`styles`, the browser's SiteStyles) for the pages
// of a Space, or null for an Agent Space's, so an Agent reads a page as the
// site serves it. A Private window's pages have no Agent Space and take them.
// One rule for every engine the chrome builds, a tab's and an Auxiliary
// window's alike.
export function stylesFor(styles, browser, spaceId) {
  if (!styles) return null;
  return browser && browser.agentSpace(spaceId) ? null : styles;
}
