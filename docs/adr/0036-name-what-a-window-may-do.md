# Name what a window may do

What a Private window may do is a value the window holds. `WindowCapabilities` names four
capabilities, `Spaces`, `PinnedTabs`, `HistorySearch` and `ClearBrowsingData`, and answers `allows`
for each. A window is built with the table its kind answers from, a main window's or a Private
window's, and the fourteen places that used to refuse an action ask that table by name.

The rule used to be fourteen tests of the private-browsing flag spread through `BrowserController`:
six around Spaces, four around Pinned tabs and Keep active, three around the Omnibar's search, and
one on the clear command. Each was a correct answer to the same question, asked fourteen times, and
nowhere was the product surface [ADR 0012](0012-isolate-private-browsing-by-window.md) describes
stated as a whole. All but two of the fourteen had no test: one existing test asserted the clear
command and the Omnibar's search, and the new suite has taken both over.

An answer is given by both kinds of window or by neither. The tables are built through one
constructor that takes every answer, so a fifteenth capability that only a main window has does not
compile until a Private window's answer is written down beside it. The obligation is on the answer
rather than on the name: a name added to the enum with nothing behind it leaves the switch in
`allows` incomplete, which the compiler warns about rather than refuses. Making it refuse would take
a count of the capabilities in the enum, which is a name in the enum that is not a capability.

Neither obliges a new call site to ask. A capability the shell reaches without asking the window
whether it has it is still possible, and nothing in the type catches it. What catches it is
`tests/core/tst_browsercontroller.cpp`, which now runs the same asks against a main window and a
Private window, allowed in the one and refused in the other, across all fourteen sites. Two of the
sites answer for a case nothing reaches today and the suite can only assert what is observable: a
window with no search thread never receives a result to suppress, and no close a window remembers
was ever pinned, because closing spares a Pinned tab.

Whether a window is private stays a separate fact. `privateBrowsing()` answers from the flag, not
from the capability set, and nothing exposes capabilities to QML. Ten tests of the flag remain and
none of them is about a capability: session storage, reader configuration, the profile-path readers
(#138), resetting Site permissions, closing the window on its last tab, the blank tab a Private
window starts with, and the coalesced persist. So does the one place the flag is read to name rather
than to refuse, where a notification's Space is rendered as "Private".

Reopening a closed tab is the exception in shape. It clears pinning and Keep active rather than
refusing, because a tab may come back into a window that has no Pinned section for it, and that is
what it did before.

What was rejected:

- **An adapter split, as [ADR 0035](0035-keep-the-private-browsing-rule-in-the-session-store.md)
  made for storage.** Spaces, pinning and history behind interfaces whose private implementations
  refuse is the only shape where forgetting to ask is structurally impossible. Unlike the session
  store there is no existing collaborator behind the pinning refusals to put an adapter in front of,
  so it means inventing three seams. That is several changes, not one.
- **Injecting the capability set into the constructor.** Rejected for ADR 0035's reason pointed the
  other way: a test wanting a main window with no Spaces would silently have built a Private window.
  The set is built from the window's kind and from nothing else.

See #137.
