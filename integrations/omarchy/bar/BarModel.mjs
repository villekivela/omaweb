// A Space's name, or its first letter in upper case where the reader asked for
// short names.
export function spaceLabel(name, letters) {
  const text = String(name || "");
  return letters ? text.charAt(0).toLocaleUpperCase() : text;
}

// The tab switcher's board: a column for each Space in the browser's order,
// its Pinned tabs first. Typing keeps the tabs with every word in their title
// or host and leaves out a Space with none. The cursor (`column`, `row`)
// starts on the tab on show, or with words typed on the first tab left, in
// the Space on show if it has one; both are -1 with nothing to take.
export function board(state, filter) {
  const words = String(filter || "")
    .toLocaleLowerCase()
    .split(/\s+/)
    .filter((word) => word !== "");
  const matches = (tab) => {
    const text = (String(tab.title || "") + " " + String(tab.host || "")).toLocaleLowerCase();
    return words.every((word) => text.includes(word));
  };
  const columns = state.spaces
    .map((space) => ({
      space,
      tabs: space.pins
        .map((tab) => ({ tab, pinned: true }))
        .concat(space.tabs.map((tab) => ({ tab, pinned: false })))
        .filter((entry) => matches(entry.tab)),
    }))
    .filter((column) => words.length === 0 || column.tabs.length > 0);
  let column = columns.findIndex(
    (each) => each.space.id === state.activeSpace && each.tabs.length > 0,
  );
  if (column === -1) column = columns.findIndex((each) => each.tabs.length > 0);
  if (column === -1) return { columns, column: -1, row: -1 };
  const shown = columns[column].tabs.findIndex(
    (entry) => entry.tab.id === columns[column].space.active,
  );
  return { columns, column, row: words.length === 0 && shown >= 0 ? shown : 0 };
}

// Where the cursor lands moving `dx` Spaces sideways or `dy` tabs up or down
// from (`column`, `row`). Sideways passes a Space with nothing open, wraps at
// either end and keeps the row where the Space has one; up and down stop at
// the ends of the column.
export function moveOnBoard(columns, column, row, dx, dy) {
  if (dx !== 0) {
    const count = columns.length;
    let next = column;
    for (let step = 0; step < count; ++step) {
      next = (next + Math.sign(dx) + count) % count;
      if (columns[next].tabs.length > 0) break;
    }
    return { column: next, row: Math.min(row, columns[next].tabs.length - 1) };
  }
  const last = columns[column].tabs.length - 1;
  return { column, row: Math.max(0, Math.min(last, row + dy)) };
}
