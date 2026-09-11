# Building the GitHub Sync adapter

Sync is Linux-only and requires libsodium, libsecret, and git. Register the project GitHub App with
device flow enabled, expiring user tokens enabled, repository Administration write permission, and
repository Contents read/write permission. No webhook or callback URL is required by the browser's
device flow. Build with its public identifiers:

```sh
cmake --preset dev \
  -DOMAWEB_GITHUB_APP_CLIENT_ID=the_public_client_id \
  -DOMAWEB_GITHUB_APP_SLUG=the_public_app_slug
```

The client ID and slug are public application identifiers, not secrets. Never compile a client
secret or private key into the browser. A build without a client ID still builds and runs, but its
Sync pane explains that GitHub authorization is unavailable.

The first connection creates a private repository. A later machine logs into the same GitHub
identity and enters the recovery key saved from the first machine. Losing every copy of that key
makes the encrypted Space and tab records unrecoverable. Disconnecting does not remove the GitHub
repository.
