// Whether the engine is asked to darken the page at an address: the reader
// turned darkening on (`setting`, the browser's PageDarkening) and has not
// turned it off for the page's site in the page's Space. A page with no site
// has no choice to turn off. One rule for every engine the chrome builds, a
// tab's and an Auxiliary window's alike.
export function wanted(setting, browser, address, spaceId) {
  if (!setting || !setting.enabled) return false;
  return !browser.darkeningOff(address, spaceId);
}
