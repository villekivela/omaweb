// The fill the sidebar's quiet surfaces rest on, the address line and the
// Pinned tabs: the theme's text at 7%, 11% under the pointer (#779). They
// have no border, so the fill is what draws them.

export function resting(text, hot) {
  const c = Qt.color(text);
  return Qt.rgba(c.r, c.g, c.b, hot ? 0.11 : 0.07);
}
