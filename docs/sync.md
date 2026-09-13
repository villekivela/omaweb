# Building the GitHub Sync adapter

Sync is Linux-only and requires libsodium, libsecret, and git. Official builds use the public client
ID and slug for the Omaweb Sync GitHub App. Register a separate development App with device flow
enabled, expiring user tokens enabled, repository Administration write permission, and repository
Contents read/write permission. No webhook or callback URL is required by the browser's device flow.
Leave "Request user authorization (OAuth) during installation" disabled because Omaweb owns that
step through device flow. Override the built-in public identifiers when testing that App:

```sh
cmake --preset dev \
  -DOMAWEB_GITHUB_APP_CLIENT_ID=the_public_client_id \
  -DOMAWEB_GITHUB_APP_SLUG=the_public_app_slug
```

The client ID and slug are public application identifiers, not secrets. Never compile a client
secret or private key into the browser. A build without a client ID still builds and runs, but its
Sync pane explains that GitHub authorization is unavailable.

Connect opens GitHub authorization. On the first device, Omaweb then opens GitHub's repository form
with the `omaweb-sync` name, description, owner, and Private visibility prefilled. GitHub does not
allow an uninstalled App to create a personal repository, and its installation form does not allow
zero selected repositories, so the reader confirms repository creation and returns to Sync. Omaweb
then opens App installation; choose "Only select repositories" and select only `omaweb-sync`. Omaweb
detects installation automatically, starts Sync, and shows the recovery key. A later machine logs
into the same GitHub identity and enters the saved recovery key. Losing every copy of that key makes
the encrypted Space and tab records unrecoverable. Disconnecting does not remove the GitHub
repository.
