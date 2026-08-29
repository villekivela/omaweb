# Automatic network requests

Tanto makes no automatic network requests in the current build.

The browser sends network requests only after an explicit user or page action:

- Committing an address in the Omnibar loads that address.
- Committing non-address text searches DuckDuckGo. Tanto sends the text as the `q` query parameter.
- Navigations started by a loaded page use the active Space's engine profile.
- Accepting a download fetches the requested file.

Remote search suggestions are off. Typing in the Omnibar queries only the active Space's local history and does not send the typed text over the network.

Future features that add update checks, filter-list subscriptions, sync, remote suggestions, telemetry, or another background request must document the destination, trigger, data sent, default state, and disable control here before release.
