# Apply procedural cosmetic filters

Supersedes the part of [0010](0010-own-portable-content-blocking.md) that kept procedural selectors
outside the content-blocking contract.

A procedural cosmetic rule finds an element a CSS selector cannot name: the `div` whose text says
"Sponsored", the card whose link points at a tracker, the ancestor three levels above a match.
uBlock Origin's lists and EasyList carry them for sites whose markup changes too often for a
selector. Until now Omaweb counted every one of them as unsupported, so a list's author and a reader
both saw the element stay.

## What is supported

Exactly what the pinned `adblock-rust` 0.12.5 parses, and nothing it does not. The operators are
`:has-text` (and its alias `:-abp-contains`), `:matches-attr`, `:matches-css`,
`:matches-css-before`, `:matches-css-after`, `:matches-path`, `:min-text-length`, `:upward` and
`:xpath`. The actions are `:remove()`, `:style()`, `:remove-attr()` and `:remove-class()`, and a
rule with none hides what it matches.

The parser splits a rule into operators only with its `css-validation` feature, which the content
blocker turns on; without it every procedural rule reads as a plain CSS selector no browser knows.
Only rules written against a site are applied. The parser refuses a generic procedural rule, one
with no domain, because running it would cost every page a document-wide search.

## What stays unsupported

A generic procedural rule, and a rule using an operator the parser lacks (`:watch-attr`,
`:matches-prop`, `:shadow`, `:others`, and ABP's `#$#` snippets), is reported in a category of its
own, apart from HTML filtering and the other unsupported syntax. Settings then says what a list lost
to the parser rather than folding it into a count a reader cannot act on.

## Whose matcher

Brave's `procedural_filters.ts` from brave-core finds the elements. It is MPL-2.0 like Omaweb, it
takes the operator JSON `adblock-rust` emits as it is, and it has no imports, so it is vendored the
way the uBlock Origin scriptlets are ([0025](0025-run-only-vendored-scriptlets.md)): pinned to a
commit, bundled at vendoring time, checked against a manifest by `ctest`.

It only finds elements. What is done with them, hiding, `:style()`, `:remove()`, `:remove-attr()`
and `:remove-class()`, is a small dispatch of Omaweb's own, because Brave's lives in a content
script bound to Brave's own messaging.

Two alternatives were declined. uBlock Origin's filterer is GPL, which Omaweb's MPL-2.0 cannot take
in ([0014](0014-license-omaweb-under-mpl-2.md)), and it expects uBlock Origin's compiled filter
format rather than the parser's JSON. A matcher written here would repeat work Brave maintains
against the same parser, and would drift from it.

## Where it runs

In `ApplicationWorld`, like the generic cosmetic survey and the collapse of refused elements, so the
page can neither reach the matcher nor tamper with it. A script injected at `DocumentCreation` in
every frame asks for the rules of the frame's own address, so a subframe from another site gets that
site's rules, and the answer loads the matcher into the frame only when there are any. A frame the
engine hands the view's scripts too late to ask, which the survey's own fallback already covers,
asks through its survey instead.

## How it hides

A hide sets a marker attribute whose name is random for each page, and one stylesheet rule hides
whatever carries it. An inline style would be visible to the page and would fight the page's own
style changes; a fixed attribute name would let a page find and undo the hide. A hide does not move
the Refusal tally ([0037](0037-count-refusals-per-page-address.md)), which counts requests.

## How long it watches

For the life of the page. A procedural rule's match can appear long after load, when a feed pages in
or a text node changes, so a MutationObserver watches the document. Mutations are batched and the
rules re-run at most every 100 ms, and only the rules the mutation's kind can affect: a text change
re-runs `:has-text` and `:min-text-length`, an attribute change `:matches-attr`, the `:matches-css`
family and the attribute actions, and a tree change all of them, because it can bring any element
in.

## An open page when the rules change

A list update, a user rule, or a site whose blocking was switched off is applied in place, as the
plain cosmetic stylesheet is. The watch stops, and hides, styles, removed attributes and removed
classes are undone. What `:remove()` deleted stays gone until the page reloads, because the element
is no longer there to put back. Switching the site on again starts the matcher with the current
rules.

## Per engine

Procedural cosmetic filtering is an engine capability, reported per engine as
[0010](0010-own-portable-content-blocking.md)'s contract allows. The Qt engine offers it. Ladybird
does not until its adapter lands ([#7](https://github.com/villekivela/omaweb/issues/7)), and
Settings names the engines that lack it, as it does for CNAME uncloaking
([0050](0050-uncloak-cname-trackers-in-the-engine.md)).

## What this costs

A frame with no procedural rule for its address pays one report and one rule lookup, which the
stylesheet's per-address cache answers, and loads no matcher. A frame with rules is sent the matcher
and the dispatch, about 22 KB of script, and pays their parse, a search of the document per rule,
and the watch.

The page-load budget holds it to a number
([`performance/budget.json`](../../performance/budget.json),
`pageload_procedural_hosts_milliseconds`). Its procedural case is the common case's page of 40
images from 4 hosts with the fixture's markup and one rule per operator and action on it. Measured
on CI's runner on 2026-09-26, blocking cost it 2.8 ms over the site switched off, where the page
without the rules cost 8.3 ms in the same run: what the rules cost is below what the runner can tell
apart. A first attempt gave every row of the fixture the same element names on one page, and one
row's `::after` style reached blocks two other rows hide, so the page came out 70 ms faster with the
rules on; the budget measures each row under names of its own.
