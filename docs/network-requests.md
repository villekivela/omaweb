# Automatic network requests

A first run subscribes to EasyList and EasyPrivacy with both enabled, so the first startup check
fetches both lists. Later runs read the stored subscriptions and subscribe to nothing on their own,
so removing a list stops its requests for good. Turning a subscription off in Settings also stops
its requests.

Omaweb checks enabled Content-blocking subscriptions when the browser starts. Each request goes only
to the update address shown for that subscription. It sends a normal HTTP GET with no browsing
history, Space identifier, Account information, or user-rule data. Disabling a subscription stops
its update requests. Applying a changed subscription set from Sync performs the same enabled-list
checks and may fetch a newly received or stale list from its displayed update address.

The browser sends network requests only after an explicit user or page action:

- Committing an address in the Omnibar loads that address.
- Committing non-address text searches DuckDuckGo. Omaweb sends the text as the `q` query parameter.
- Navigations started by a loaded page use the active Space's engine profile.
- Accepting a download fetches the requested file.
- Adding a Content-blocking subscription fetches its declared update address immediately.

With favicon artwork turned off, a tab's two-letter tile takes its colour from the site's favicon.
The icon is read from the icon store the engine already filled while loading the page, so no tile
ever costs a request; a site whose icon was never loaded, or whose icon carries no colour, keeps a
neutral tile.

Remote search suggestions are off. Typing in the Omnibar queries only the active Space's local
history and does not send the typed text over the network.

Omaweb opens no listening socket during an ordinary session. The `--remote-debugging[=port]` launch
option is the one exception: it binds Chromium's debugging listener to loopback, prints the address
and a warning, and disables Private windows for that launch. It is never on by default, and a
Chromium debugging switch passed to the engine by any other route refuses the launch.

Future features that add sync, remote suggestions, telemetry, or another background request must
document the destination, trigger, data sent, default state, and disable control here before
release.

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
leaves the private repository in GitHub.
