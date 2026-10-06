// Whether anything in Settings is waiting on the reader: a keymap that
// could not honour every binding, or an input method the desktop names but
// did not install. The page draws the notices and the sidebar's settings
// button carries a mark, and the button's is on show before the page is
// built (#618), so both ask here.
export function needed(keyboardReport, inputMethodAvailable) {
  return keyboardReport.length > 0 || !inputMethodAvailable;
}
