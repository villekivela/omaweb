# Read a page's certificate from the engine

Widens what [0049](0049-ship-omawebs-own-engine-build.md) and
[0050](0050-uncloak-cname-trackers-in-the-engine.md) say the engine patch series is for.

Site information shows the certificate a page arrived over: subject, issuer, validity period,
SHA-256 fingerprint, subject alternative names, and the chain up to the trust anchor. The
certificate interstitial shows the same view for the certificate it refused
([#325](https://github.com/villekivela/omaweb/issues/325)).

## Where the chain comes from

QtWebEngine reports a certificate in one place: a certificate failure carries the chain it was
raised for, in `QWebEngineCertificateError::certificateChain()`. A page whose certificate verified
raises nothing, and Qt offers no call that says what it arrived over.

A patch in the series adds `QWebEngineLoadingInfo::certificateChain()`. When a main-frame navigation
finishes, the engine keeps the chain from the navigation's SSL info, which is the chain Chromium's
verifier built, and hands it over with the load's other facts. For a certificate the application
accepted through a certificate failure, it is the chain that failed. A load that made no TLS
connection carries none. The patch follows the response headers Qt 6.6 added to the same class, so
it is written as Qt's own API rather than Omaweb's, and an application that never reads it sees no
change.

The build reads the patch from the engine's headers, as it does the DNS aliases patch. An Omaweb
built against a stock QtWebEngine still shows the chain a certificate failure named, which covers
the interstitial and a page reached through a Certificate exception in the same load. Everywhere
else Site information says the engine cannot show the certificate, rather than leaving the button
out without a word.

## Why not ask the site again

Omaweb could open a TLS connection of its own to the page's host and read the certificate from it.
That connection is not the engine's. It resolves the host through the system resolver, so under
Secure DNS it sends the name the reader chose to hide from that resolver. It can reach a different
server behind the same name, a load balancer that rotates certificates or a proxy that the engine's
connection went through and this one does not. The certificate it shows would be one the page may
not have arrived over, and a view whose purpose is checking would be the thing needing a check.

## What this costs

One more patch in the series, on Qt's own loading path rather than on copied Chromium code, so a
rebase moves it little. Until the engine that carries it is released, readers see the interstitial's
certificate and not a verified page's.
