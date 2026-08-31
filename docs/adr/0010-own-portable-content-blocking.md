# Own portable content blocking

Scriptlets moved inside the contract in [0025](0025-run-only-vendored-scriptlets.md);
the paragraphs below still read as though they had not. Every other exclusion stands.

Tanto provides Content blocking through a shared filter-list contract rather than an engine-specific Web extension. Both engine adapters must support network-request blocking, cosmetic element hiding, refusing the windows a list's `$popup` rules name, user lists, automatic updates, per-site disabling, and a visible blocked-request count. Tanto publishes unsupported filter syntax and does not claim scriptlet, dynamic-filtering, or full uBlock Origin compatibility until those behaviors exist.

The Qt adapter uses Brave's `adblock-rust` behind a narrow Tanto-owned C interface, pinned to the version used by the selected Ladybird revision. Ladybird uses its internal copy. Both adapters run the same fixtures for URL and hostname rules, exceptions, shared resource types, popup rules, first-party and third-party rules, domain restrictions, and plain CSS cosmetic rules and exceptions. The first contract excludes scriptlets, procedural selectors, response rewriting, HTML filtering, dynamic rules, CNAME uncloaking, and redirects or resource replacement.

Each engine matches requests against an immutable in-memory ruleset. Subscription updates download, validate, and compile off the request path, then atomically replace the active snapshot. Compiled caches are disposable local acceleration data and never enter Sync or backups. CI publishes compatibility results for the exact parser and engine revisions.
