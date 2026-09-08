# Count refusals per page address

Content blocking owns the Refusal tally. A tally belongs to one page address in one Space, and the
view that loads a document there announces the load so the tally starts again. Pending announcements
are delivered to the outgoing tally before it clears, so a refusal the replaced document earned is
never credited to the document that follows. The view closes its tally when it navigates away or
goes away, so the live tallies are the page loads that are open.

Refusals cannot be attributed to a tab. `QWebEngineUrlRequestInterceptor` receives the request
address, the first-party address, and the resource type, and nothing that names the view the request
came from. Interception attaches to an Engine profile, and a profile belongs to a Space
([ADR 0003](0003-isolate-browsing-identities-by-space.md)), so a profile for each tab is not
available either. The page address is the finest key the engine offers, and the interceptor carries
the Space id the way `QtCookiePolicy` already does.

So two tabs on the same address in the same Space read one tally, and both report what either of
them refused. Two tabs on different pages of one host do not, which is the case a reader sees: the
tally used to be keyed by host, a leftover from the running per-host totals the blocker kept before
[#131](https://github.com/villekivela/omaweb/issues/131), and every tab on a host showed everything
that host refused.

A view tells a load apart from an arrival by carrying the page load it is on. A reload asks for the
same address and is a new document, so the tally starts again; a redirect the load resolved to is
the same load arriving elsewhere, so the tally moves to the address it resolved to; a jump inside
the document is neither, and the fragment is off the key, so the tally carries on. Without the page
load in hand these three are the same call.

The chrome reads a tally for the address it is showing rather than being handed a number. Site
information, Settings and the Space outline each already know that address, so nothing carries the
count through the view, the window, or the outline on its way to being drawn. The engine-view
contract stops declaring a `blockedRequestCount` property, because no adapter exposes one.

What was rejected:

- **Qt's private headers.** They do not reach it either. `QWebEnginePage::setUrlRequestInterceptor`
  attributes per view, but a QML `WebEngineView` owns no page: `QQuickWebEngineViewPrivate` is a
  `WebContentsAdapterClient` beside `QWebEnginePage` rather than beneath it, and the private view
  header exposes the Engine profile and nothing narrower. Per-view interception means reaching into
  `WebContentsAdapter`, and pinning the request path of the one subsystem
  [ADR 0010](0010-own-portable-content-blocking.md) promises across engines to Chromium-adjacent Qt
  internals buys accuracy in the development engine that the target engine cannot inherit.
- **Inferring a new load from the first refusal after the address changes.** It cannot tell a reload
  from a load still in progress, and it never starts the tally for a page that refuses nothing.
- **Leaving the tally in the adapter.** The reset was a line in `EngineView.qml` and the tally an
  integer re-declared in six QML files, none of it reachable from a C++ test. What the tally does
  when a load starts is now asserted where it is decided.

See #142.
