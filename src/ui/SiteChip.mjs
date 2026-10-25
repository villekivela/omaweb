// The side of the box a tab row draws its favicon in. The stand-in the chip
// draws is two characters of the theme's smallest type, so the box is derived
// from that size rather than fixed: a theme with a larger font would otherwise
// clip its own letters. The address trigger's lock takes the same size (#779).

export function side(captionSize) {
  return Math.round(captionSize * 1.6);
}
