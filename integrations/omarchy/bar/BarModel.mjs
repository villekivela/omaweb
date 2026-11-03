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
// the Space on show if it has one. Row -1 is a Space's heading, where it
// starts with no tab open anywhere; both are -1 with no Space left.
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
  if (column === -1 && columns.length === 0) return { columns, column: -1, row: -1 };
  if (column === -1) {
    const shown = columns.findIndex((each) => each.space.id === state.activeSpace);
    return { columns, column: Math.max(0, shown), row: -1 };
  }
  const shown = columns[column].tabs.findIndex(
    (entry) => entry.tab.id === columns[column].space.active,
  );
  return { columns, column, row: words.length === 0 && shown >= 0 ? shown : 0 };
}

// Where the cursor lands moving `dx` sideways or `dy` up or down from
// (`column`, `row`), with the pins `perLine` tiles to a line above the rows.
// The pins move as a grid; sideways off the end of a line or a row goes to
// the next Space with anything open, wrapping at either end, onto the same
// line where it has one. Up from the top reaches the Space's heading, and
// along the headings every Space is a stop. Up and down stop at the ends of
// the column.
export function moveOnBoard(columns, column, row, dx, dy, perLine = 1) {
  if (row < 0) {
    if (dx !== 0) {
      const count = columns.length;
      return { column: (column + Math.sign(dx) + count) % count, row: -1 };
    }
    return { column, row: dy > 0 && columns[column].tabs.length > 0 ? 0 : -1 };
  }
  const pinsIn = (index) => columns[index].tabs.filter((entry) => entry.pinned).length;
  const linesIn = (index) =>
    Math.ceil(pinsIn(index) / perLine) + columns[index].tabs.length - pinsIn(index);
  const placeOf = (index, at) => {
    const pins = pinsIn(index);
    return at < pins
      ? { line: Math.floor(at / perLine), slot: at % perLine }
      : { line: Math.ceil(pins / perLine) + at - pins, slot: 0 };
  };
  // The tab at `slot` on `line`, or the last on a shorter line.
  const rowAt = (index, line, slot) => {
    const pins = pinsIn(index);
    const pinLines = Math.ceil(pins / perLine);
    return line < pinLines ? Math.min(line * perLine + slot, pins - 1) : pins + line - pinLines;
  };
  const here = placeOf(column, row);
  if (dx !== 0) {
    const along = here.slot + Math.sign(dx);
    if (row < pinsIn(column) && along >= 0 && along < perLine) {
      const next = rowAt(column, here.line, along);
      if (next !== row && placeOf(column, next).line === here.line) return { column, row: next };
    }
    const count = columns.length;
    let next = column;
    for (let step = 0; step < count; ++step) {
      next = (next + Math.sign(dx) + count) % count;
      if (columns[next].tabs.length > 0) break;
    }
    const line = Math.min(here.line, linesIn(next) - 1);
    return { column: next, row: rowAt(next, line, dx > 0 ? 0 : perLine - 1) };
  }
  if (here.line + dy < 0) return { column, row: -1 };
  const line = Math.min(linesIn(column) - 1, here.line + dy);
  return { column, row: rowAt(column, line, here.slot) };
}

// The Spaces the board shows: at most four, as many as `fits` on the screen,
// from `first`. It moves only when the cursor's `column` of `count` would
// leave it, just far enough to keep it in view. `before` and `after` say a
// side has more Spaces than it shows.
export function boardView(first, column, count, fits) {
  const shown = Math.min(4, fits, count);
  let start = Math.min(first, count - shown);
  if (column >= 0 && column < start) start = column;
  if (column >= start + shown) start = column - shown + 1;
  start = Math.max(0, start);
  return { first: start, shown, before: start > 0, after: start + shown < count };
}
