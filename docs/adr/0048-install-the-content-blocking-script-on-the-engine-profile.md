# Install the Content blocking script on the Engine profile

The Content blocking script carries a site's hiding rules and Scriptlets into a document at its
creation, and it is rebuilt for each navigation because both depend on the address. The view
installed it in its own script collection at load start, by assigning the collection whole, and two
documents ran the wrong thing: the target of a cross-site redirect ran the script of the address the
load set out from, and a reload that started while the page before it was still loading, which the
development engine reports no load start for, ran that page's Scriptlet after the site had been
turned off. See #321.

Measured against the engine, the view's collection cannot carry the script at all for a navigation
to another site. The engine hands a view's scripts to a frame when the frame is created. A
navigation to another site creates its frame, in a renderer process of its own, before the view
hears of the navigation: before the load-started report and before the navigation request. A script
the view installs for the new address reaches the frame being left, and the new document runs the
script of the page before it, in every such navigation, on every timing the view can choose. A
cross-site redirect happens to work because its frame is created once the redirect is known, which
is after the navigation request the engine raises for it.

So the script is installed on the Engine profile, matched to its origin. An Engine profile's scripts
go to every renderer process the moment they are installed, and to a new process as it starts, and
the messages that carry them are ordered before the one that commits a document, so the script is in
place before any document is created wherever the navigation goes. The script is named for the
origin and carries the engine's own metadata header, `@include <origin>/*`, so a document runs the
script of its own origin and no other view's. Every view of the profile installs for the address it
is loading at the navigation request, which is raised for a redirect with the address the load is
arriving at and for a reload the load-started report is not raised for
([ADR 0046](0046-judge-a-redirected-document-by-where-it-arrived.md)); two views at one origin
install one script, and the second finds it there. The Engine profile keeps at most sixteen origins,
those installed longest ago going first, and a script left stale by a rule or site change is
replaced by the next navigation to its origin, which is before any document there is created.

The view's other scripts, the ones that report and survey, stay in the view's collection: each
carries the view's own seal, and an Engine profile's script cannot. That collection is now written
once, when the view is built, and edited a script at a time afterwards, for a theme or a keyboard
configuration that changes. Assigning it whole clears it and puts every script back, one message per
script, and leaves the engine one more observer of the view per assignment, each resending every
script to every new frame.

One miss remains, and it is the engine's. The first main frame of a renderer process the engine has
just started is sometimes handed the view's scripts only once its document has parsed, so the survey
script has run in nothing: seen in about one first navigation of a view in three here, a cross-site
redirect into a cold process, with the subframe of the same document surveyed. The survey a document
has not reported by load success is still asked for from the view, as #317 arranged, and the reason
is now that one.

What was rejected:

- **Editing the view's collection in place.** It removes the gap an assignment opens and the
  observers it leaks, and it is kept for the view's own scripts. It cannot reach a frame the engine
  created before the view heard of the navigation, which is every navigation to another site.
- **Installing at the navigation request or the load start, in the view.** Both are raised after
  that frame exists. The same measurement showed the engine's order: the navigation request, then
  the load start, both after the frame.
- **A constant script that asks for its rules by address at document creation.** The ask is a report
  over the console and the answer a script run from the view, a round trip that ends after the
  page's first script has run and its markup has rendered, which is what the script exists to
  precede.
- **Moving every script to the Engine profile.** It would put the reporting scripts beyond the
  engine's cold-process miss as well, at the price of a seal shared by every view of a Space and a
  keyboard configuration applied from outside the script. The miss it would close is caught already,
  and the price is a change to what a report proves.
