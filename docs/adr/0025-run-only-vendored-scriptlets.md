# Run only vendored scriptlets

Supersedes the part of 0010 that placed scriptlets outside the first content-blocking contract. The
rest of that contract's exclusions stand.

A `##+js(...)` rule is the point where a filter list stops being data. Every other rule Omaweb
honours describes something, an address to refuse or a selector to hide, and Omaweb decides what to
do about it. A scriptlet rule asks for code to run in the page's own world, before the page's own
scripts, which is exactly the privilege a Web extension has and the reason Omaweb does not load
third-party extensions. Scriptlets are still worth having: a scriptlet is what neutralises an
anti-adblock wall or a tracker's own initialiser, so the pages that need one most are the pages
where network blocking alone shows nothing but a demand to turn it off.

The rule may name code. It may not supply it. Omaweb vendors uBlock Origin's scriptlet library under
`third_party/ubo-scriptlets`, pinned to a commit by `MANIFEST.json`, and a rule contributes a name
from that set plus the arguments to call it with. `adblock-rust` resolves the name against the
library and returns the library's own source together with the call; a name the library does not
carry yields nothing at all, rather than a call to something a list defined. So the set of code that
can ever run in a page is the set in the repository, reviewable as a diff, and a list that changes
upstream cannot enlarge it.

uBlock Origin marks twenty-nine of its scriptlets as requiring trust. `trusted-set-cookie` sets any
cookie to any value, and `trusted-replace-fetch-response` rewrites what a page reads back from the
network. Upstream grants that trust to lists the user vouched for individually. Omaweb grants it to
none: the generator stamps those resources with a permission bit, every rule set compiles with no
permissions, and `adblock-rust` refuses the injection. Subscribing to a list is not a statement
about that list's authors, and Omaweb has no interface today in which it could become one.

A refusal is reported, not swallowed. A rule naming a trust-gated scriptlet, or a name the pinned
library does not carry, is counted in its own unsupported category rather than among the rules the
list contributed. The two are kept apart, because one says the pin is behind upstream and the other
says Omaweb declined on purpose. Counting either as accepted would have Settings advertise a
compatibility that does nothing on a page, which is the claim `0010` set out not to make.

`scriptlets.json` beside the copies is the same library expressed as `adblock-rust` resource
descriptors, and it is committed. Generating it means importing uBO's ES modules and asking what
registered itself, which is Node's job and no build's: `scripts/build_ubo_scriptlets.mjs` runs at
vendoring time, `scripts/sync_ubo_scriptlets.py` drives it and records its digest in the manifest,
and `ctest` fails if either the copies or the generated file drifts from the pin. A build needs
neither Node nor the network, and the content blocker builds the descriptors straight into its own
binary.

The library expects one name from its host, `scriptletGlobals`, through which uBO passes its
extension's origin, a logging channel, and a cache of the sandbox it builds. Omaweb passes an empty
object. That is not a stub: an absent origin is what says "no web-accessible resources to redirect
to", and an absent channel is what says "nothing is listening". The library reads the object
defensively and builds the rest itself.

Injection rides the per-navigation document-creation script that already carries the cosmetic
stylesheet, and it runs in the main world in a scope of its own. Two consequences are deliberate. A
site the user turned blocking off for runs no scriptlet, because the per-site switch is checked
where the code is fetched rather than where it is injected. And a scriptlet is never re-applied into
a document that is already open: one that missed the page's first script has nothing left to
intercept, and one that already ran cannot be taken back, so a rule set or a per-site decision that
changes mid-page waits for the next navigation. Only the stylesheet is re-applied in place.

This is the first third-party code Omaweb compiles into a binary it distributes rather than links
dynamically or downloads at runtime, and it is GPL-3.0-or-later while Omaweb-owned code is MPL 2.0
([0014](0014-license-omaweb-under-mpl-2.md)). The two are compatible in the direction that matters,
since MPL 2.0 is explicitly a secondary-license grant for GPL. But "compatible" is not the same as
"settled": what the combined binary may be distributed under, and what corresponding source must
accompany it, is a release-engineering question this decision raises rather than answers.
`THIRD_PARTY_NOTICES.md` carries the requirement; the release process owes it legal review before a
public build, the same way the media codecs do. Nothing here changes for a source checkout or a
development build.

uBlock Origin runs some of its scriptlets in an isolated world, which `adblock-rust`'s composed
script does not distinguish; here they all run in the main world. That is a robustness difference
against a hostile page, not a capability one.
