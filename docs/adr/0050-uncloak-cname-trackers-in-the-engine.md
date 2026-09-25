# Uncloak CNAME trackers in the engine

Supersedes the part of [0026](0026-serve-substitutes-under-an-omaweb-scheme.md), and of
[0010](0010-own-portable-content-blocking.md) before it, that kept CNAME uncloaking outside the
content-blocking contract, and widens what [0049](0049-ship-omawebs-own-engine-build.md) says the
engine patch series is for.

Some sites serve a tracker from a subdomain of their own, `metrics.news.example`, whose DNS CNAME
points at the tracker's real host. A rule for the tracker's host never sees its name, so Content
blocking lets the request through, and the tracker gets the site's first-party cookies besides.
EasyPrivacy carries rules written for exactly these hosts. ADR 0026 left them dead because the
request interceptor never sees DNS. Since ADR 0049 Omaweb builds its own QtWebEngine, so the
interceptor can be taught to.

## What the engine does

A patch in the series gives the request interceptor a second call. On the first call the interceptor
may ask for the request host's DNS aliases. If it asked, and neither blocked nor redirected the
request, the engine resolves the host through the request's own Engine profile, with that profile's
resolver and host cache, and calls the interceptor again with the names in the host's CNAME chain.
There is no second call when the lookup fails or returns no alias other than the host itself, and
the first call's decision stands.

No lookup is made for a main-frame navigation, for an IP literal, or when a proxy applies to the
request. Behind a proxy a local lookup leaks the query and can disagree with the answer the proxy
gets, and Brave skips uncloaking there for the same reason. The lookup warms the cache the request
itself then reads, so each host is resolved once.

The patch is written as a general addition to Qt's interceptor API rather than as Omaweb's, and an
application that never asks sees no change. That keeps a path upstream open and the rebase surface
small: the interception path is Qt's own code, not copied Chromium code.

## What Content blocking does

The request check takes the aliases as data. Chromium keeps a host's aliases in a sorted set, so the
engine cannot say which is canonical, and Content blocking checks each of them. The request is
checked again with its host replaced by each name in turn, keeping the page it came from, its type
and the Space's site toggles, and a block, a Substitute resource or an exception applies as it would
to a direct match. A `$removeparam` rewrite found only through the canonical name is dropped,
because the request goes out under its own address. A canonical name on the same site as the request
is not checked, following uBlock Origin, so a site's own CDN aliases cause no refusal a list did not
intend.

An uncloaked refusal counts in the Refusal tally like any other. Site information now lists the
refused requests beside the tally, and an uncloaked one carries the canonical name it matched, so a
reader can tell why a first-party-looking address was refused.

adblock-rust 0.12.5, the pinned parser, does not know the `$cname` option, so a list's rule that
turns uncloaking off for a host cannot be honoured. It is counted among the unsupported categories,
as CNAME exceptions, rather than accepted and ignored.

## Per engine

CNAME uncloaking is an engine capability, reported per engine as
[0010](0010-own-portable-content-blocking.md)'s contract allows. The Qt engine offers it when it
carries the patch, which Omaweb reads from the engine's headers at build time. Ladybird does not,
and an Omaweb built against a stock QtWebEngine does not either. Settings names only the engines
that lack it.

## What this costs

The series now carries content-blocking work as well as extension work, which ADR 0049 did not count
on. The added cost is one patch on Qt's interception path.

A request no rule refuses waits for a proxy check and a lookup before it starts, and the host cache
does not make that free, so the engine remembers each Engine profile's answers for a minute. A
remembered answer makes no proxy check and no lookup. Only an answer the proxy check let through is
remembered, so a proxy set in that minute cannot be bypassed by a lookup.

Measured on an M2 Max with the patched engine, over loopback, the medians of ten warm loads of a
page of 40 images: with hosts the profile had resolved in the last minute, 58 ms with uncloaking and
57 ms without, and 71 ms against 72 ms when the 40 came from four hosts. With 40 hosts the profile
had never seen, 75 ms against 51 to 55 ms. The comparison turns the site off, so it also leaves out
the first check. A page that reaches many hosts for the first time pays the lookups once, and that
is also the page most likely to carry trackers. Nothing yet holds this to a number before a release
([#371](https://github.com/villekivela/omaweb/issues/371)).
