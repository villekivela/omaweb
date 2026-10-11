# Automatic network requests

A first run subscribes to EasyList and EasyPrivacy with both enabled, so the first startup check
fetches both lists. Later runs read the stored subscriptions and subscribe to nothing on their own,
so removing a list stops its requests for good. Turning a subscription off in Settings also stops
its requests. EasyList Cookie is named in Settings and fetched only after the reader subscribes it,
from `https://secure.fanboy.co.nz/fanboy-cookiemonster.txt`, the address easylist.to publishes for
it.

Omaweb checks enabled Content-blocking subscriptions when the browser starts. Each request goes only
to the update address shown for that subscription. It sends a normal HTTP GET with no browsing
history, Space identifier, Account information, or user-rule data. Disabling a subscription stops
its update requests. Applying a changed subscription set from Sync performs the same enabled-list
checks and may fetch a newly received or stale list from its displayed update address.

Omaweb asks GitHub once a day what its newest release is, so that a reader running an old alpha is
told one exists. The request is a GET to the releases endpoint of this repository. It carries the
browser's name and version as its user agent and nothing else: no identifier, no history, no Space,
no machine detail. The answer is read for a tag and nothing else is kept. The check runs off the
engine, so it takes no Engine profile and appears in no Space's history, and a Private window is
told nothing about it. A check that fails is silent, because being offline is not a browser fault.
The day is counted from the last answer rather than the last attempt. Turning off "Check for new
releases" in Settings stops the request. Omaweb never downloads or installs a release: pacman owns
`/usr`.

The first launch after an upgrade opens the running release's notes on `omaweb.app` as a tab behind
the page on show. A tab opened behind the page loads nothing until it is selected, so the request is
the reader's. The setting above does not cover it, because it is a page and not a check.

With Secure DNS on, every name any Engine profile looks up is sent to the resolver chosen in the
privacy section of Settings, as a DNS-over-HTTPS request to the address Settings shows, instead of
to the system's resolver. That resolver learns every site visited, from every Space and Private
window. Secure DNS is off by default, and turning it off sends names back to the system.

Every request an Engine profile makes carries `Sec-GPC: 1`, the Global Privacy Control header, while
the setting is on; it is on by default and the privacy section of Settings turns it off. The header
adds no request of its own and goes on requests from every Space and Private window alike. The
subscription checks and the release check above run off the engine and do not carry it.

The browser sends network requests only after an explicit user or page action:

- Committing an address in the Omnibar loads that address.
- Committing non-address text searches the default engine, or the engine a typed keyword chose, at
  that engine's query URL.
- Navigations started by a loaded page use the active Space's engine profile.
- Accepting a download fetches the requested file.
- Adding a Content-blocking subscription fetches its declared update address immediately.

Favicons come only from pages the Space has loaded. When a tab's page reports its favicon, Omaweb
reads the icon from the icon store the engine already filled while loading the page and keeps a copy
in the Space's own database. A restored tab and an Omnibar row draw that copy, and a two-letter tile
with favicon artwork turned off takes its colour from it, so no tile or row ever costs a request. A
site the Space has never loaded keeps its host code, and one whose icon carries no colour keeps a
neutral tile. Omaweb never asks a third-party favicon service or a site's `/favicon.ico` for an
icon.

The engine asks a site for the icon its page declares once per run. Omaweb removes the engine's own
favicon database at each start, so an icon that follows the colour scheme is drawn under the current
one ([ADR 0065](adr/0065-draw-favicons-afresh-in-each-run.md)). The HTTP cache answers where the
site allows it.

Engine suggestions are off by default, and while they are off typing in the Omnibar queries only the
active Space's local history and sends nothing over the network. With the Engine suggestions switch
in Settings' network section on, typing search terms in the Omnibar sends them, about 150 ms after
the last keystroke, to the suggest URL of the engine Return would search: the one in the keyword
chip, or the default engine. The request is a GET that carries the typed terms in that address and
nothing else: no cookies, no Space, no history, and `Omaweb` as its user agent. It goes through the
browser's own network client rather than an Engine profile. Nothing is sent for an address, in
command scope, for empty terms, or to an engine without a suggest URL, and a Private window never
asks whatever the setting says. Turning the switch off stops the requests.

Omaweb opens no listening socket during an ordinary session. The `--remote-debugging[=port]` launch
option is the one exception: it binds Chromium's debugging listener to loopback, prints the address
and a warning, and disables Private windows for that launch. It is never on by default, and a
Chromium debugging switch passed to the engine by any other route refuses the launch.

Future features that add sync, telemetry, or another background request must document the
destination, trigger, data sent, default state, and disable control here before release.

Sync is off by default and is unavailable in Private windows. Connecting it starts GitHub's device
authorization flow and polls GitHub only while that flow is active. GitHub receives the App client
ID, device code, and ordinary authorization metadata. After login on the first device, Omaweb opens
GitHub's new-repository form with private visibility and the Sync repository details prefilled. Once
the reader confirms creation and continues, Omaweb opens App installation; the reader selects only
that Sync repository. Omaweb polls until the App can access it and downloads the identity's avatar
once. No browsing state is sent to the avatar host.

While Sync is enabled, git fetch and push contact the private repository after 30 seconds without a
new local change, every five minutes, after reconnecting, or when Sync now is pressed. Omaweb
refreshes an expired GitHub access token before one of these reconciliations by sending the refresh
credential and App client ID to GitHub's OAuth endpoint. Spaces and tabs are encrypted before
upload. The repository also receives readable approved settings, keybindings, filter subscription
addresses, commit authorship using the GitHub login, and the timing and approximate size inherent in
git traffic. Pause stops these requests. Disconnect also deletes local credentials and state, but
leaves the private repository in GitHub. The [Sync privacy page](sync-privacy.md) states the same
boundary for readers.

## Known extensions

Turning a Known extension on downloads it from the Chrome Web Store, at
`https://clients2.google.com/service/update2/crx`. Google learns which extension the reader asked
for. Settings says so before the switch is used, because that request is the one Omaweb makes to
Google and a reader should not find out afterwards.

While an extension is on, the first window opened on a new day asks the same address whether a newer
version has been published, and downloads it when there is one. The check is a day apart whatever a
reader does, so several windows still make one request. A Private window asks nothing, and a build
that cannot host an extension asks nothing.

Both requests are ordinary HTTP GETs carrying the extension's store identifier. Neither sends
browsing history, a Space identifier, or Account information. Turning the extension off stops both.
Being offline is not reported: the package on disk goes on working and the next day asks again.
