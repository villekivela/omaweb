# Defer a URL reputation provider

See #59.

Omaweb does not yet approve a URL reputation provider. Public daily-driver
status remains blocked. Google Safe Browsing v5 in Local List Mode is the
closest technical fit, but Omaweb must not ship it until Google confirms in
writing that the public, free Linux package is a permitted non-commercial API
client and agrees on a credential model suitable for software whose source and
binary are public. Google Web Risk is the fallback if Omaweb can fund it and
operate a credential-holding service that never receives full URLs.

An early alpha continues to say that reputation protection is unavailable, as
required by [Product requirements](../product/requirements.md#privacy-and-security).
This decision records the contract to implement once the provider and
credentials are settled, so the engine-neutral work need not be rediscovered.

## Provider comparison

| Candidate | Coverage and update contract | Licence and redistribution | Result |
| --- | --- | --- | --- |
| Google Safe Browsing v5, Local List Mode | Google's lists cover malware and social engineering, including phishing. A client keeps SHA-256 prefixes locally and asks for full hashes only after a prefix collision. The v5 REST service exposes `hashLists:batchGet` and `hashes:search`. [Threat types](https://developers.google.com/safe-browsing/reference/rest/v5/hashList#ThreatType), [Local List Mode](https://developers.google.com/safe-browsing/reference/Local.List.Mode), [REST methods](https://developers.google.com/safe-browsing/reference/rest) | Free, but only for non-commercial use unless Google makes a separate agreement. The general API terms also forbid permanent copies, overlong caches, and redistribution of returned content unless permission applies. [Safe Browsing terms](https://developers.google.com/safe-browsing/terms), [Google API terms, section 5](https://developers.google.com/terms/#5_content) | Best protocol fit, not approved until use and credential terms are confirmed. |
| Google Web Risk Update API | The required classes are available as `MALWARE` and `SOCIAL_ENGINEERING`. A client downloads variable-length SHA-256 prefixes with `threatLists:computeDiff`, then confirms collisions through `hashes:search`. [Web Risk overview](https://docs.cloud.google.com/web-risk/docs/overview), [Update API](https://docs.cloud.google.com/web-risk/docs/update-api) | Google says Web Risk information must not be redistributed. Diff calls are free, but every hash confirmation costs USD 50 per 1,000 calls at the first published tier. [Web Risk overview](https://docs.cloud.google.com/web-risk/docs/overview), [pricing](https://cloud.google.com/web-risk/pricing) | Technically viable only with a commercial agreement, a budget, and a credential service. |
| PhishTank plus URLhaus community feeds | PhishTank publishes verified, online phishing URLs in XML, CSV, JSON, and PHP formats each hour. URLhaus publishes full-URL malware feeds and five-minute CSV/JSON dumps, but it accepts only URLs that directly distribute malware. Neither feed alone covers both release-gate classes. [PhishTank developer information](https://phishtank.org/developer_info.php), [URLhaus API](https://urlhaus.abuse.ch/api/) | PhishTank now points users to the Cisco EULA; its older page says API data was free for commercial use under CC BY-SA 2.5, but labels that text archived. URLhaus offers its community API under fair-use terms and says commercial or for-profit users may need a paid subscription. Neither current source grants an unambiguous right to redistribute a snapshot inside Omaweb packages. [PhishTank terms](https://phishtank.org/terms.php), [URLhaus API terms](https://urlhaus.abuse.ch/api/) | Rejected as the release-gate provider. The pair has a coverage seam, carries full URLs, and leaves current redistribution rights and production reliability unsettled. |

The Safe Browsing and Web Risk restrictions concern provider data, not the
MPL-2.0 code that implements the client. The Linux package may contain that
client and deterministic synthetic fixtures. It must not contain a provider
snapshot or a production credential.

Neither community provider publishes a maximum feed size, so its storage and
download budget cannot be bounded from the contract. PhishTank's feed contains
one record per verified online phish, and URLhaus's dump contains full URLs and
metadata rather than fixed-width prefixes. This is a material operations gap,
not a number Omaweb should fill in from a one-day sample.

PhishTank's smallest documented practical feed is
`https://data.phishtank.com/data/<app-key>/online-valid.json.bz2`. It is a full
JSON record set, updated on the hour. Automated downloads should use an
application key; keyed clients may make unlimited `HEAD` requests for the
`ETag`, while unkeyed clients get only a few downloads each day. The URLhaus
v2 example is
`https://urlhaus-api.abuse.ch/v2/files/exports/<auth-key>/recent.csv`.
URLhaus requires an auth key, generates its CSV and JSON dumps every five
minutes, and asks clients not to fetch more often. Its unauthenticated
documentation hides the exact full and active dump paths behind login, which
is another contract gap. [PhishTank feed contract](https://phishtank.org/developer_info.php), [URLhaus feed contract](https://urlhaus.abuse.ch/api/)

## Candidate update contracts

### Safe Browsing v5

Use Local List Mode, never `urls.search`. On first use, fetch `mw-4b` for
malware and `se-4b` for social engineering with
`GET https://safebrowsing.googleapis.com/v5/hashLists:batchGet`. Later
requests send the opaque `version` returned for each list. The response
supplies either a full replacement or a partial update, Rice-delta-encoded
removals and additions, a checksum of the sorted resulting list, and a
`minimumWaitDuration`. Additions may be 4, 8, 16, or 32 bytes, although four
bytes is the common prefix length. The client follows the server-provided wait
rather than inventing a fixed poll period. [Batch update method](https://developers.google.com/safe-browsing/reference/rest/v5/hashLists/batchGet), [hash-list representation](https://developers.google.com/safe-browsing/reference/rest/v5/hashList), [local database and list names](https://developers.google.com/safe-browsing/reference/Local.Database)

For a local prefix collision, send the canonical URL expressions' four-byte
prefixes to `GET https://safebrowsing.googleapis.com/v5/hashes:search`. The
request accepts no more than 1,000 prefixes and Google says normal URL
processing should produce no more than 30. It returns matching 32-byte hashes
and one cache duration, which applies to positive and negative answers and may
not be extended beyond 24 hours. [Hash search](https://developers.google.com/safe-browsing/reference/rest/v5/hashes/search)

The v5 request examples send an API key. Under Google's general API terms,
developer credentials identify the API client, must remain confidential, and
may not be embedded in an open-source project. [Safe Browsing v5 examples](https://developers.google.com/safe-browsing/reference#example-requests), [Google API terms, section 4b](https://developers.google.com/terms/#b_confidential_matters)

Set `maxDatabaseEntries` and `maxUpdateEntries` to 16,777,216 for each list.
At four bytes per entry, the two required lists can occupy at most 128 MiB of
raw prefixes. Cap steady-state storage at 256 MiB for prefixes, opaque versions,
checksums, indices, and the confirmation cache. Require 512 MiB of free space
before a full reset so the old and new databases can coexist until an atomic
swap. Do not duplicate the full lists in the application heap; memory-map the
sorted prefixes and cap cached confirmations at 16 MiB. If a valid update
cannot fit those limits,
keep the previous valid database, report protection unavailable, and measure
the rejected size before changing the budget. Google defines both constraints
as entry counts and permits the server to return fewer entries.
[Size constraints](https://developers.google.com/safe-browsing/reference/rest/v5/SizeConstraints)

### Web Risk

Fetch one class at a time with
`GET https://webrisk.googleapis.com/v1/threatLists:computeDiff`. The request
contains the threat type, the last opaque version token, supported compression,
and maximum update and database entry counts. The response is `RESET` or
`DIFF`, with additions, removals, a new token, a SHA-256 checksum, and
`recommendedNextDiff`. An empty token requests a full snapshot. Raw and Rice
compression are supported. Google recommends an update-response ceiling of
16,777,216 entries when no other requirement applies. At the typical four bytes
per prefix, that is 64 MiB per list, or 128 MiB for the required malware and
social-engineering lists before database overhead. Use the same 256 MiB
steady-state disk cap, 512 MiB free-space requirement for a full reset, and
16 MiB confirmation-cache limit as Safe Browsing. Set the separate
`maxDatabaseEntries` to 16,777,216 per list as well.
[Update API](https://docs.cloud.google.com/web-risk/docs/update-api), [integration choices](https://docs.cloud.google.com/web-risk/docs/detect-malicious-urls), [update constraints](https://docs.cloud.google.com/web-risk/docs/update-constraints)

Confirm a collision with
`GET https://webrisk.googleapis.com/v1/hashes:search`, passing a 4-to-32-byte
SHA-256 prefix and the requested threat types. The response contains matching
32-byte hashes, positive expiry times, and a negative expiry time. Do not keep
either verdict after its expiry. [Hash search](https://docs.cloud.google.com/web-risk/docs/reference/rest/v1/hashes/search)

The initial update must be jittered across the first minute after start or
wake. Later requests must respect the service's wait or backoff. Web Risk setup
requires a Cloud project, billing, the enabled API, and an API key. Google tells
developers not to put API keys in client code and recommends that a server add
the credential. [Request frequency](https://docs.cloud.google.com/web-risk/docs/request-frequency), [setup](https://docs.cloud.google.com/web-risk/docs/detect-malicious-urls), [API-key practices](https://docs.cloud.google.com/docs/authentication/api-keys-best-practices)

## Data leaving the device

| Event | Safe Browsing v5 Local List Mode | Web Risk Update API | Community feeds |
| --- | --- | --- | --- |
| Update | TLS connection metadata, including the source IP visible to the endpoint; API key; list names; opaque prior versions; storage and update constraints | TLS connection metadata, including the source IP visible to the endpoint; API key or Cloud authentication; threat type; opaque prior version; compression and size constraints | TLS connection metadata; feed-specific application or auth key; request headers. PhishTank asks for a descriptive User-Agent and records an API user's key, action, parameters, and IP address. [PhishTank privacy](https://phishtank.org/privacy.php) |
| Ordinary safe navigation | Nothing after the local lookup | Nothing after the local lookup | Nothing after the local lookup |
| Prefix collision | One or more four-byte SHA-256 prefixes, API key, and connection metadata. No full URL or full hash leaves the client. This still reveals a small candidate set, so it is browsing-derived data. | One 4-to-32-byte SHA-256 prefix, threat types, credential, and connection metadata. Google says the Update API never receives the actual URL. [Web Risk overview](https://docs.cloud.google.com/web-risk/docs/overview) | Nothing. The feeds contain full provider URLs, and matching stays local. |
| Optional reporting | None. Omaweb does not submit a URL or telemetry. | None. Omaweb does not use the Submission API, `uris.search`, or `evaluateUri`. | None. Omaweb does not submit URLs or votes. |

Safe Browsing's optional Oblivious HTTP path can hide the client's IP from the
Google gateway and hide hash prefixes from the relay, but it requires a relay
authenticated with OAuth and a public-key refresh each day. That replaces one
provider dependency with a relay service and is not part of the first contract.
[Safe Browsing OHTTP](https://developers.google.com/safe-browsing/ohttp/reference)

No Space identifier, tab identifier, complete URL, full hash, stable device
identifier, crash report, or Omaweb telemetry may be added to any request. The
production implementation must add its destinations and triggers to
[Automatic network requests](../network-requests.md) before release.

## Verdict, false positives, and offline policy

Only top-level HTTP and HTTPS navigation is checked. Check a redirect target as
a new navigation. Do not check subresources, WebSockets, local files,
browser-owned pages, or Local-development sites. Google expressly scopes
Safe Browsing to URLs shown in the address bar rather than subresources.
[Appropriate usage](https://developers.google.com/safe-browsing/reference/Appropriate.Usage)

A local prefix match is not a threat verdict. It causes a full-hash
confirmation. A collision that has no full-hash match proceeds normally and is
cached as a negative result until the provider's expiry. If confirmation fails,
Safe Browsing's Local List procedure returns safe. Omaweb therefore fails open,
marks reputation protection unavailable for that navigation, and never turns a
four-byte collision into a warning. [Local List procedure](https://developers.google.com/safe-browsing/reference/Local.List.Mode)

A confirmed malware or social-engineering match replaces the page with a
browser-owned warning. The warning uses qualified language, names the threat,
attributes Google only when Google supplied the verdict, links the provider's
advisory and false-positive form, and states that false positives and false
negatives occur. Google requires those qualifications, attribution, and user
notice. Google and both community services publish false-positive reporting
paths. [Safe Browsing warning rules](https://developers.google.com/safe-browsing/reference/Appropriate.Usage), [Google correction path](https://developers.google.com/safe-browsing/v4/advisory), [PhishTank correction path](https://phishtank.org/faq.php), [URLhaus correction path](https://www.spamhaus.com/faqs/)

The provider documents support these measurements without production
credentials:

| Property | Measured result for the proposed contract |
| --- | --- |
| False positives | No candidate publishes a browser-relevant false-positive rate or a correction-time guarantee. Google states that both false positives and false negatives occur. PhishTank requires community verification, and URLhaus accepts false-positive reports in its web interface. The measurable rate and correction time therefore remain unknown, which blocks release qualification rather than becoming an assumed zero. |
| Bypass | One exact canonical URL, one navigation, and zero bytes of durable bypass state. A reload, redirect, other tab, or Private window gets no reuse. |
| Safe Browsing staleness | A verdict becomes unusable 30 minutes after the last applicable Google update, even when a longer local cache duration remains. |
| Web Risk staleness | A confirmed full-hash verdict becomes unusable at its returned expiry time. |
| Offline behavior | Zero requests leave the machine. A cached full-hash verdict may warn only while its provider freshness rule still permits it. A local miss proceeds. A prefix collision without a usable cached confirmation fails open and marks protection unavailable for that navigation. |

These are contract measurements, not accuracy benchmarks. Measuring collision
rate, correction time, bandwidth, and time without protection needs working
credentials and a representative corpus; those observations remain explicit
conditions for provider approval.

The warning offers `Go back` as the primary action and `Proceed once` behind a
second explicit action. A bypass belongs to the exact canonical URL and current
navigation only. It is not written to disk, reused for redirects, shared with
another tab, or carried to a Private window. Reloading or following a redirect
checks again. This is Omaweb policy, not a provider claim.

Provider expiry wins over local availability. Safe Browsing's terms forbid
treating a listed URL as unsafe unless the application received updated
information in the preceding 30 minutes. Web Risk permits a warning only while
the confirming full-hash response is unexpired. After that point, offline or
failed-refresh operation is fail-open and visibly unprotected, not silently
based on stale data. [Safe Browsing terms](https://developers.google.com/safe-browsing/terms), [Web Risk warning rules](https://docs.cloud.google.com/web-risk/docs/user-warnings)

This stale policy is one reason a public daily-driver release remains blocked.
The product can still load pages offline, but it cannot honestly describe that
session as protected when provider terms prohibit acting on the stored verdict.

## Linux packaging

The native Arch package will use the same Omaweb-owned C++ contract as every
engine build. The protocol needs HTTPS, hashing, a clock, and durable storage,
all of which the existing Qt/core build can provide; no provider SDK or daemon
belongs in the package. Store list state and the confirmation cache under the
application-data root, never the configuration root, Sync, backups, or an
engine-profile directory assigned to a Space. One shared database serves
ordinary and Private windows without recording which one caused a lookup.

The package must not embed an API key. Google's API terms say developer
credentials identify the API client, must remain confidential, and may not be
embedded in an open-source project. Its security guidance also says client code
should send requests through a credential-adding server. User-supplied Cloud
credentials would turn a default safety feature into setup work and expose
readers to provider billing, so that is not a daily-driver answer. The viable
choices are a provider-approved public-client credential or an Omaweb-operated
narrow proxy. The proxy may authenticate only list and prefix requests, must
not accept full URLs, and must
not log prefixes or client identifiers. Its cost and abuse controls need a
separate operational decision. [Google API terms, section 4b](https://developers.google.com/terms/#b_confidential_matters), [API-key practices](https://docs.cloud.google.com/docs/authentication/api-keys-best-practices)

Provider attribution, limitation language, and data terms must appear in About
and on the warning page. Runtime downloads, rather than the `PKGBUILD`, obtain
the current list. This follows Omaweb's existing rule that release engineering
keeps third-party data under its own licence in [ADR 0014](0014-license-omaweb-under-mpl-2.md)
and its Linux-only distribution decision in [ADR 0029](0029-distribute-only-for-linux.md).

## Deterministic contract and fixtures

Put provider-free fixtures under `tests/fixtures/url-reputation/`. No test calls
a live reputation service or includes a provider snapshot. Synthetic domains
use `.test`, and every fixture fixes the wall clock. Keep these files:

- `canonicalization.json` contains input URLs, expected host-suffix and
  path-prefix expressions, their SHA-256 hashes, and four-byte prefixes. Include
  escaped characters, repeated dots, numeric hosts, ports, query strings, and
  the official URL-processing examples. Google specifies the canonicalization,
  expression, and SHA-256 algorithm. [URLs and hashing](https://developers.google.com/safe-browsing/reference/URLs.and.Hashing)
- `reset.json`, `diff.json`, and `bad-checksum.json` contain small raw and
  Rice-coded list responses with fixed versions, removals, additions, checksum,
  and next-update time. They prove atomic replacement, remove-before-add order,
  checksum refusal, and recovery by a later full reset.
- `confirm-positive.json`, `confirm-collision.json`, `confirm-expired.json`, and
  `confirm-error.json` contain fixed hash-search responses. They distinguish a
  prefix collision from a full match and cover positive and negative cache
  expiry without sleeping.
- `navigation.json` describes expected decisions for safe, malware, phishing,
  stale, offline, bypassed, redirected, local, non-HTTP, and subresource cases.
- `network.json` lists the exact request method, host, path, headers, and allowed
  fields for each scenario. It rejects full URLs, full hashes, Space and tab
  identifiers, and telemetry.

The core test target owns parsing, updates, hashing, caching, expiry, and verdict
tests. A shared engine-contract suite then runs unchanged against Qt and
Ladybird. Each adapter must prove that it pauses a top-level request until the
core verdict, never starts the refused request, rechecks every redirect,
releases a safe or fail-open request exactly once, and leaves subresources out
of this contract. The same suite checks that `Proceed once` is limited to one
navigation, Private windows use protection without persisting a lookup, and an
unavailable capability is reported rather than silently bypassed.

The fake transport records every outbound field for the network fixture. The
fake clock crosses update, positive-cache, negative-cache, and provider-stale
boundaries deterministically. The Qt and Ladybird builds consume the same
fixture directory and expected verdict enum, so an adapter cannot redefine
`safe`, `unsafe`, `unavailable`, or `bypassed`.

## Conditions for revisiting the decision

Approve Safe Browsing v5 only after all of these are written down:

1. Google confirms that Omaweb's public Linux distribution and intended use are
   non-commercial or supplies separate terms.
2. Google permits the chosen runtime list and cache use and confirms that
   Omaweb does not redistribute provider data by letting each installation
   download its own list.
3. A production credential design keeps billable or quota-bearing secrets out
   of the package without sending full URLs or stable client identifiers to an
   Omaweb service.
4. The 30-minute freshness rule has an operational design and honest offline
   wording that Google accepts.
5. Tests measure list size, update bandwidth, collision rate, confirmation
   latency, false-positive corrections, and unprotected time over at least 30
   days on both supported engine contracts.

If Google does not resolve those points, price Web Risk from measured collision
traffic and negotiate terms for a public desktop client. The community-feed
pair is not the fallback for public daily-driver status unless both operators
grant current redistribution rights and an independent coverage evaluation
shows that the pair can replace a browser-grade phishing and malware service.
