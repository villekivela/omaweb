// This runs in the application world. Only the native touchpad recognizer asks it a question;
// neither a site's script nor a mouse wheel can ask the browser to navigate through this helper.
globalThis.__omawebHistorySwipeCanStart = (x, y, direction) => {
  let element = document.elementFromPoint(x, y);
  if (!element || document.fullscreenElement) return false;
  while (element) {
    if (element.shadowRoot) {
      const inner = element.shadowRoot.elementFromPoint(x, y);
      if (inner && inner !== element) {
        element = inner;
        continue;
      }
    }
    break;
  }
  for (; element; element = element.parentElement || element.getRootNode().host) {
    // Frames have their own scroll chain. A canvas or a closed component can implement scrolling
    // without a DOM scroll container, so leave their gestures to them rather than guessing.
    if (
      ["IFRAME", "FRAME", "EMBED", "OBJECT", "CANVAS", "INPUT", "TEXTAREA"].includes(
        element.tagName,
      ) ||
      (element.localName.includes("-") && !element.shadowRoot)
    )
      return false;
    const style = getComputedStyle(element);
    const root = element === document.scrollingElement;
    const scrollable = root || /^(auto|scroll|overlay)$/.test(style.overflowX);
    if (!scrollable) continue;
    if (["contain", "none"].includes(style.overscrollBehaviorX)) return false;
    const range = element.scrollWidth - element.clientWidth;
    if (range <= 1) continue;
    // Chromium's RTL scrollLeft runs from -range at the left to zero at the right.
    const left = style.direction === "rtl" ? -range : 0;
    const right = style.direction === "rtl" ? 0 : range;
    if (direction < 0 ? element.scrollLeft > left + 1 : element.scrollLeft < right - 1)
      return false;
  }
  // The viewport's overscroll policy may be on html even when the body is the scrolling element.
  const policy = getComputedStyle(document.documentElement).overscrollBehaviorX;
  return !["contain", "none"].includes(policy);
};
