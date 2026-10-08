// What one of the reader's Spaces is drawn in: the colour the theme resolves
// for the Space colour's name, or the muted text colour while the reader has
// turned Space colour off. An Agent Space is not drawn through this.
export function drawn(colors, name, coloured) {
  if (!coloured) return colors.mutedText;
  const spaces = colors.spaces;
  return spaces && spaces[name] ? spaces[name] : colors.accent;
}
