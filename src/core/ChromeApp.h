#pragma once

#include <QString>

namespace omaweb {

// Chrome's renderer gives every page a `window.chrome.app`, and QtWebEngine
// gives none. Without it Google's sign-in turns the browser away as one that
// "may not be secure" (rrk=46) as soon as an address is entered (#711). This
// is the object Chrome answers with for a page that is not an installed app.
class ChromeApp final {
public:
    // Meant to run before the page's own scripts, in the main world, in every
    // frame. A `chrome.app` that already exists is left as it is.
    static QString scriptSource();
};

} // namespace omaweb
