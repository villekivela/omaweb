# Keep favicons in the Space that loaded them

Supersedes the part of [0035](0035-keep-the-private-browsing-rule-in-the-session-store.md) that gave
every Private window one shared `PrivateSessionStore` holding nothing but Site permissions.

A tab's favicon used to be only the icon address the engine reported while the page was loaded. A
restored tab, a tab in a new window and an Omnibar row for a site with no open tab showed the host
code, because the address only resolves while a view has the page. Each Space's database now keeps
the icon each page showed, keyed by page address with its origin beside it, and the tab model's icon
role carries an `image://omaweb-favicon/...` address answered from that store until the page reports
its own.

The icon is read through the path the favicon tint already samples one, from the image provider the
engine filled while loading the page. Nothing is fetched: not from a third-party service, not from a
site's `/favicon.ico`, and not from Chromium's own favicon database, which Qt WebEngine does not
expose and which would need an engine patch to read. A site the Space never loaded keeps its host
code.

Stored favicons are browsing data. Deleting History for a visit, an origin or a time range deletes
the icons of those pages, except one a sidebar tab still shows, and an icon whose page has no
History and no tab left is dropped when History is trimmed. Deleting a Space removes them with its
directory.

A Private window keeps its pages' icons in memory, for that window alone, so each Private window is
now given a `PrivateSessionStore` of its own. The Site permissions stay the session's: every one of
those stores holds the same hash, so a decision made in one Private window is still the same
decision in the next and goes when the last one closes. The window's favicons go when the window
does, and its icon addresses name its own store, so it never reads a Space's.

The lookup is asynchronous. The image provider asks from the image loader's thread; the question is
carried to the window's thread and asked of its store, which for a Space answers from the store
thread, so neither the interface nor the image loader waits on the disk. A page with nothing stored
is answered as one transparent pixel rather than as an error, which Qt Quick would log once for
every tab and row that asked, and the tile draws the host code for it.

See #464.
