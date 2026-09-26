# Omaweb

Omaweb is a keyboard-driven web browser. Omaweb is meant to be a daily driver on Linux, with
first-class Wayland support, and is distributed for no other platform. Its macOS build must stay
usable during development.

## Language

**Development engine**: The mature web engine used to build and validate Omaweb while the target
engine is not ready. QtWebEngine is the current development engine. _Avoid_: Fallback engine

**Target engine**: The web engine Omaweb ultimately intends to use for everyday browsing. Ladybird
is the current target engine. _Avoid_: Experimental backend

**Local-development site**: A site operated by the reader for software development, reached through
localhost, an IP literal, or a reserved local-development domain. Omaweb may give it narrowly scoped
behavior that public sites never receive, such as a one-time overridable certificate exception.
_Avoid_: Trusted site, Project site

**Space**: A named browsing identity with its own logins, site data, permissions, history, session,
and tabs. Switching Spaces does not expose one Space's browsing identity to another. _Avoid_:
Workspace, container, profile (a Space is a browsing identity, not an Engine profile)

**Engine profile**: The directory where one web engine keeps its own state for one Space: cookies,
site storage, cache, and whatever else that engine writes for itself. A Space has one per engine
that has run in it, and a Private window's engine profile is temporary and shared. _Avoid_: Profile
directory, engine data, user data dir

**Pinned tab**: A tab saved within one Space and restored whenever that Space resumes. Its saved
address changes only when the user explicitly updates the pin. _Avoid_: Bookmark, favorite

**Keep active**: A Pinned-tab setting that lets its page continue running while the reader is
looking at something else, whether that is another tab of its own Space or another Space entirely.
Freezing does not reach it, so it is the one page the reader can rely on to still be doing something
when they are not there. The setting survives restart, and Omaweb always identifies the retained tab
and its resource use. _Avoid_: Background Space, never suspend

**Frozen tab**: A tab whose page has stopped running because the reader is not looking at it. It
keeps its document and everything the page holds, and continues where it stopped when the tab is
selected, rather than loading again. A tab an inspector is attached to, a tab making sound and an
Agent tab are not frozen, and neither is a Pinned tab marked Keep active. _Avoid_: Suspended tab,
sleeping tab, discarded tab

**Sounding tab**: The tab that is making sound, and the one Omaweb announces to the desktop so the
media keys, the bar's media widget, and anything else that asks reach it. There is one for the whole
browser rather than one per tab: where two tabs are playing, the one that started last is the
Sounding tab, and when it stops or is paused the one still playing takes over. What the desktop is
told is what the page declares about itself, falling back to the tab's own title, and a tab that is
paused stays the Sounding tab so the next key press has somewhere to go. A Private window's Sounding
tab announces that something is playing and nothing about what. _Avoid_: Now playing, media tab,
active player

**Space at rest**: A Space with nothing open in it, because nothing has been opened yet or the last
page has been closed. Its only ordinary tab is blank. A Space at rest lists no ordinary tab and
shows the Start page in place of a webpage; its Pinned tabs are unaffected. _Avoid_: Empty tab,
blank tab, new tab

**Split**: Two ordinary tabs of one Space shown side by side where one webpage would be, and listed
as one sidebar row. One of the two is the active tab; the other is the tab beside. A split belongs
to its Space, stays listed while the reader looks at other tabs, and ends when either tab closes or
leaves the Space. _Avoid_: Split view (as the name of the thing), tab group, pane group

**Tab beside**: The tab of a split that is on show but not active. Its page runs and is seen, and
nothing else about the browser answers for it until the reader focuses it. _Avoid_: Companion tab,
secondary tab, other pane

**Tab jump list**: The order in which a Space's tabs became active, kept in memory for the current
run and never written down. Every activation of a tab appends it, a tab listed earlier moves to the
end rather than appearing twice, and a tab that closes leaves the list. Jumping back and forward
walks the list without changing it, so a new activation after a jump keeps the entries ahead of it
and appends at the end. Each half of a split is its own entry. Each Space has one, and it survives a
switch to another Space and back. _Avoid_: Tab history, recent tabs, tab stack, last tab

**Start page**: What Omaweb draws where a webpage would be whenever the tab on show has no address
to load, in a Space at rest or on `about:blank` itself: the browser's keyboard commands and the keys
that run them, rather than a document loaded from anywhere. It costs no engine and takes the
sidebar's translucency. The same sheet answers on demand over a live page, where it is opaque and
closeable instead. _Avoid_: Home page, new tab page, about:blank, cheat sheet

