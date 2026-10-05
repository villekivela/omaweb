// Where something is drawn, moved onto the display's pixel grid. Under a
// fractional scale such as 1.25 or 1.6 a whole logical pixel can fall between
// two of the display's, where borders and text are drawn soft and a page's
// texture splits along its diagonal (#571). At a whole scale this is
// `Math.round`. Only the drawn value is snapped: a width the reader chose is
// kept and written down in whole logical pixels.
//
// `ratio` is the window's `devicePixelRatio`, read in the binding that calls
// this, so a window moved to a display of another scale snaps again.
export function snap(value, ratio) {
  return Math.round(value * ratio) / ratio;
}
