# Gather problem reports from the reader's own browser

A report that reaches the tracker without the facts behind it costs a round trip per fact. #711 took
three rounds over two days to learn which version Google saw, the reporter redacted Google's
addresses by hand, and testing one suspicion took a one-off prerelease. Omaweb therefore writes the
Problem report itself, and the reader files it. Nothing is sent by Omaweb, and there is no
telemetry: the report leaves the machine only as text the reader submits on GitHub.

The Problem report is started from the Omnibar command "Report a problem", from Settings' About
section, or with `omaweb report`. The browser opens GitHub's bug form in a new tab with the report
in its report field. A link carries only about 8 KB, so the report is filled in a fixed order:
versions, what sites see, the tab, settings, Sync, the last crash and the Problem recording, cut
from its oldest end. A report that had to be cut is also copied to the clipboard, and the form says
to paste it. `omaweb report` prints the same text, and reads what it can from disk when no browser
is running. It is a browser command, open to an Agent without Allow agents and written to the
activity log, and a browser holding only Private windows refuses it as it refuses every request.

The report holds what is the same on every site: the versions of Omaweb, the engine, Chromium and
its security patch, Qt, Omarchy, Hyprland, the kernel and the architecture, the user agent, client
hints and language a page reads, the settings that differ from their defaults, Sync's state and its
last error, the extensions, the number of Spaces and tabs, and the last crash from `coredumpctl`.
Started from a tab, it adds that tab's address, its Space kind, the site's permissions, its zoom and
the Page scale. A Private window reports only that it was one. History, tab addresses, Space names,
cookies, passwords, form data and the permissions of other sites are never in it.

Addresses lose their user information and fragment. A query keeps its parameter names, and keeps a
value only when it is short and plain, a number or a word, because the fact a report needs is often
one of those: #711's was `rrk=46`. A long or random-looking value, where a token or an identifier
lives, becomes `…`. Stripping every query was rejected because it loses that fact, and leaving
redaction to the reporter is what this replaces.

A Problem recording catches what happened rather than what is. "Record a problem" starts it, a mark
in the sidebar shows it running, and it records navigations, redirects, hand-offs to other
applications, permission prompts and their answers, console errors, failed loads and Omaweb's own
warnings in every tab of the window, with addresses redacted as above. It is kept in memory only and
ends when the reader stops it, starts a report, which takes it in, quits, or after ten minutes. A
log kept all the time was rejected: a browser that keeps less should not keep a history of
everything for the day something breaks.

A Diagnostic override tests a suspicion on the reader's own build.
`omaweb --diagnose report-chrome=152` starts a browser that presents itself differently for that run
alone, with a banner while it lasts. Overrides are never saved or synced, and a report taken during
the run lists them. Each one is named and ships in a release, so a new suspicion is a new override
rather than a build made for one reader.

Rejected: a crash and error service such as Sentry. None of the bugs that prompted this crashed or
threw: a theme that came back, a sign-in refused, a hand-off that never arrived. A crash dump of a
browser holds page content and form values, so sending one by default contradicts Omaweb, and opt-in
leaves too few to learn from. systemd-coredump already keeps every crash on the machine, which is
where the report reads it from.
