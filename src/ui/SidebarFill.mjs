// The fill the sidebar's quiet surfaces rest on, the address trigger and the
// Pinned tabs: the theme's text at 7%, 11% under the pointer (#779). They
// have no border, so the fill is what draws them.

export function fill(text, hovered) {
  return Qt.alpha(text, hovered ? 0.11 : 0.07);
}
