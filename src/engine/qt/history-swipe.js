// This runs in the application world. Only the native touchpad recognizer asks it a question;
// neither a site's script nor a mouse wheel can ask the browser to navigate through this helper.
globalThis.__omawebHistorySwipeCanStart = (x, y, direction) => {
  // Whether the point lies over a text node of the element's own light DOM. A closed shadow root
  // draws only what it slots, so its host's other light-DOM text has no client rects.
  const overOwnText = (element) => {
    const range = document.createRange();
    return Array.from(element.childNodes).some((child) => {
      if (child.nodeType !== Node.TEXT_NODE) return false;
      range.selectNodeContents(child);
      return Array.from(range.getClientRects()).some(
        (rect) => x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom,
      );
    });
  };
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
  const hit = element;
  for (; element; element = element.parentElement || element.getRootNode().host) {
    // Frames have their own scroll chain. A canvas or a closed component can implement scrolling
    // without a DOM scroll container, so leave their gestures to them rather than guessing. A
    // closed shadow root's content is hit as its host, so only the hit element can be one, and
    // not where the point lies over its own light-DOM text. A custom element further up wraps
    // light-DOM content, as YouTube's do (#960).
    if (
      ["IFRAME", "FRAME", "EMBED", "OBJECT", "CANVAS", "INPUT", "TEXTAREA"].includes(
        element.tagName,
      ) ||
      (element === hit &&
        element.localName.includes("-") &&
        !element.shadowRoot &&
        !overOwnText(element))
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