**Private window**: A temporary browser window whose browsing identity is separate from every Space
and whose distinct appearance identifies it as private. Private windows share temporary state with
each other only while at least one private window remains open. _Avoid_: Private Space, incognito
Space

**Private accent**: The one colour a theme names for its Private windows. Omaweb derives the grounds
it is cast over, the Private window, sidebar, surface and hover fill, by tinting each of the theme's
own surfaces towards it, so a theme identifies private browsing by naming a hue rather than by
naming a palette, and a Private window keeps the darkness the theme drew its chrome in. A theme that
names a ground itself keeps it. _Avoid_: Private colour, incognito accent

**Glance**: A page opened by a link that asked for a new tab, shown over the tab it came from
instead of as a tab. It runs on the Space's engine profile and records its visits, but it is not
listed, saved, or synced, and the chrome goes on answering for the tab beneath. It ends with
`Escape`, the close-tab command, or anything that changes the tab on show, and one command keeps it
as an ordinary tab, page and all. _Avoid_: Peek, preview, popup, quick look, overlay tab

**Auxiliary window**: A temporary, minimally framed window requested by a site to complete a flow
such as authentication or payment. It inherits the browsing identity of its opener and is not an
ordinary Omaweb browsing window. _Avoid_: Browser window, pop-up tab

**Keyboard navigation**: An optional first-party command layer for controlling Omaweb and the active
page without a pointer. It provides the same user-facing commands with every engine and lets sites
receive selected conflicting keys. _Avoid_: Vim mode, Vimium extension

**Omnibar**: A centered overlay for opening addresses, searching, selecting tabs or Spaces, and
invoking browser commands. A new-tab request creates its tab only after the user commits a
destination in the Omnibar. _Avoid_: Command bar, omnibox, command palette

**Developer tools**: The inspector supplied by the current web engine and attached to one tab.
Omaweb opens it, positions it, and draws it in the browser's own theme, but does not normalize its
interface or debugging protocol across engines. _Avoid_: Diagnostics, DevTools platform

**Agent**: A program the reader runs, such as a coding agent or their own script, that drives Omaweb
through its control socket. Browser commands are open to every Agent; reading or driving a page
needs Allow agents and, outside an Agent Space, a Space grant. Omaweb holds no model and no provider
credentials; the Agent is always the reader's own. A connection names itself for the activity log
and the markers, but the name is not an identity. _Avoid_: Bot, assistant, automation, Diagnostics
client

**Agent Space**: A Space an Agent created, which Agents may use without a Space grant. It is marked
as an Agent's, and the reader can take it over, which removes the mark and keeps the Space. A
temporary Agent Space is deleted with its Engine profile when the connection that created it closes.
_Avoid_: Bot Space, sandbox, throwaway profile

**Space grant**: The reader's standing permission for Agents to use a Space they did not create,
given once from a prompt and kept until revoked in Settings. It belongs to the Space rather than to
any Agent, stays on this machine and is outside the Sync projection. A Private window can never be
granted. _Avoid_: Agent permission, Site permission, pairing

**Agent tab**: A tab an Agent is attached to. It is marked as such in the sidebar, and the Space
holding it is marked too. It is not frozen and stays rendered behind the page on show, so an Agent
can work in a Space the reader is not looking at, and an Agent tab never takes the reader's focus.
_Avoid_: Automated tab, bot tab, controlled tab

**Web extension**: A third-party browser package that can modify pages or add browser behavior
through a supported WebExtensions contract. _Avoid_: Feature module, plugin

**Known extension**: A Web extension Omaweb names on its own, with its source and license, so a
reader enables it in Settings with one action rather than by installing a package. Omaweb loads no
Web extension it has not named, and a Known extension runs only on an engine that reports it can
host one. _Avoid_: Installed extension, vendor extension, password-manager extension

**Feature module**: An optional Omaweb component that adds a first-party product capability without
becoming part of the browser core. Account and Sync are Feature modules. _Avoid_: Extension, plugin

**Account**: An optional identity used by a Feature module to access remote services. An Account is
never required for local browser features. _Avoid_: Space, browser profile (an Account is remote
identity, not an Engine profile)

**Sync**: An optional Feature module that copies selected non-secret browser state between Omaweb
installations through a replaceable provider. _Avoid_: Backup, account

