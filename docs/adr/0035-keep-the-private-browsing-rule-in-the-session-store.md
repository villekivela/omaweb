# Keep the private-browsing rule in the session store

A Private window writes nothing down. Where that rule is enforced is the session store's interface:
`SessionStore` says what a window's session is kept in, `SqliteSessionStore` records it, and
`PrivateSessionStore` accepts every write and drops it. A window is given the adapter its kind of
session needs, so no call site tests whether this window is private before touching the store.

The rule used to be 23 copies of that test in `BrowserController`, one beside each store call, and
the store a Private window held was a SQLite store that had never been opened. A call that forgot
the test would not have written to disk, but it would have run a query against an unopened database,
and nothing asserted that a Private window left its data root untouched.
`tests/core/tst_sessionstore.cpp` now asserts it directly: one conformance suite both adapters run,
where the recording row reopens the root and finds its session and the private row finds the root
still empty.

A private session is shared, so its store is too. `WindowManager` builds one `PrivateSessionStore`
when the first Private window opens and hands the same object to each of them, releasing it with the
temporary profile and the rest of the session when the last one closes
([ADR 0012](0012-isolate-private-browsing-by-window.md)). What that store keeps is the Site
permissions those windows have agreed to, held in memory in the hash the session already passed
round, so a decision made in one Private window is the same decision in the next and none of it
outlives the session.

Whether a window is private stays a separate fact from whether its store records. They coincide
today, and deriving one from the other would read well until an ordinary window wanted a store that
keeps nothing, for a test, and silently became a Private window instead.

One test of the flag remains beside the store, and it is not about storage. Resetting Site
permissions reports success where the store reports nothing stored, because a Private window with
nothing to reset has not failed.

What this does not touch:

- The fourteen places a Private window refuses an action rather than a write, and the one place it
  renders its Space name as "Private": no Spaces, no Pinned tabs, no Keep active, no clearing. That
  is what a Private window is, recorded in [ADR 0012](0012-isolate-private-browsing-by-window.md),
  and naming it once instead of fourteen times is #137.
- Reader configuration. The download directory and the search engines are read by every window and
  changed by none of the private ones, which is a different rule
  ([ADR 0016](0016-separate-user-configuration-from-application-data.md)) and stays visible where it
  is.
- `SessionSiteState`, which holds third-party allowances and certificate exceptions in memory for
  every window already.
- Any answer a test asserts, including a remembered decision in a Private window being spent the
  first time it is read.

See #136.
