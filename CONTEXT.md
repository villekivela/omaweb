# Tanto

Tanto is a keyboard-driven web browser for developers. Its macOS build must be usable during development. Tanto is intended to become a daily driver on Linux, with first-class Wayland support.

## Language

**Development engine**:
The mature web engine used to build and validate Tanto while the target engine is not ready. QtWebEngine is the current development engine.
_Avoid_: Fallback engine

**Target engine**:
The web engine Tanto ultimately intends to use for everyday browsing. Ladybird is the current target engine.
_Avoid_: Experimental backend

**Agent access**:
Permission for a software agent to inspect and interact with the contents and diagnostics of a specific browser tab. Agent access is granted explicitly, with remembered permission allowed for selected local-development sites.
_Avoid_: DevTools access, remote debugging

**Local-development site**:
A site operated by the user for active software development, usually served from the local machine. A remembered permission may grant agents access to matching local-development sites.
_Avoid_: Trusted site

**Project attachment**:
An optional local project directory attached to a Space. Agent tools use it as the source context for that Space's local-development sites.
_Avoid_: Workspace, project window

**Space**:
A named browsing identity with its own logins, site data, permissions, history, session, and tabs. Switching Spaces does not expose one Space's browsing identity to another.
_Avoid_: Workspace, container, profile

**Pinned tab**:
A tab saved within one Space and restored whenever that Space resumes. Its saved address changes only when the user explicitly updates the pin.
_Avoid_: Bookmark, favorite

**Private window**:
A temporary browser window whose browsing identity is separate from every Space and whose distinct appearance identifies it as private. Private windows share temporary state with each other only while at least one private window remains open.
_Avoid_: Private Space, incognito Space

**Auxiliary window**:
A temporary, minimally framed window requested by a site to complete a flow such as authentication or payment. It inherits the browsing identity of its opener and is not an ordinary Tanto browsing window.
_Avoid_: Browser window, pop-up tab

**Keyboard navigation**:
An optional first-party command layer for controlling Tanto and the active page without a pointer. It provides the same user-facing commands with every engine and lets sites receive selected conflicting keys.
_Avoid_: Vim mode, Vimium extension

**Omnibar**:
A centered overlay for opening addresses, searching, selecting tabs or Spaces, and invoking browser commands. A new-tab request creates its tab only after the user commits a destination in the Omnibar.
_Avoid_: Command bar, omnibox, command palette

**Web extension**:
A third-party browser package that can modify pages or add browser behavior through a supported WebExtensions contract.
_Avoid_: Feature module, plugin

**Feature module**:
An optional Tanto component that adds a first-party product capability without becoming part of the browser core. Account, Sync, and agent diagnostics are candidate Feature modules.
_Avoid_: Extension, plugin

**Account**:
An optional identity used by a Feature module to access remote services. An Account is never required for local browser features.
_Avoid_: Space, browser profile

**Sync**:
An optional Feature module that copies selected non-secret browser state between Tanto installations through a replaceable provider.
_Avoid_: Backup, account

**Diagnostics**:
A deferred Feature module that gives an explicitly authorized user or agent structured access to a tab's runtime failures and page state.
_Avoid_: DevTools, remote debugging

**Site permission**:
A Space-specific decision that allows or blocks an origin from using a protected browser capability. A Private window retains its Site permissions only while the shared private session exists.
_Avoid_: System permission, engine permission

**Content blocking**:
Tanto's built-in removal of unwanted network requests and page elements using subscribed filter lists. Content blocking is a browser capability and does not depend on an installed extension.
_Avoid_: Ad-blocking extension

**Theme palette**:
A complete, versioned set of visual values that defines Tanto's current appearance, including the semantic opacity values for its interface surfaces and the type family and base size they draw with. Tanto can replace the active theme palette while running.
_Avoid_: Theme injection, color scheme

**Transparent surface**:
A Tanto-owned interface region whose theme allows the desktop to remain visible behind it. Webpage viewports are not Transparent surfaces.
_Avoid_: Transparent webpage, click-through region
