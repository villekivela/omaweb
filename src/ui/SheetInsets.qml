import QtQuick
import qs.Commons

// The inset a full-window sheet leaves above its own heading.
//
// Settings, History and the Start page are the same thing from where a reader
// sits: a sheet standing over the page, with an eyebrow above a heading in its
// top left. Each of them used to name its own top inset -- 40, 48 and 56 -- so
// the heading landed in a different place depending on which one had been
// opened, and the odd one out was not even scaled by the theme's spacing. Side
// by side in the website's screenshots the three gaps read as a mistake,
// because that is what they were.
//
// Instantiated rather than a singleton: `src/ui` is resolved as a directory
// import, so a singleton would need a `qmldir` and every other type in here
// would start resolving through it.
QtObject {
    readonly property int top: Style.space(48)
}
