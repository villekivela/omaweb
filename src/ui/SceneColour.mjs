// Colour and scatter arithmetic the Start page's Scenes draw by: the night
// road (NightRoad.qml) and the night sky (NightSky.qml). Each Scene resolves
// its own roles; these only mix, fade and write colours, and scatter indices.

// `from` mixed toward `to` by `amount`, from 0 to 1, opaque.
export function mix(from, to, amount) {
  const a = Qt.color(from);
  const b = Qt.color(to);
  return Qt.rgba(
    a.r + (b.r - a.r) * amount,
    a.g + (b.g - a.g) * amount,
    a.b + (b.b - a.b) * amount,
    1,
  );
}

// A colour at an alpha, for an item to draw.
export function withAlpha(colour, alpha) {
  const c = Qt.color(colour);
  return Qt.rgba(c.r, c.g, c.b, alpha);
}

// A colour as a canvas takes it, at an alpha, opaque when none is given.
export function css(colour, alpha) {
  const c = Qt.color(colour);
  const channel = (value) => Math.round(value * 255);
  return `rgba(${channel(c.r)}, ${channel(c.g)}, ${channel(c.b)}, ${alpha === undefined ? 1 : alpha})`;
}

export function fraction(value) {
  return value - Math.floor(value);
}

// A repeatable scatter from 0 to 1: the same index and seeds always give the
// same value.
export function scatter(index, seeds) {
  return fraction(
    Math.sin(index * seeds[0] + (seeds.length > 2 ? seeds[1] : 0)) * seeds[seeds.length - 1],
  );
}
