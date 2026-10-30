// The spacing on either side of a favicon, widest first. The tabs take the
// widest that fits before any of them is hidden.
const pads = [7, 5, 3];

// Which of a Space's tabs fit in `room` pixels along the bar, each a favicon
// `iconLength` long: every one at the widest spacing that fits, or else the
// active tab and the ones nearest it, from either side, with the rest counted
// in `hidden` for the "+N" mark.
export function fitTabs(tabs, activeId, room, iconLength) {
  for (const pad of pads) {
    if (tabs.length * (iconLength + pad * 2) <= room) return { pad, tabs, hidden: 0 };
  }
  const pad = pads[pads.length - 1];
  const fit = Math.max(1, Math.floor(room / (iconLength + pad * 2)));
  const at = Math.max(
    0,
    tabs.findIndex((tab) => tab.id === activeId),
  );
  const nearest = tabs
    .map((tab, index) => index)
    .sort((left, right) => Math.abs(left - at) - Math.abs(right - at) || left - right)
    .slice(0, fit);
  const shown = tabs.filter((tab, index) => nearest.includes(index));
  return { pad, tabs: shown, hidden: tabs.length - shown.length };
}

// A Space's name, or its first letter in upper case where the reader asked for
// short names.
export function spaceLabel(name, letters) {
  const text = String(name || "");
  return letters ? text.charAt(0).toLocaleUpperCase() : text;
}
