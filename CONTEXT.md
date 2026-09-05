# Omaweb

Omaweb is a keyboard-driven web browser for developers. Its macOS build must be usable during development. Omaweb is intended to become a daily driver on Linux, with first-class Wayland support.

## Language

**Development engine**:
The mature web engine used to build and validate Omaweb while the target engine is not ready. QtWebEngine is the current development engine.
_Avoid_: Fallback engine

**Target engine**:
The web engine Omaweb ultimately intends to use for everyday browsing. Ladybird is the current target engine.
_Avoid_: Experimental backend

**Local-development site**:
A site operated by the reader for software development, reached through localhost, an IP literal, or a reserved local-development domain. Omaweb may give it narrowly scoped behavior that public sites never receive, such as a one-time overridable certificate exception.
_Avoid_: Trusted site, Project site

**Space**:
A named browsing identity with its own logins, site data, permissions, history, session, and tabs. Switching Spaces does not expose one Space's browsing identity to another.
_Avoid_: Workspace, container, profile

**Pinned tab**:
A tab saved within one Space and restored whenever that Space resumes. Its saved address changes only when the user explicitly updates the pin.
_Avoid_: Bookmark, favorite

**Keep active**:
A Pinned-tab setting that lets its page continue running while another Space is active. The setting survives restart, and Omaweb always identifies the retained tab and its resource use.
_Avoid_: Background Space, never suspend

**Space at rest**:
A Space with nothing open in it, because nothing has been opened yet or the last page has been closed. Its only ordinary tab is blank. A Space at rest lists no ordinary tab and shows the Start page in place of a webpage; its Pinned tabs are unaffected.
_Avoid_: Empty tab, blank tab, new tab

**Start page**:
What Omaweb draws where a webpage would be whenever the tab on show has no address to load — a Space at rest, or `about:blank` itself: the browser's keyboard commands and the keys that run them, rather than a document loaded from anywhere. It costs no engine and takes the sidebar's translucency. The same sheet answers on demand over a live page, where it is opaque and closeable instead.
_Avoid_: Home page, new tab page, about:blank, cheat sheet

**Private window**:
A temporary browser window whose browsing identity is separate from every Space and whose distinct appearance identifies it as private. Private windows share temporary state with each other only while at least one private window remains open.
_Avoid_: Private Space, incognito Space

**Private accent**:
The one colour a theme names for its Private windows. Omaweb derives the grounds it is cast over — the Private window, sidebar, surface and hover fill — by tinting each of the theme's own surfaces towards it, so a theme identifies private browsing by naming a hue rather than by naming a palette, and a Private window keeps the darkness the theme drew its chrome in. A theme that names a ground itself keeps it.
_Avoid_: Private colour, incognito accent

**Auxiliary window**:
A temporary, minimally framed window requested by a site to complete a flow such as authentication or payment. It inherits the browsing identity of its opener and is not an ordinary Omaweb browsing window.
_Avoid_: Browser window, pop-up tab

**Keyboard navigation**:
An optional first-party command layer for controlling Omaweb and the active page without a pointer. It provides the same user-facing commands with every engine and lets sites receive selected conflicting keys.
_Avoid_: Vim mode, Vimium extension

**Omnibar**:
A centered overlay for opening addresses, searching, selecting tabs or Spaces, and invoking browser commands. A new-tab request creates its tab only after the user commits a destination in the Omnibar.
_Avoid_: Command bar, omnibox, command palette

**Developer tools**:
The inspector supplied by the current web engine and attached to one tab. Omaweb opens it, positions it, and draws it in the browser's own theme, but does not normalize its interface or debugging protocol across engines.
_Avoid_: Diagnostics, Agent access, DevTools platform

**Web extension**:
A third-party browser package that can modify pages or add browser behavior through a supported WebExtensions contract.
_Avoid_: Feature module, plugin

**Feature module**:
An optional Omaweb component that adds a first-party product capability without becoming part of the browser core. Account and Sync are Feature modules.
_Avoid_: Extension, plugin

**Account**:
An optional identity used by a Feature module to access remote services. An Account is never required for local browser features.
_Avoid_: Space, browser profile

**Sync**:
An optional Feature module that copies selected non-secret browser state between Omaweb installations through a replaceable provider.
_Avoid_: Backup, account

**Site permission**:
A Space-specific decision that allows or blocks an origin from using a protected browser capability. A Private window retains its Site permissions only while the shared private session exists.
_Avoid_: System permission, engine permission

