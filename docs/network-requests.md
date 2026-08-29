# Automatic network requests

Tanto checks enabled Content-blocking subscriptions when the browser starts. Each request goes
only to the update address shown for that subscription. It sends a normal HTTP GET with no browsing
history, Space identifier, Account information, or user-rule data. Disabling a subscription stops
its update requests.

The browser sends network requests only after an explicit user or page action:

- Committing an address in the Omnibar loads that address.
- Committing non-address text searches DuckDuckGo. Tanto sends the text as the `q` query parameter.
- Navigations started by a loaded page use the active Space's engine profile.
- Accepting a download fetches the requested file.
- Adding a Content-blocking subscription fetches its declared update address immediately.

Remote search suggestions are off. Typing in the Omnibar queries only the active Space's local history and does not send the typed text over the network.

Future features that add sync, remote suggestions, telemetry, or another background request must
document the destination, trigger, data sent, default state, and disable control here before release.
