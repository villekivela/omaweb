# Own a window's downloads in core

`Downloads` is a list model in core, held one per window by `BrowserController` and exposed to QML
as a property on it. It is the window's downloads: the ones running now and the Download records its
Space kept, in one list with a role saying which. It owns its own mutations, so a caller starts,
answers or forgets a download and binds to what comes out.

Eight members moved off the controller onto it: `recordDownload`, `updateDownload`,
`downloadHistory`, `forgetDownload`, `downloadDisposition`, `noteDownloadStarted`,
`noteDownloadSettled` and `activeDownloadCount`. The last three collapsed into the intake, because a
list that is told every start and every settlement already knows how many an origin has running. The
download directory did not move: it is a setting rather than a selection
([ADR 0031](0031-distinguish-a-selection-from-a-setting.md)), it has a change signal the engine
adapter reads directly, and it belongs to the window whether or not anything is downloading.
Revealing a saved file did not move either: it is a platform action on a path and needs nothing the
list holds.

`src/ui/Main.qml` had held the whole of this as fifteen properties and fourteen functions. Two of
those functions, `refreshDownloadActivity` and `refreshVisibleDownloads`, had to be called by hand
after every mutation. Both are gone, and so is `downloadDetailOpen`, the gate that suppressed
building the per-file list. The gate existed because the array was rebuilt on every progress tick; a
model does not rebuild, and the two properties that do project a list, `runningDownloads` and the
`question`, are read rather than kept, so nothing is built where nothing is bound.

## Where the boundary falls

**Row identity is the type's own, not the record id.** A Private window's session store refuses
`recordDownload`, so its downloads have no record id at all
([ADR 0035](0035-keep-the-private-browsing-rule-in-the-session-store.md)); keying rows on the record
id would make two of them the same row. The record id and the runtime id are both fields, either may
be empty, and a row carries at least one. A settled row with neither is dropped, because a download
no Space wrote down is not a Download record and cannot become one.

**The hold stays in the engine adapter.** Holding keeps a pointer to a live engine download object,
which cannot cross into core. `QtHeldDownloads` keeps that pointer and mints a token; the queue on
`Downloads` carries only the token, as an opaque string. The queue itself has to be in core rather
than in a per-adapter type because it spans engine profiles: a window with several Spaces has
several profiles, and the reader answers one question at a time across all of them. It must not go
in the session store either, because it is window state that no session outlives. "One at a time" is
now an invariant of the type rather than an early return in the window.

**The engine control path stays in QML, keyed by the download namespace.** `Downloads` emits a
request carrying the runtime id, and the window keeps one table from download namespace to engine
profile and dispatches. The runtime id is already minted as `namespace + ":" + id`, so the namespace
is a routing key that existed and was thrown away by convention. A `DownloadHost` interface in core
holding engine profiles was rejected: an engine profile is a QML object built by the window, core
holds no reference to one today, and putting one behind an interface means core keeping a pointer
into the adapter it was separated from. The table is one line per profile and lives where the
profiles are made.

**The window answers what an origin has been allowed.** Deciding a disposition needs Site
permissions and the reader's interactions, which are the window's. `Downloads` takes a
`DownloadPermissions`, four questions rather than a browser, and `BrowserController` implements it.
This is the seam the boundary really has: the list needs a window to decide a disposition and needs
no window for anything else, and `tests/core/tst_downloads.cpp` says so by standing up a stub for
the list, the queue and the aggregate, and a real controller only for the three disposition tests.

**The disposition crosses as the enum.** `DownloadDisposition` was already a `Q_ENUM` and was being
stringified by `dispositionName`, then spelled again in the real adapter, the mock adapter and the
window, with nothing checking the four agreed. `BrowserController` is now registered as an
uncreatable QML type, the map carries the enum value, and `dispositionName` is gone.

**The Download mark follows the notice.** The glossary rule is that the mark remains until the
saved-file notice closes. That used to hold because one constant, 4200 ms, was handed to the notice
and to the mark's dwell timer. The mark now takes whether the saved-file notice is showing, the
timer is gone, and the constant is an ordinary notice duration again.

## What is not on it

The page-context save-target is not a download. It creates no Download record, no runtime id and no
row: it drives `performPageContextAction` on the tab's engine. Recording the pending request and
opening the file dialog were one function, which is why its test set four private fields to reach
past the API. They are two now, `recordPendingSave` and `requestTargetSave`, and the test calls the
first. Folding the save-target into `Downloads` was rejected for the same reason: nothing it holds
is a download.

## What this narrows

A window's list reads the Space's Download records once, when the window is built, and after that it
is the window's own. Before, opening Settings re-read the store, so a record another window had
written since appeared there. Nothing tells a window that a sibling wrote one, and inventing that
notification is a change of its own; a refresh call on the way into Settings is the thing this ADR
removed. Restarting shows them, as it always did.

See #141.
