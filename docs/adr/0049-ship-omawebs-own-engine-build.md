# Ship Omaweb's own engine build

Supersedes the part of [0013](0013-preserve-engine-sandboxes-in-every-build.md) that deferred a
bundled engine until Omaweb could maintain engine security updates, and the part of
[0001](0001-isolate-web-engines-by-build.md) that withheld extensions until both engines support the
same extension system.

Omaweb builds QtWebEngine itself, from the released Qt source with a patch series applied, and ships
that build. The engine is its own package installed into a private prefix, so it never replaces the
distribution's `qt6-webengine` and no other Qt application on the machine runs it.

The patches are what a Chromium extension needs and Qt does not offer: the missing renderer
bindings, a fix for a crash when a profile's storage path changes, the `tabs`, `windows`,
`permissions` and event namespaces, `scripting`, and native messaging. The series and its process
live outside this repository, because they carry Qt and Chromium source under their own licences and
an engine does not belong in Omaweb's tree. What they buy is a **Known extension**: Bitwarden and
1Password run, and a reader's password manager fills a form.

## What is promised, and what is not

Omaweb does not promise that Chromium extensions work. It promises the ones it names. A Known
extension is one Omaweb has run against these patches and tested, and it is the only kind Omaweb
loads.

The patches are generic API work rather than accommodations for one vendor: they implement
namespaces as Chromium defines them, behind an embedder delegate. So extensions Omaweb has never
tried may well run, and some certainly will. That is a property of the engine, not an offer. An
untested extension becomes supported by being tested and named, which costs a reader nothing and
Omaweb a run of the extension probe.

Large parts of the surface are still missing: `cookies`, `contextMenus` and `notifications` have
schemas but no implementation, `action` cannot draw a badge, `webRequest` listeners never fire, and
`tabs` carries only its read side. An extension that needs any of them fails, and finding out which
is what naming one costs.

## Why this and not the alternatives

Waiting for Qt was the first plan and it is still the preferred ending, but it cannot be the plan.
Qt strips the chrome-layer extension schemas deliberately, has not moved in three releases, and its
documentation now avoids both "extension" and "browser". The two bug fixes will likely land
upstream. The rest ask Qt for an application-supplied tab model it has shown no appetite for. A
capability that ships only if that request succeeds does not ship.

Maintaining the series proved cheap enough to change the answer. It rebased onto 6.11.2 with no
conflicts and no one's time, and a Chromium major bump, measured across six versions, moves the
copied files by four to thirteen lines each. The cost is a build, and a build is machine time.

Ladybird is no longer a reason to wait. Extensions are a per-engine capability and Ladybird will
have its own story, so Omaweb reports the capability per engine rather than withholding it until
both agree.

## What this commits Omaweb to

Omaweb becomes the vendor of record for its readers' engine security. The commitment is two days
from Qt publishing a release to a qualified Omaweb release, and the baseline check looks daily
rather than weekly so the window is not spent on not having looked. Qt is the upstream that starts
the clock, not a distribution: Arch built QtWebEngine 6.11.2 within an hour of Qt publishing it and
had it in the repository two days later, which is lag Omaweb now owns instead of inherits.

Because Omaweb ships the engine, a late rebase delays a release rather than breaking one. Readers
keep the build they have and their extensions go on working. Where a rebase cannot make the window,
Omaweb drops the extension patches and ships the engine anyway: the two bug fixes apply to anything,
so the Chromium fix reaches readers and Known extensions go missing until a follow-up engine update
restores them. The vault data is untouched, because it belongs to the Space's engine profile.

Distributing a modified QtWebEngine means publishing the modified source, keeping it a separate
shared library so it can be relinked, and marking it as modified. Chromium's attribution goes in
`THIRD_PARTY_NOTICES.md`. Signing happens where the key lives and never on a build machine.

Release builds are Linux, for x86_64 and aarch64. A macOS build is a development tool and cannot
ship, so the engine is built on machines rented for the purpose and destroyed afterwards.

## What ends it

Upstream submission continues in parallel, and the day Qt carries the work this decision is reversed
rather than maintained: the series is deleted and Omaweb goes back to the distribution's engine. The
patch repository exists to become unnecessary.
