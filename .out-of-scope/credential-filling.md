# Credential filling

Omaweb does not fill credentials into pages, does not read the reader's password manager, and does
not carry passkeys. A reader signs in by whatever means they already use outside the browser.

## Why this is out of scope

The daily-driver non-goals list Password management, and this stays inside that non-goal. What
follows is why the narrower reading, filling a credential Omaweb never stores, was considered and
declined.

### An external picker already does it, minus one property

On the desktop Omaweb targets, a launcher wrapping `rbw`, `bw`, or `op` already picks an entry and
puts it in the focused field. Against that, a first-party fill adds exactly one thing: the browser
knows the origin in the address bar, so it can refuse an entry saved for a different site. A
compositor-level typer cannot, because it has no idea what page is on show.

That property is real and it is the only one. Unlocking, the agent holding the unlock, choosing
between several accounts on one site, and keeping the secret off disk are all things the external
tool does as well.

Buying one property costs a Feature module, an ADR permitting a secret into the page's world, an
engine-neutral seam call, a capability bit, a glossary term, a configuration format, and a Settings
surface. The target engine cannot implement the seam, so the feature ships disabled the day Omaweb
moves to it.

### Form detection is the expensive part, and it does not converge

Deciding which field takes the password is not a heuristic that can be written once. Firefox runs a
Fathom machine-learning model with 44 weighted signals and a 0.75 confidence cutoff to decide
whether a password field is a new password or a current one, a backwards scan from the first
password field with a three-tier precedence for the username, and a separate code path for
username-first multi-step forms that is throttled to five forms per document because the inference
is too expensive to run more often. Old, new, and confirmation passwords are told apart by comparing
the typed values at submission.

After fifteen years of that, Firefox still matches a `display:none` input as the username field,
still has no distance limit between the username and password fields, still does not support Shadow
DOM, and still fills only the first of several formless sign-in forms on one page. Mozilla's own
documentation states that many fields cannot be placed into a definite type.

Filling only the focused field avoids all of this, and is also what the external picker does.

## Passkeys have no mechanism, not merely no ticket

Passkeys were the capability that made this worth reconsidering. On Linux there is no route to them
that does not run through a vendor's own browser extension.

- Bitwarden and 1Password both provide passkeys by replacing `navigator.credentials` from an
  extension content script. Their native messaging channels are restricted to their own extension
  identifiers.
- No command-line tool serves one. `rbw` states that WebAuthn and passkeys are unsupported, `bw` has
  no passkey object or command, and `op` has no passkey field type. `bw serve` exposes items, TOTP,
  send, generate, and sync, and no fido2 route.
- QtWebEngine implements WebAuthn from 6.7, with no default interface and, on Linux, one transport:
  a USB security key. There is no platform authenticator and no hybrid transport. Qt records that
  only YubiKeys are tested.
- Ladybird implements no WebAuthn. Every `CredentialsContainer` method rejects, and two pull
  requests adding them were closed unmerged.

One-time codes are the exception and work headlessly today through all three tools.

## Hosting a vendor's extension instead

Qt 6.10 added `QWebEngineExtensionManager`, so the engine can load a Manifest V3 extension. The API
surface behind it is Chromium's core extensions layer plus a `tabs` schema defining only
`chrome.tabs.update`. There is no `tabs.query`, no `windows`, no `action`, no `contextMenus`, and
native messaging is unimplemented and refuses. There is no toolbar, and an embedder renders an
extension's popup itself. uBlock Origin does not work and Privacy Badger does not load.

A password manager's extension needs `tabs` queries and events, popup hosting, context menus, and
native messaging for desktop unlock. Adopting extensions for this would also contradict
[ADR 0001](../docs/adr/0001-isolate-web-engines-by-build.md), which withholds them until both
engines support the same system. Ladybird's maintainers state that the WebExtension APIs and every
version of the manifest specification are out of scope for a long while, so that condition is
further off than when the decision was taken.

## What would reopen this

- Bitwarden or 1Password shipping a Linux platform passkey provider that a browser can call without
  hosting their extension.
- Native messaging landing in QtWebEngine, together with enough of the `tabs` and `action` surface
  for a manager's extension to run.

## Prior requests

- #272: "Fill credentials from the reader's own password manager"