**Sync projection**: The positive allowlist of browser state Sync may copy: Space names, colours and
order, ordinary and Pinned open tabs, approved Settings, keybindings, and filter subscriptions.
Active Space and tab selection stay local, and Browsing data and every Private window are outside
the projection. _Avoid_: Browser backup, profile snapshot, all settings

**Site permission**: A Space-specific decision that allows or blocks an origin from using a
protected browser capability. A Private window retains its Site permissions only while the shared
private session exists. _Avoid_: System permission, engine permission

**Browsing data**: What a Space accumulates as it is used and can be cleared without unmaking the
Space: its cookies, site storage, cache, Site permissions and history. Browsing data is cleared by
category and time range, and clearing it leaves the Space itself, its Pinned tabs, its name and
colour, and everything the reader configured about the browser. Deleting a Space is the separate act
that removes the Space as well. _Avoid_: History, cache, site data, user data

**History search**: The Omnibar's search of the addresses and titles a Space has visited, answered
off the interface's thread while the reader types. It reads one Space, the one on show, and answers
only the most recent request. A Private window keeps no history, so it has none to search and is
answered with nothing. _Avoid_: Autocomplete, suggestions, omnibox search

**Site information**: What Omaweb states about the site on show, for the Space it is on show in: its
origin, the connection the engine reports and the certificate it arrived over, the requests Content
blocking refused, the site data the Space holds, the origin's Site permissions, and the confirmed
ways to clear that data or reset those decisions. Reached from the address trigger, which reports
the connection from the engine's own facts and never from the address itself. _Avoid_: Site status,
page info, security panel

**Certificate exception**: A reader's decision to let one blocked certificate failure through.
Omaweb offers it only for an engine-overridable, non-fatal failure in a Local-development site's own
main frame, and writes nothing down, so a later session asks again. The engine holds the exception
for the rest of the session and cannot be made to forget it, so Site information and the address
trigger keep reporting the connection as in error for as long as it stands. _Avoid_: Trusted
certificate, security exception, certificate override

**Global Privacy Control**: The one signal Omaweb sends every site about the reader: `Sec-GPC: 1` on
every request an Engine profile makes and `navigator.globalPrivacyControl` reading `true` in every
frame, which a site bound by the CCPA and the state laws written after it must treat as an opt-out
of sale and sharing. It is one setting for the whole browser, on by default, kept with the reader's
configuration, and every Space and Private window sends it alike. _Avoid_: Do Not Track, DNT,
privacy header, opt-out signal

**WebRTC address policy**: What a page's call may learn about the reader's network. WebRTC gathers a
candidate address on every interface the host has and hands the list to the page before any call is
placed, so the policy offers the public interface only: the address of the default route, which is
what a call connects through, and not the reader's LAN address or the one a VPN hides. It is one
setting for the whole browser, on by default, kept with the reader's configuration beside Global
Privacy Control, and every Engine profile, a Space's and the Private windows' shared one, carries it
alike. Off is for a reader whose peer is on their own network. _Avoid_: WebRTC leak protection, IP
leak, disable WebRTC

**Third-party cookie allowance**: A temporary permission for one origin to keep cookies and site
storage while embedded in another site, granted for a named authentication or payment flow inside
one Space. It is held in memory only, listed in Site information, and revocable there, so it cannot
cross a Space or outlive the session that granted it. _Avoid_: Cookie exception, tracking allowance

**High-risk download**: A file intended to be run, installed, or mounted: an executable, script,
installer, disk image, or archive. Omaweb identifies the type before downloading it, never opens it,
and removes its execute permissions. _Avoid_: Dangerous file, malware, unsafe download

**Download disposition**: What Omaweb decides about a download request before it starts: accept it,
ask the reader to confirm it, ask whether the origin may download by itself, refuse it, or ask where
to put it. _Avoid_: Download rule, download action

**Held download**: A download that Omaweb cancels while waiting for the reader. The engine requires
a synchronous decision, so it cannot pause the request. If the reader accepts, the page requests the
file again. _Avoid_: Paused download, pending download, blocked download

**Download record**: What a Space remembers about a download once it is no longer running. A Private
window keeps none, so a download there is only ever running. _Avoid_: Download history entry, saved
download

**Download activity**: What a window's running downloads add up to: how many there are and how far
along they are together. What the Download mark reports. _Avoid_: Download progress, download status

