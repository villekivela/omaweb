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

**Site information**:
What Omaweb states about the site on show, for the Space it is on show in: its origin, the connection the engine reports, the requests Content blocking refused, the site data the Space holds, the origin's Site permissions, and the confirmed ways to clear that data or reset those decisions. Reached from the address trigger, which reports the connection from the engine's own facts and never from the address itself.
_Avoid_: Site status, page info, security panel

**Certificate exception**:
A reader's decision to let one blocked certificate failure through. Omaweb offers it only for an engine-overridable, non-fatal failure in a Local-development site's own main frame, and writes nothing down, so a later session asks again. The engine holds the exception for the rest of the session and cannot be made to forget it, so Site information and the address trigger keep reporting the connection as in error for as long as it stands.
_Avoid_: Trusted certificate, security exception, certificate override

**Third-party cookie allowance**:
A temporary permission for one origin to keep cookies and site storage while embedded in another site, granted for a named authentication or payment flow inside one Space. It is held in memory only, listed in Site information, and revocable there, so it cannot cross a Space or outlive the session that granted it.
_Avoid_: Cookie exception, tracking allowance

**Content blocking**:
Omaweb's built-in removal of unwanted network requests and page elements using subscribed filter lists. Content blocking is a browser capability and does not depend on an installed extension.
_Avoid_: Ad-blocking extension

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
