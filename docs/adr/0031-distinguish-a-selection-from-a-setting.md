# Distinguish a selection from a setting

The settings page used one control, `SettingToggle`, for three different
contracts. Six of its eleven call sites were settings: flipping "Use site
favicons" changes the browser now and it stays. Five were not. Privacy's
Cookies, Site storage, Cache, Site permissions and History built an array that
only the *Clear this Space* button read, so flipping one changed nothing at all
until a different control was pressed. "Every Space" was a third meaning again —
action scope, quietly gating a confirmation field, relabelling the button and
widening what the action destroyed. The adapter already half-admitted it:
`SettingToggle.qml` declared `Accessible.role: Accessible.CheckBox` on a control
drawn as a switch, and it was the only `Accessible.CheckBox` in `src/`.

So the interface now names four kinds and holds them apart. A **setting** is a
switch: the browser changes now and stays changed. A **selection** is a
checkbox: an argument to an action that has not happened yet. A **scope** is a
dropdown: how far an action reaches. An **action** is a button that opens a
dialog, and a destructive action composes its arguments inside that dialog
rather than on the page. Persistence is orthogonal to all of this. A selection
that survives a restart is still a selection, because nothing happens until the
button is pressed — a future reader who finds a persisted checkbox and concludes
it should have been a switch would be undoing the decision, not tidying it.

Prior art decided the shapes. Chrome, Firefox, Edge and Safari all draw the data
categories as checkboxes and the time range as a dropdown; none of them uses a
switch, and Firefox persists the categories in prefs of its own
(`privacy.clearHistory.*`, `privacy.clearSiteData.*`) beside
`privacy.sanitize.timeSpan`. All four also put the whole form in a modal reached
from a settings row or a menu item, and none of them asks a second time
afterwards: opening the modal deliberately is the confirmation. Omaweb had
inverted that, keeping the form on the page and inventing an inline typed
`CLEAR ALL` field to guard it. The design that was settled first kept the form
inline and moved only the confirmation into a dialog, which would have cost a
click no shipping browser charges. Following the prior art costs less and asks
less.

That is also the stronger reading of the claim `CommandDialog.qml` already
makes, that every question about the browser itself arrives in the same shape.
Deleting a Space had honoured it; clearing browsing data had not. But
`CommandDialog` is a fixed three-part shape whose body is a message, one
optional field and an optional row list, and whose root `Keys.onPressed` owns
Up, Down, Enter and Escape — a form of checkboxes and dropdowns cannot live
inside it, because `Down` means "step the rows" to the dialog and "open the
list" to a dropdown. Giving it a content slot would have made a component that
owns those keys also host children that need them, and a second caller does not
make that conflict better. So the grammar was moved down a level: `DialogPanel`
holds the scrim, the bordered panel, the head and the foot, `CommandDialog`
became one user of it and `ClearBrowsingDataDialog` another, each owning its own
body and its own keys. One grammar, two dialogs, no container.

One thing has no prior art. Firefox clears a single profile and Chrome's dialog
is per-profile with no cross-profile option, so no mainstream browser offers
"clear every profile" as a single act; Omaweb offers it because Spaces are core
to it in a way profiles are not to Chrome. Having no tested pattern to borrow,
scope is the one value that is not remembered: the categories and the time range
persist through `setPreference`, and scope resets to the active Space on every
launch. `docs/product/requirements.md` calls clearing every Space a separate
explicit choice, and a choice inherited from a config file written weeks ago is
a default rather than a choice. The typed `CLEAR ALL` guard in
`BrowserController::clearBrowsingData` stands either way; it did not move, and
neither did anything else in the core (0011). Only the supplier of that string
changed.

See #84.
