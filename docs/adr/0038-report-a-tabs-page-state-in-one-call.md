# Report a tab's page state in one call

The engine adapter reports what it sees about one tab, in one call carrying the address, title,
icon, whether the page is loading and whether it is making sound. The core diffs the report against
what it holds, decides what changed, what is worth persisting and when, and in what order the model
hears about it. Ordering is an invariant inside the core rather than a reading of
`TabEngineHost.qml`, which used to drive four tab setters plus history from seven engine
property-change handlers.

This is [ADR 0011](0011-drive-the-interface-through-core-commands.md) applied to the last place that
did not follow it, not a revision of it. Two rules in it are new, because neither follows from "QML
submits commands":

The core decides what a visit is. A report whose load has finished on a real address is what records
history, so the conditions that will accumulate around that rule, a Private window keeping none, a
failed load earning none, a navigation within a document earning none, accumulate in one place.
`recordVisit` stays the history write API for the lab and for tests; the engine host is what stops
calling it.

The core keeps a tab's icon and audibility across a Space reload. Neither is persisted: artwork
belongs to the loaded page and a stored `audible` would claim a page is making sound before it has
loaded. But by [ADR 0033](0033-stop-an-away-spaces-pages-instead-of-taking-them.md) the tabs and
their engines both survive a Space switch, so a core that reloads its tabs from the store and drops
what it knows about a tab that never went away is asking the adapter to tell it again. It used to
ask exactly that, in two restore paths and a discard path.

An engine names no address between pages. A report with an empty address leaves the tab's address
and title alone and applies the rest of what it carries, so the loading state that changes during
that gap is not lost. The guard belongs in the core, stated as a rule: an adapter that withholds the
whole report while address-less would turn a page-blanking rule into a missing-loading-state bug.

What was rejected:

- **A `QVariantMap` report**, in the style of `notificationTarget`. Its keys are checked by no
  compiler and no test, which is the failure mode [0037](0037-count-refusals-per-page-address.md)
  had just finished removing from Content blocking.
- **Handing the core the engine object** to read the properties off. The engine-view contract
  already guarantees the property names, and a C++ test could pass a stub, but it makes the core
  read the adapter's own surface across the boundary
  [0009](0009-separate-browser-policy-from-engine-mechanics.md) draws.
- **Splitting the report by lifetime**, one call for what a session keeps and one for what it does
  not. Which facts survive a restart is the core's business, and choosing a call by persistence
  semantics hands the adapter back the decision this removes.
- **Persisting the icon and the audibility** so a Space reload could answer from the store. Both are
  facts about a loaded page rather than about a saved tab.

Two engine events stay their own calls. An origin interaction happened once and is keyed by origin
across every tab showing it, and a renderer failure is a transition carrying a reason. Neither is a
fact that would be true again in the next report, so folding either into one would mean re-firing it
or carrying a flag that says it just happened.

See #143.
