# URL reputation provider research

This note records the provider research completed for #59. It is not an implementation contract.
[ADR 0032](../adr/0032-ship-without-url-reputation.md) records the current decision to ship without
URL reputation.

## Candidates

| Candidate                                | Service and privacy model                                                                                                                                                                                                                                                                                                         | Cost and distribution constraints                                                                                                                                                                                                                                                                                             |
| ---------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Google Safe Browsing v5, Local List Mode | A client stores SHA-256 prefixes locally and asks for matching full hashes only after a prefix collision. Ordinary local misses send no browsing-derived lookup. [Local List Mode](https://developers.google.com/safe-browsing/reference/Local.List.Mode), [REST API](https://developers.google.com/safe-browsing/reference/rest) | The service is free for non-commercial use. Google requires developer credentials to remain confidential and says they may not be embedded in open-source projects. [Safe Browsing terms](https://developers.google.com/safe-browsing/terms), [Google API terms](https://developers.google.com/terms/#b_confidential_matters) |
| Google Web Risk Update API               | A client downloads hash-prefix differences and confirms collisions through `hashes:search`. Google says the Update API does not receive the complete URL. [Update API](https://docs.cloud.google.com/web-risk/docs/update-api), [Web Risk overview](https://docs.cloud.google.com/web-risk/docs/overview)                         | Full-hash confirmations are billed, and Google recommends keeping API keys out of client code. A public browser would need a credential-holding service. [Pricing](https://cloud.google.com/web-risk/pricing), [API-key practices](https://docs.cloud.google.com/docs/authentication/api-keys-best-practices)                 |
| PhishTank and URLhaus                    | PhishTank publishes verified phishing URLs. URLhaus publishes URLs that directly distribute malware. Matching could remain local after each installation downloads both feeds. [PhishTank developer information](https://phishtank.org/developer_info.php), [URLhaus API](https://urlhaus.abuse.ch/api/)                          | The feeds have different coverage, update contracts, and terms. Their current public documentation does not give Omaweb a clear right to redistribute combined snapshots in its package. [PhishTank terms](https://phishtank.org/terms.php), [URLhaus API terms](https://urlhaus.abuse.ch/api/)                               |

No candidate fits a public browser that has no service budget and cannot ship a confidential
credential. Safe Browsing is the closest protocol fit, but the credential rule prevents Omaweb from
treating its free tier as permission to put a shared key in public binaries.

## Candidate update contracts

### Safe Browsing v5

Local List Mode downloads the `mw-4b` malware list and `se-4b` social engineering list through
`GET https://safebrowsing.googleapis.com/v5/hashLists:batchGet`. An initial request has no version.
Later requests send the opaque version returned for each list. A response contains a full
replacement or partial update, removals, additions, a checksum, and `minimumWaitDuration`. That
duration controls the next update rather than a client-defined fixed cadence.
[Batch updates](https://developers.google.com/safe-browsing/reference/rest/v5/hashLists/batchGet),
[local database](https://developers.google.com/safe-browsing/reference/Local.Database)

A local prefix collision is confirmed through
`GET https://safebrowsing.googleapis.com/v5/hashes:search`. The response contains matching full
hashes and one cache duration for positive and negative answers. Requests use an API key. Google
permits no more than 1,000 prefixes in one request and says ordinary URL processing should produce
no more than 30.
[Hash search](https://developers.google.com/safe-browsing/reference/rest/v5/hashes/search),
[example requests](https://developers.google.com/safe-browsing/reference#example-requests)

The API accepts `maxDatabaseEntries` and `maxUpdateEntries` limits. A ceiling of 16,777,216
four-byte entries for each of the two lists means at most 128 MiB of raw prefixes. A 256 MiB
steady-state allowance and 512 MiB free-space check would leave room for indices, versions,
checksums, a confirmation cache, and an atomic full replacement. These are research estimates, not
accepted product budgets. The provider does not publish a typical production list size.
[Size constraints](https://developers.google.com/safe-browsing/reference/rest/v5/SizeConstraints)

### Web Risk

The Update API downloads one threat class at a time through
`GET https://webrisk.googleapis.com/v1/threatLists:computeDiff`. The request contains the threat
type, previous opaque version, supported compression, and entry limits. The response contains a
reset or difference, additions, removals, a checksum, a new version, and `recommendedNextDiff`,
which sets the next-update cadence. Prefix collisions are confirmed through
`GET https://webrisk.googleapis.com/v1/hashes:search`.
[Update API](https://docs.cloud.google.com/web-risk/docs/update-api),
[hash search](https://docs.cloud.google.com/web-risk/docs/reference/rest/v1/hashes/search)

Web Risk requires a Google Cloud project with billing, the enabled API, and a credential. Google
recommends a maximum update response of 16,777,216 entries. At the common four-byte prefix length,
two threat classes have the same 128 MiB raw-prefix ceiling and the same provisional 256 MiB
steady-state estimate as Safe Browsing. Confirmation requests are billable.
[Setup](https://docs.cloud.google.com/web-risk/docs/detect-malicious-urls),
[update constraints](https://docs.cloud.google.com/web-risk/docs/update-constraints),
[pricing](https://cloud.google.com/web-risk/pricing)

### Community feeds

PhishTank's smallest documented practical feed is
`https://data.phishtank.com/data/<app-key>/online-valid.json.bz2`. It contains full records for
verified online phishing URLs and updates hourly. Automated clients should use an application key.
URLhaus documents authenticated CSV and JSON exports generated every five minutes and asks clients
not to fetch more often. Neither provider publishes a maximum feed size, so no bounded storage
estimate can be derived from their contracts.
[PhishTank feeds](https://phishtank.org/developer_info.php),
[URLhaus feeds](https://urlhaus.abuse.ch/api/)

## Browser precedents

- Helium disables Chromium Safe Browsing and relies on its other browser and content-blocking
  protections. [Helium build flags](https://github.com/imputnet/helium/blob/main/flags.gn)
- Zen retains Firefox's Safe Browsing implementation and injects a Google key into release builds
  from a private CI value. That keeps the key out of the source repository but does not answer
  Omaweb's public-binary credential constraint.
  [Zen build configuration](https://github.com/zen-browser/desktop/blob/dev/configs/common/mozconfig),
  [Zen release script](https://github.com/zen-browser/desktop/blob/dev/.github/workflows/src/release-build.sh)
- Brave sends desktop Safe Browsing traffic through Brave-operated proxies so Google does not
  receive the reader's IP address. That model requires Brave to operate and protect a public
  service.
  [Brave Safe Browsing](https://support.brave.com/hc/en-us/articles/15222663599629-Safe-Browsing-in-Brave)

These projects demonstrate different product choices. They do not provide a credential or data
licence that Omaweb can inherit.

## Data leaving the device

| Event               | Safe Browsing v5 Local List Mode                                           | Web Risk Update API                                                                       | Community feeds                                           |
| ------------------- | -------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------- | --------------------------------------------------------- |
| Update              | Source IP, API key, list names, previous opaque versions, and entry limits | Source IP, Cloud credential, threat type, previous version, compression, and entry limits | Source IP, feed-specific key, and request headers         |
| Ordinary local miss | Nothing                                                                    | Nothing                                                                                   | Nothing                                                   |
| Prefix collision    | One or more SHA-256 prefixes, API key, and connection metadata             | One SHA-256 prefix, threat classes, credential, and connection metadata                   | Nothing, because the feed contains complete provider URLs |
| Reporting           | No automatic reporting is required                                         | No automatic reporting is required                                                        | No submission or voting is required                       |

None of these contracts needs a Space identifier, tab identifier, complete URL for a prefix-based
check, crash report, or Omaweb telemetry. Prefixes still come from browsing activity and reveal a
small candidate set. The provider also sees the source IP unless Omaweb operates a proxy.

## False positives, bypasses, stale data, and offline use

No candidate publishes a browser-relevant false-positive rate or a guaranteed correction time.
Google says both false positives and false negatives occur. PhishTank relies on community
verification, and URLhaus accepts correction reports. Without working credentials and a
representative browsing corpus, Omaweb cannot measure collision rate, confirmation latency, or
correction time. Those unknowns count against selecting a provider rather than becoming assumed
zeroes.

If Omaweb later adds a provider, the narrow bypass researched for #59 applies to one exact canonical
URL and one navigation. It is not stored, shared with another tab, reused for a redirect, or carried
into a Private window. This is a candidate policy, not part of the current browser contract.

Safe Browsing requires applicable provider data received within the preceding 30 minutes before a
client treats an address as unsafe. Web Risk full-hash matches expire at provider-supplied times.
With no fresh confirmed match, offline operation must fail open. A local miss may proceed, while a
prefix collision without a usable confirmation cannot become a warning.
[Safe Browsing terms](https://developers.google.com/safe-browsing/terms),
[Web Risk warning rules](https://docs.cloud.google.com/web-risk/docs/user-warnings)

## Linux packaging

The native Arch package could contain an engine-neutral client and synthetic test data, but no
provider snapshot or production credential. Each installation would download current data into the
application-data directory at runtime. The data must not enter Sync, backups, a Space, or an engine
profile.

Google's credential terms rule out embedding a shared API key in the package. A user-supplied Cloud
key would turn default protection into setup work and may expose the reader to provider billing. A
provider-approved public-client key or an Omaweb-operated credential proxy would be required. The
community feeds do not remove the packaging problem because their current redistribution rights and
maximum sizes remain unclear.

## Deterministic fixtures

A future provider implementation should use synthetic `.test` data and a fake clock. No test should
call a live service or contain a provider snapshot. The research contract defines these fixture
groups:

- canonicalization cases with expected URL expressions, SHA-256 hashes, and prefixes;
- full-reset, partial-update, and bad-checksum responses that test atomic list replacement and
  recovery;
- positive, collision-only, expired, and transport-error confirmations;
- safe, malware, phishing, stale, offline, bypassed, redirected, local, non-HTTP, and subresource
  navigations;
- recorded network requests that reject complete URLs, full hashes, Space and tab identifiers, and
  telemetry.

The same navigation cases must run against the core fake transport, Qt adapter, and Ladybird
adapter. Provider-specific parsing and storage stay outside the engine adapters. The fixtures become
binding only if a later decision approves a provider and its exact protocol.

## Conditions for another evaluation

Evaluate providers again only when at least one of these changes:

- a provider explicitly permits Omaweb's public open-source distribution and supplies a suitable
  public-client credential model;
- Omaweb can fund and operate a narrow credential proxy;
- a community source grants clear runtime and redistribution rights and shows coverage suitable for
  a browser warning.

Before enabling a provider, measure its current list size, update bandwidth, collision rate,
confirmation latency, false-positive correction, and time without protection. Then define
provider-specific fixtures and run the same navigation contract against the Qt and Ladybird
adapters.
