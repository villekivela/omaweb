# Serve substitutes under a Tanto scheme

Supersedes the part of [0010](0010-own-portable-content-blocking.md) that placed
redirects and resource replacement outside the first content-blocking contract, and
the part of [0025](0025-run-only-vendored-scriptlets.md) that vendored uBlock
Origin's scriptlets alone. Response rewriting, HTML filtering, CNAME uncloaking and
the dynamic rules stay outside it, and `$csp` joins them.

Blocking a request outright is the wrong answer for some of what a filter list
refuses. A page that waits on `analytics.js` and is handed nothing waits forever:
the tracker is gone and so is the article. This is what a list's `$redirect=` rules
are for — they name a small stand-in body that defines the symbols the page expects
and does nothing else, so the page finishes initialising with the tracker replaced
rather than missing. Twenty-seven such rules ship in EasyList and EasyPrivacy today,
plus eight more written as `$rewrite=abp-resource:…`, and until now Tanto discarded
all thirty-five and blocked what they named.

A rule names a substitute. It does not supply one, for the same reason a scriptlet
rule may not supply code ([0025](0025-run-only-vendored-scriptlets.md)). The bodies
come from uBlock Origin's `web_accessible_resources`, vendored beside the scriptlets
under `third_party/ubo-scriptlets` and pinned by the same manifest, and the set a
rule may name is the map upstream keeps in `redirect-resources.js` rather than the
directory listing — the directory also holds uBO's own extension pages. A rule
naming a body this build does not carry is counted unsupported rather than accepted,
because a rule that blocks and then serves nothing leaves exactly the half-loaded
page substitutes exist to prevent.

`adblock-rust` hands a matched body over as a `data:` URL, and Chromium refuses to
redirect a request to one. So Tanto registers `tanto-resource:` and serves the body
under the name the library knows it by — `tanto-resource:noop.js` rather than twenty
kilobytes of base64 — which keeps a replaced request legible in a network log as the
resource that replaced it. Reading the name back out of the body is a lookup built
once for the process, because the engine reports which body it chose and never which
rule chose it. The scheme is registered secure, CORS-enabled, fetch-allowed and
exempt from the page's own content policy: a substitute has to be as loadable as the
request it stands in for, or the refusal it was written to soften becomes a second
failure.

A redirect does not imply a block. `$redirect-rule` names a substitute that stands
in only once some separate blocking rule has matched, and `$removeparam` rewrites
the address of a request that is still going out. One boolean cannot carry that, so
the C interface reports the three answers separately and drops each one where it
would mean nothing, rather than leaving a caller to remember which combinations are
real. `$removeparam` falls out of the same mechanism — a redirect to the address
with the parameters stripped — and applies to documents, frames and XHR, which is
the set of requests the option's own default names.

Both answers are redirects, and Chromium drops a redirect on a request carrying a
payload. So a substitute cannot stand in for a POST, and the request is refused
outright instead, which is what Tanto did with it before; and a `$removeparam` rule
cannot strip a POST's parameters, which is a limit uBlock Origin shares. Every rule
in the lists Tanto ships names a resource a page fetches rather than posts to.

Two things stay out of contract, and both for the same reason: a
`QWebEngineUrlRequestInterceptor` sees a request and never a response. `$replace`
rewrites a response body and `$csp` adds a response header, and neither is reachable
from where Tanto decides. CNAME uncloaking needs DNS the interceptor never sees.
`$replace` is zero rules in the lists Tanto ships and `$csp` is five, four of which
add a policy and one of which takes one back, so the cost is a reported category
rather than blocking that quietly does nothing. They can be
reconsidered when the Ladybird adapter can offer response access.
