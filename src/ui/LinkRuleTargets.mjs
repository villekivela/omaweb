// What a Link rule names in place of a Space to open its links in a Private
// window (linkrules::privateWindow).
export const privateWindow = "private";

// The targets a Link rule picker offers: the reader's Spaces, then a Private
// window. A Space the rule names that the reader has not got is listed too, so
// the picker shows what the rule says. A Space called what a Private window is
// called cannot be a target, so it is not offered as one. `privateLabel` is
// the caller's, since a module has no catalogue to translate it from.
export function targets(spaceNames, current, privateLabel) {
  const list = spaceNames
    .filter((name) => name !== privateWindow)
    .map((name) => ({ value: name, label: name }));
  if (current.length > 0 && current !== privateWindow && spaceNames.indexOf(current) < 0)
    list.push({ value: current, label: current });
  list.push({ value: privateWindow, label: privateLabel });
  return list;
}
