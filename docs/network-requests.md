# Automatic network requests

A first run subscribes to EasyList and EasyPrivacy with both enabled, so the first startup check
fetches both lists. Turning a subscription off in Settings stops its requests.

Omaweb checks enabled Content-blocking subscriptions when the browser starts. Each request goes only
to the update address shown for that subscription. It sends a normal HTTP GET with no browsing
history, Space identifier, Account information, or user-rule data. Disabling a subscription stops
its update requests.

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
