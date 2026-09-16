import QtQuick

// The floor a sheet or overlay stands on: the wheel stops here.
//
// A sheet over a page does not take the page away. The page is still drawn
// beneath it and still answers, and a wheel turned over the sheet reaches the
// page whenever nothing in the sheet accepted it. The sheet's own scroll view
// accepts one only while it has somewhere to go, so a wheel at either end of
// its travel falls through, and a wheel turned over a margin, a rail or a
// scrim falls through with nothing in the way at all. The page moves under
// the sheet that covers it, and the reader finds it somewhere else when the
// sheet closes.
//
// Declared as a child of the sheet, on which it then stands. What the sheet
// draws is asked first, so a scroll view or a list that can still move keeps
// the wheel; what they refuse reaches this and goes no further.
WheelHandler {}
