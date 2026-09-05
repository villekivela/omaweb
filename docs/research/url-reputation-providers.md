# URL reputation provider research

This note records the provider research completed for #59. It is not an
implementation contract. [ADR 0032](../adr/0032-ship-without-url-reputation.md)
records the current decision to ship without URL reputation.

## Candidates

| Candidate | Service and privacy model | Cost and distribution constraints |
| --- | --- | --- |
| Google Safe Browsing v5, Local List Mode | A client stores SHA-256 prefixes locally and asks for matching full hashes only after a prefix collision. Ordinary local misses send no browsing-derived lookup. [Local List Mode](https://developers.google.com/safe-browsing/reference/Local.List.Mode), [REST API](https://developers.google.com/safe-browsing/reference/rest) | The service is free for non-commercial use. Google requires developer credentials to remain confidential and says they may not be embedded in open-source projects. [Safe Browsing terms](https://developers.google.com/safe-browsing/terms), [Google API terms](https://developers.google.com/terms/#b_confidential_matters) |
| Google Web Risk Update API | A client downloads hash-prefix differences and confirms collisions through `hashes:search`. Google says the Update API does not receive the complete URL. [Update API](https://docs.cloud.google.com/web-risk/docs/update-api), [Web Risk overview](https://docs.cloud.google.com/web-risk/docs/overview) | Full-hash confirmations are billed, and Google recommends keeping API keys out of client code. A public browser would need a credential-holding service. [Pricing](https://cloud.google.com/web-risk/pricing), [API-key practices](https://docs.cloud.google.com/docs/authentication/api-keys-best-practices) |
| PhishTank and URLhaus | PhishTank publishes verified phishing URLs. URLhaus publishes URLs that directly distribute malware. Matching could remain local after each installation downloads both feeds. [PhishTank developer information](https://phishtank.org/developer_info.php), [URLhaus API](https://urlhaus.abuse.ch/api/) | The feeds have different coverage, update contracts, and terms. Their current public documentation does not give Omaweb a clear right to redistribute combined snapshots in its package. [PhishTank terms](https://phishtank.org/terms.php), [URLhaus API terms](https://urlhaus.abuse.ch/api/) |

No candidate fits a public browser that has no service budget and cannot ship a
confidential credential. Safe Browsing is the closest protocol fit, but the
credential rule prevents Omaweb from treating its free tier as permission to
put a shared key in public binaries.

## Browser precedents

- Helium disables Chromium Safe Browsing and relies on its other browser and
  content-blocking protections. [Helium build flags](https://github.com/imputnet/helium/blob/main/flags.gn)
- Zen retains Firefox's Safe Browsing implementation and injects a Google key
  into release builds from a private CI value. That keeps the key out of the
  source repository but does not answer Omaweb's public-binary credential
  constraint. [Zen build configuration](https://github.com/zen-browser/desktop/blob/dev/configs/common/mozconfig), [Zen release script](https://github.com/zen-browser/desktop/blob/dev/.github/workflows/src/release-build.sh)
- Brave sends desktop Safe Browsing traffic through Brave-operated proxies so
  Google does not receive the reader's IP address. That model requires Brave to
  operate and protect a public service. [Brave Safe Browsing](https://support.brave.com/hc/en-us/articles/15222663599629-Safe-Browsing-in-Brave)

These projects demonstrate different product choices. They do not provide a
credential or data licence that Omaweb can inherit.

## Data and operating constraints

Safe Browsing Local List Mode and Web Risk Update API keep normal URL checks on
the device. List updates still reveal the client's IP address, credential, list
selection, and previous list version to the provider. A hash-prefix collision
also sends browsing-derived prefix data for confirmation. Neither design needs
to send a Space identifier, tab identifier, full URL, crash report, or Omaweb
telemetry.

Community feeds can keep every lookup local, but each installation must obtain
fresh data under the feed operator's terms. PhishTank and URLhaus cover
different threat classes and publish no shared production reliability or
false-positive guarantee.

A provider integration would also need to define list freshness, offline
behavior, false-positive correction, one-navigation bypasses, bounded storage,
atomic updates, and the exact automatic requests it makes. Those decisions
depend on the selected provider and should not become architecture commitments
before one is approved.

## Conditions for another evaluation

Evaluate providers again only when at least one of these changes:

- a provider explicitly permits Omaweb's public open-source distribution and
  supplies a suitable public-client credential model;
- Omaweb can fund and operate a narrow credential proxy;
- a community source grants clear runtime and redistribution rights and shows
  coverage suitable for a browser warning.

Before enabling a provider, measure its current list size, update bandwidth,
collision rate, confirmation latency, false-positive correction, and time
without protection. Then define provider-specific fixtures and run the same
navigation contract against the Qt and Ladybird adapters.
