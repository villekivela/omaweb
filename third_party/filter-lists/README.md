# EasyList and EasyPrivacy (snapshots)

Byte-for-byte copies of the two lists Omaweb subscribes to on a first run, pinned by
`MANIFEST.json` to the address each came from, the day it was fetched and its SHA-256.

The browser does not read these. It fetches the lists itself from https://easylist.to/. The copies
are for the measurements that need the rules a reader runs and a fixed version of them: the
page-load budget (`scripts/benchmark_runtime.py pageload`) and the cosmetic benchmark
(`scripts/benchmark_cosmetic_resources.py --lists third_party/filter-lists`).

## Licence

EasyList and EasyPrivacy are written by the EasyList authors and published under the GNU General
Public License version 3 or later, or Creative Commons Attribution-ShareAlike 3.0 Unported, at the
reader's choice. See https://easylist.to/pages/licence.html. Each file's header names its version
and the EasyList commit it was built from.

## Nothing here is edited

`ctest` fails when a copy does not match its digest or a list is here that the manifest does not
name (`omaweb-vendored-filter-lists`). `.gitattributes` keeps Git from normalising line endings or
reporting the lists' trailing whitespace, either of which would change the digest.

## Refreshing

```sh
scripts/sync_filter_lists.py --verify   # the snapshots match the manifest
scripts/sync_filter_lists.py --sync     # fetch both lists again
```

A refresh changes what the page-load budget measures. Take one on its own, and re-record the
budget's `pageload` numbers against it.
