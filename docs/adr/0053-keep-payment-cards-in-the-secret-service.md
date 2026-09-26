---
status: proposed
---

# Keep payment cards in the Secret Service

Omaweb will fill payment cards ([#339](https://github.com/villekivela/omaweb/issues/339)). A card
number is the most valuable thing the browser would keep on disk, so this settles how it is kept
before the first one is. The addresses and form history before it are browser data; a card is closer
to a password, and is treated as one.

## Where a card is kept

Each saved card is one item in the desktop's Secret Service, the store `gnome-keyring` provides on
Omarchy and KWallet provides on KDE. The item's secret holds the number, the name on the card, the
expiry, and the nickname the reader gave it. Its attributes, which a keyring stores unencrypted,
hold only Omaweb's schema name and a random identifier, so nothing about the card is readable
without unlocking the keyring. Omaweb writes no card to its own data directory, not even the last
four digits; Settings reads them from the item when it lists the cards.

The keyring encrypts the items at rest under a key derived from the reader's login password, and
unlocks with the session. Omaweb adds no cryptography of its own. The core links `libsecret` for
this, which the package already depends on because the Sync module keeps its tokens and recovery key
in the same service ([0039](0039-sync-configuration-through-a-git-remote.md)); the Sync module stays
optional ([0005](0005-keep-optional-features-out-of-the-core.md)), and a build without it still has
cards.

A machine with no Secret Service has no payment cards. Settings says that the desktop offers no
secret store, and Omaweb neither offers to save a card nor falls back to a file of its own. A locked
keyring is asked to unlock when a card is needed, through the desktop's own prompt.

Considered and declined:

- **An encrypted file with its key in the Secret Service.** The key has the same trust root as the
  items, so the file protects nothing the items do not, and it adds a file format, a nonce scheme
  and a migration path to maintain.
- **A passphrase the reader sets in Omaweb.** It would keep cards from another program running as
  the reader while the keyring is unlocked, which the Secret Service does not: it has no
  per-application access control on Linux. It would do so only for the reader who types a passphrase
  at every fill, and not at all against a keylogger, which is what such a program would be. The
  friction would push readers to a weak passphrase or to no saved cards. Omaweb accepts the
  desktop's boundary here as it does for Sync's tokens.
- **The engine's own card storage.** QtWebEngine exposes no autofill storage or API for cards.

## What is never stored

The security code. A card field whose autocomplete token is `cc-csc` is never read for saving, the
fill leaves it empty for the reader to type, and form history never remembers a value typed into it
or into any field shaped like a card number
([#329](https://github.com/villekivela/omaweb/issues/329)).

## Sync

Cards never enter Sync. The README promises that passwords never do, and a card joins them. Sync's
records are encrypted, but the repository they sit in belongs to a forge, the recovery key that
opens them lives in the same Secret Service a card would come from, and a reader who loses a laptop
should not also have to wonder what their Sync repository holds. A second machine gets a card by the
reader typing it there.

Considered and declined: syncing cards as encrypted records like Spaces. It would make every card as
safe as the least careful copy of the recovery key, for the convenience of not typing a card twice.

## Filling

A card is filled only when the reader picks it from the suggestion list under a focused card field.
Picking it is the confirmation; there is no second dialog, and nothing is filled without that pick:
not on page load, not on a script's focus, and not into fields the reader did not start from. The
pick fills the number, name and expiry fields of the form the focused field belongs to, in that
field's frame only, so a payment frame from a processor gets the card and the page around it does
not.

A card is offered only in a secure context: an `https:` page whose certificate the engine accepted
without an exception. A page with a certificate error, a Local-development exception, or plain HTTP
is offered no card, and the suggestion list says why.

Site information on the page lists the fill for the rest of the page's life: which card, by its last
four digits, and the origin of the frame it went into when that is not the page's own. Nothing about
a fill is written to history or to disk.

Considered and declined: asking the desktop to re-authenticate the reader before each fill, as some
browsers do with a system password or a fingerprint. Linux has no desktop-neutral way to ask for it,
and polkit prompts are for administration, not for a browser's own data.

## Private windows and clearing

A Private window neither offers a saved card nor offers to save one it sees typed. It has no
identity to keep a card for, and a card saved from it would outlive the private session, which
[0012](0012-isolate-private-browsing-by-window.md) says nothing does.

Cards are the reader's, not a Space's, so they are offered in every Space, as addresses are
([#338](https://github.com/villekivela/omaweb/issues/338)). Settings removes a card, which deletes
its Secret Service item. Clear browsing data offers Payment cards as a category of its own, off by
default, because a card is not browsing history and clearing a day's history should not cost the
reader their cards. Removing Omaweb's data directory leaves the items in the keyring; the keyring's
own manager lists and removes them under Omaweb's schema name.

## What the documentation has to say

When #339 lands:

- The README's Privacy and security paragraph lists payment cards among what never enters Sync.
- The Sync privacy page, `website/pages/sync.html`, lists payment cards under Never synced.
- The requirements gain the filling rules above: the pick as the confirmation, secure contexts only,
  the security code never stored, Private windows excluded, and Site information naming the fill.
- The network request ledger needs no change: the Secret Service is a local D-Bus service.
