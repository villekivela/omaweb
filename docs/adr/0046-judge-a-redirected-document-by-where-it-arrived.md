# Judge a redirected document by where it arrived

The development engine names a document's first party by the address its load set out from. A
document that arrived somewhere else through a cross-site redirect is reported, for every cookie and
storage access it makes, with the first party it left behind: `claude.com/cai/oauth/authorize` for a
document living on `claude.ai`, and `thirdParty` set for the document's own cookies. Only the
navigation response itself is judged against the address it committed at. Two hops report the first
of them. The Qt cookie policy refuses every third party the reader has not allowed, so a site
reached through a redirect could not read or keep its own cookies until the reader navigated inside
it, and a sign-in that returned through one lost its session. See #301.

So the view tells the Qt cookie policy which document it is showing, in two addresses: where the
load set out from and where it arrived. The policy keeps one document per view, and when the engine
reports an access against an address a live document set out from, judges the access by the site
that document arrived at instead. The view says it when the load starts, and again for each redirect
the load resolves to, from the navigation request the engine raises for the redirect. That request
is raised before the destination is fetched, which is the last moment before the arriving document
runs its own scripts; the view's address property moves only once the document has committed, and by
then an inline script has read `document.cookie` and been refused.

The correction lives in the engine adapter and nowhere above it
([ADR 0009](0009-separate-browser-policy-from-engine-mechanics.md)). The wrong first party is an
engine fact, so the core does not learn of it, the engine-view contract does not declare it, and
Site information goes on listing the third parties the policy refused, of which a corrected access
is not one. The view announces the document the way it announces the page load Content blocking
tallies ([ADR 0037](0037-count-refusals-per-page-address.md)): the fact is stated where it is known,
by the view, and kept where it is used, by the policy, with the view's lifetime as the entry's.

The rule is deliberately narrow. The entry is keyed by the exact address the load set out from,
query included, so a page a redirecting address serves of its own, embedding the redirect's
destination as a third party, matches nothing. The access must be for the site the document arrived
at: the same host, or one beneath the other. The engine's own comparison reads the public suffix
list, which Qt keeps private, so two hosts sharing only a registrable domain are not matched and a
document arriving at `www.example.com` still has a cookie set by `api.example.com` refused. That is
a smaller correction than the engine would make for a direct load, and one that can be widened if a
site is found to need it, rather than a wider one that can only be narrowed after it has let
something through.

What was rejected:

- **Correcting from the request interceptor.** `QWebEngineUrlRequestInfo` names the first party of a
  document's requests by the address the document committed at, so the interceptor knows the right
  answer. It cannot hand it over: the cookie filter is a separate callback with nothing that ties an
  access to the request it belongs to, and a `document.cookie` read passes through no interceptor at
  all. The view's announcement covers both.
- **Comparing sites with a public suffix list of Omaweb's own.** The content blocker vendors one
  inside its matcher, behind an interface that answers about requests and not about hosts, and a
  second copy is a second thing to keep current for a rule that the engine's own fix will retire.
- **Waiting for the engine.** The fault is Qt's to fix, and a fixed engine leaves the entries idle:
  a load that arrives where it set out from records nothing, and a correctly named first party never
  matches a set-out address. Nothing here has to be removed when the engine is fixed, and a reader
  on an engine that still carries the fault keeps the correction.