**Browsing data**:
What a Space accumulates as it is used and can be cleared without unmaking the Space: its cookies, site storage, cache, Site permissions and history. Browsing data is cleared by category and time range, and clearing it leaves the Space itself, its Pinned tabs, its name and colour, and everything the reader configured about the browser. Deleting a Space is the separate act that removes the Space as well.
_Avoid_: History, cache, site data, user data

**Site information**:
What Omaweb states about the site on show, for the Space it is on show in: its origin, the connection the engine reports, the requests Content blocking refused, the site data the Space holds, the origin's Site permissions, and the confirmed ways to clear that data or reset those decisions. Reached from the address trigger, which reports the connection from the engine's own facts and never from the address itself.
_Avoid_: Site status, page info, security panel

**Certificate exception**:
A reader's decision to let one blocked certificate failure through. Omaweb offers it only for an engine-overridable, non-fatal failure in a Local-development site's own main frame, and writes nothing down, so a later session asks again. The engine holds the exception for the rest of the session and cannot be made to forget it, so Site information and the address trigger keep reporting the connection as in error for as long as it stands.
_Avoid_: Trusted certificate, security exception, certificate override

**Third-party cookie allowance**:
A temporary permission for one origin to keep cookies and site storage while embedded in another site, granted for a named authentication or payment flow inside one Space. It is held in memory only, listed in Site information, and revocable there, so it cannot cross a Space or outlive the session that granted it.
_Avoid_: Cookie exception, tracking allowance

**High-risk download**:
A file a browser would be handing over to be run, installed or mounted rather than read: an executable, a script, an installer, a disk image, or an archive that may carry any of them. Omaweb names the kind to the reader before writing one down, never opens one afterwards, and takes its execute bits off.
_Avoid_: Dangerous file, malware, unsafe download

**Held download**:
A download Omaweb has taken off the engine while a question about it stands. Nothing has been written for it, because the engine decides a download's fate inside one synchronous handler and cannot keep one waiting: the request is cancelled and the page is asked for the same file again once the reader answers.
_Avoid_: Paused download, pending download, blocked download

**Download mark**:
The outline footer's one control for every download in flight: how many there are, how far through the whole of them the bytes have got, and, when the reader asks it, which file is which. It is derived from the downloads the window is running rather than from a Space's records, so it works in a Private window, which records none. It appears when a download starts, holds a finished state for as long as the notice naming the saved file stands, and then leaves — the footer says nothing about downloads on the days there are none. It opens the downloads section it is the summary of.
_Avoid_: Download badge, progress bar, download indicator, download tray

**Engine security baseline**:
The QtWebEngine version a supported Omaweb build runs on, and the Chromium release whose security fixes that engine carries. One file names both; the build reads it to say whether it meets it, and CI reads it weekly to say whether it has gone stale. A build below the baseline is an unsupported preview and says so.
_Avoid_: Minimum Qt version, supported engine

**Content blocking**:
Omaweb's built-in removal of unwanted network requests and page elements using subscribed filter lists. Content blocking is a browser capability and does not depend on an installed extension.
_Avoid_: Ad-blocking extension

**URL reputation**:
An optional browser capability that checks an address or download against a provider's current phishing, malware, or software-reputation data. Content blocking may refuse some known malicious addresses but is not URL reputation. URL reputation is not part of Omaweb's daily-driver contract.
_Avoid_: Safe Browsing, malicious-site blocking

**Substitute resource**:
A small stand-in body from the vendored uBlock Origin library that Omaweb serves in place of a request a filter list's rule names with `$redirect`. A filter list names a Substitute resource and never supplies its body.
_Avoid_: Redirect resource, stub, mock response

**Scriptlet**:
A function from the vendored uBlock Origin library that a filter list's rule names and supplies arguments for, run in a page before the page's own scripts. A filter list names a Scriptlet and never supplies its code.
_Avoid_: Injected script, user script

**Site fullscreen**:
A page holding the whole screen because the site asked for it. It begins with a notice naming the origin and always ends with `Escape`. It is separate from the fullscreen the reader asks Omaweb for, and handing one back never takes the other away.
_Avoid_: Fullscreen mode, video fullscreen

**Theme palette**:
A complete, versioned set of visual values that defines Omaweb's current appearance, including the semantic opacity values for its interface surfaces and the type family and base size they draw with. Omaweb can replace the active theme palette while running.
_Avoid_: Theme injection, color scheme

**Transparent surface**:
A Omaweb-owned interface region whose theme allows the desktop to remain visible behind it. Webpage viewports are not Transparent surfaces.
_Avoid_: Transparent webpage, click-through region