**Download mark**: The outline footer control for active downloads. It shows their count and
combined progress, and can list progress by file. It uses the window's live downloads, so it also
works in Private windows. After the last download finishes, the mark remains until the saved-file
notice closes. It opens the downloads section. _Avoid_: Download badge, progress bar, download
indicator, download tray

**Release check**: The once-a-day question Omaweb asks GitHub about its own age. It sends nothing
about the machine, runs off the engine so it belongs to no Space, and is silent when it fails. A
Setting turns it off. _Avoid_: Update check, auto-update, version ping

**Release mark**: The outline footer control for a release newer than the running build. It names
the version and what to run to get it, opens the release notes, and is not shown in a Private
window. Omaweb never installs the release itself. _Avoid_: Update badge, update banner, upgrade
prompt

**Engine security baseline**: The approved QtWebEngine version and the latest Chromium security
patch it includes. Settings reports whether the running build meets the baseline, and CI checks for
upstream changes each week. Builds below the baseline are unsupported previews. _Avoid_: Minimum Qt
version, supported engine

**Hardware video decode**: Video frames decoded by the GPU rather than by the CPU, which Omaweb asks
the engine for and a VA-API driver on the host answers. A host with no working driver decodes in
software and starts normally, so the driver is a host's choice rather than a requirement of the
browser. _Avoid_: GPU acceleration, VA-API, hardware acceleration

**Content blocking**: Omaweb's built-in removal of unwanted network requests and page elements using
subscribed filter lists. Content blocking is a browser capability and does not depend on an
installed extension. _Avoid_: Ad-blocking extension

**Known list**: A filter list Omaweb names on its own, with its source and license, so a reader
subscribes it in Settings with one action rather than by address. EasyList and EasyPrivacy are Known
lists subscribed on a first run; EasyList Cookie is a Known list subscribed only when the reader
asks. Subscribed, a Known list is a subscription like any other. _Avoid_: Built-in list, preset,
recommended list

**Refusal tally**: What Content blocking refused for one page address in one Space: how many
requests the document loaded there has been prevented from making. A new document at that address
starts the tally again, and a navigation within that document carries it on, so the tally follows
the document rather than the reader's sense of a new page. Two tabs on the same address in the same
Space read the same tally. _Avoid_: Blocked count, blocked-request total, blocked requests

**CNAME uncloaking**: Content blocking checking a subresource request the lists let through again
under each name in its host's DNS CNAME chain, so a tracker served from a site's own subdomain is
refused as the tracker it is. The engine resolves the host for Content blocking; a canonical name on
the request's own site is not checked. An uncloaked refusal counts in the Refusal tally and is
listed in Site information under the address the page asked for. _Avoid_: CNAME blocking, DNS
filtering

**Secure DNS**: The reader's choice to have every name the browser looks up resolved over
DNS-over-HTTPS by a resolver they chose, instead of by the system's resolver. Off by default,
browser-wide, and secure only: a resolver that cannot be reached fails the lookup rather than
sending it in the clear. _Avoid_: DoH, encrypted DNS setting, private DNS

**URL reputation**: An optional browser capability that checks an address or download against a
provider's current phishing, malware, or software-reputation data. Content blocking may refuse some
known malicious addresses but is not URL reputation. URL reputation is not part of Omaweb's
daily-driver contract. _Avoid_: Safe Browsing, malicious-site blocking

**Substitute resource**: A small stand-in body from the vendored uBlock Origin library that Omaweb
serves in place of a request a filter list's rule names with `$redirect`. A filter list names a
Substitute resource and never supplies its body. _Avoid_: Redirect resource, stub, mock response

**Scriptlet**: A function from the vendored uBlock Origin library that a filter list's rule names
and supplies arguments for, run in a page before the page's own scripts. A filter list names a
Scriptlet and never supplies its code. _Avoid_: Injected script, user script

**Site fullscreen**: A page holding the whole screen because the site asked for it. It begins with a
notice naming the origin and always ends with `Escape`. It is separate from the fullscreen the
reader asks Omaweb for, and handing one back never takes the other away. _Avoid_: Fullscreen mode,
video fullscreen

**Theme palette**: A complete, versioned set of visual values that defines Omaweb's current
appearance, including the semantic opacity values for its interface surfaces and the type family and
base size they draw with. Omaweb can replace the active theme palette while running. _Avoid_: Theme
injection, color scheme

**Transparent surface**: An Omaweb-owned interface region whose theme allows the desktop to remain
visible behind it. Webpage viewports are not Transparent surfaces. _Avoid_: Transparent webpage,
click-through region
