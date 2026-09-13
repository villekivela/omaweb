# Feature modules

Optional first-party capabilities live here as independent shared-library targets. The internal
module contract is versioned but is not a public third-party binary API.

Sync is the first module. Its forge adapter, secret store, and git reconciler are separate seams,
and the browser loads its plugin only after an explicit connection or an enabled marker. There is no
Account module: the forge login is Sync's identity without becoming an Omaweb account.
