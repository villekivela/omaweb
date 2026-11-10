import QtQuick
import qs.Commons

// Where the reader writes a site's style and hides parts of its pages: a panel
// docked opposite the sidebar, beside the page it styles. It stays open across
// navigation and tab switches and follows the host of the page on show.
//
// What it edits is a file, `styles/<host>.css` (`SiteStyles`), so the reader's
// own editor, the live reload and Sync see each change too.
Rectangle {
    id: root

    property var colors
    property string iconFontFamily
    // The reader's site styles (`SiteStyles`).
    property var styles: null
    // The address of the page on show.
    property url pageUrl

    // The host the panel writes: the page's registrable site, unless the
    // reader picked a narrower one for this site.
    readonly property string site: root.styles ? root.styles.siteFor(root.pageUrl) : ""
    property string chosenHost: ""
    readonly property string host: root.chosenHost.length > 0 ? root.chosenHost : root.site

    signal closeRequested

    onSiteChanged: root.chosenHost = ""

    color: root.colors.sidebar

    // The seam the page stops at.
    Rectangle {
        x: root.dockedLeft ? root.width - width : 0
        width: Style.spacing.hairline
        height: root.height
        color: root.colors.separator
    }

    property bool dockedLeft: false
}
