# Browser account and Sync interface research

This note compares how current desktop browsers expose a signed-in identity and Sync. It informs
[issue #173](https://github.com/villekivela/omaweb/issues/173) but is not an implementation
contract. The evidence was checked on 2026-09-12. It uses first-party support material and upstream
source rather than third-party screenshots.

## Comparison

| Browser             | Persistent entry point                                                    | Identity treatment                                                                                           | Sync state and recovery                                                                              | Settings hierarchy                                                                             |
| ------------------- | ------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------ | ---------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------- |
| Chrome and Chromium | Profile control at the top right                                          | Account image when available, otherwise a profile avatar; Chromium crops the image to a circle               | Dotted avatar ring, highlight, tooltip, and sometimes an expanded action label for actionable errors | Account control precedes Sync controls in the account or Sync page                             |
| Firefox             | Account icon in the toolbar or application menu                           | Personalized avatar when set, username initial when signed in, generic silhouette or no icon when signed out | Blue dot for verification, explicit menu items such as `Sync now` or `Turn On Syncing...`            | Dedicated Account and Sync section, with account state before synced-data controls             |
| Edge                | Profile icon at the top right                                             | Account image, default avatar, or initials                                                                   | Profile menu is the entry point; Sync controls and sign-out are explicit settings actions            | `Settings > Profiles > Sync`                                                                   |
| Brave               | Main menu, then Brave Sync                                                | No Sync account or account avatar                                                                            | Named Sync chain, device list, recovery code, and explicit add/remove-device actions                 | Dedicated Sync page under Settings                                                             |
| Vivaldi             | Sync button in the status bar, Synced Tabs button, or User Profile button | Connected account username and profile image replace the local profile identity                              | Sync page exposes login, encryption password or key, item selection, and recovery actions            | `Settings > Sync`, with account information before the backup encryption key and data controls |

## Chrome and Chromium

Chrome documents the Profile control at the top right. Chromium treats that toolbar avatar as a
stable identity surface and layers transient status onto it. Current code selects a Google Account
image when one is available and ultimately crops the result to a circle. For actionable Sync errors,
it draws a dotted ring around the avatar and supplies a tooltip that combines the short profile
identity with an error description.
[Chrome sign-in guide](https://support.google.com/chrome/answer/185277),
[avatar state source](https://chromium.googlesource.com/chromium/src/+/main/chrome/browser/ui/views/profiles/avatar_toolbar_button_state_manager.cc)

A pending sign-in has collapsed and expanded forms. Both keep the smaller avatar inside a dotted
circle. The expanded form says `Verify it's you`; it appears for errors other than an immediate web
sign-out, after a delay, or after Chrome restarts. This preserves identity while making recovery
actionable instead of representing the problem only through image opacity.
[pending-sign-in source](https://chromium.googlesource.com/chromium/src/+/main/chrome/browser/ui/views/profiles/avatar_toolbar_button_state_manager.cc#1801)

Chrome's Settings groups the feature under `You and Google > Sync and Google services`, then offers
`Manage what you sync`. The current Chromium Settings tree also keeps a dedicated account page and
places its Sync account control before the lower-level Sync controls.
[Chrome Settings path](https://support.google.com/chrome/a/answer/16465307),
[Chromium account page](https://chromium.googlesource.com/chromium/src/+/main/chrome/browser/resources/settings/people_page/account_page.html)

## Firefox

Firefox uses its Mozilla account icon as both identity and Sync entry point. A signed-in reader sees
either their personalized avatar or the first letter of their username. A signed-out reader sees a
generic silhouette or no account icon. Opening the control reveals the Sync state and either
`Sync now` or `Turn On Syncing...`. A blue dot marks an account that still needs email verification.
[Mozilla Sync status](https://support.mozilla.org/en-US/kb/how-do-i-know-status-my-sync)

Firefox also puts the account before the data choices in Settings. Its support flow leads to an
`Account and sync` section, then a Sync section and `Manage synced data`. Upstream strings use
explicit headings such as `Data synced across devices`, and its controls name the data rather than
relying on a single undifferentiated switch.
[Mozilla setup flow](https://support.mozilla.org/en-US/kb/how-do-i-set-sync-my-computer),
[Firefox Settings strings](https://searchfox.org/mozilla-central/source/browser/locales/en-US/browser/preferences/preferences.ftl)

Firefox remains signed in within Private Browsing, but its support documentation explicitly says
that Private Browsing history is not recorded and therefore is not added to or synchronized from the
Mozilla account. This is useful evidence for stating the boundary, although Omaweb has chosen a
stricter rule and shows neither identity nor Sync in a Private window.
[Mozilla private-browsing note](https://support.mozilla.org/en-US/kb/how-do-i-know-status-my-sync#why-am-i-signed-in-to-my-mozilla-account-in-private-browsing)

## Edge

Edge places a profile icon in the top-right corner. Microsoft says it may contain an account image,
a default avatar, or account initials. Opening it handles sign-in and account switching. Settings
then separates the hierarchy as `Profiles > Sync`, where the reader chooses individual data types.
[Microsoft Edge Sync guide](https://support.microsoft.com/en-us/edge/sign-in-to-sync-microsoft-edge-across-devices)

Microsoft's screen-reader instructions confirm that the same control is keyboard reachable and
announced as a named, collapsed profile button. They also define a complete keyboard route through
the Profiles category to the Sync button and its individual controls. This makes the parent
control's semantic name and expanded state part of the feature, not optional metadata.
[Microsoft Edge accessibility guide](https://support.microsoft.com/en-US/accessibility/edge/use-a-screen-reader-to-sign-in-to-microsoft-edge)

Microsoft also warns beside destructive Sync behavior that deleting data while signed in removes it
from every synchronized device. The reusable pattern is to put consequences next to the action, not
only in general privacy prose elsewhere on the page.
[Microsoft Edge Sync guide](https://support.microsoft.com/en-us/edge/sign-in-to-sync-microsoft-edge-across-devices)

## Brave

Brave is the useful counterexample to account-led Sync. Its official documentation says there is no
Brave Sync account. It names the shared identity a `Sync chain`, encrypts it client-side, and uses a
recovery code rather than an account avatar. The main Sync page lists participating devices and puts
`Categories`, `Add another device`, and `Remove this device` together.
[Brave Sync FAQ](https://support.brave.app/hc/en-us/articles/360047642371-Sync-FAQ),
[Brave Sync setup](https://support.brave.app/hc/en-us/articles/360021218111-How-do-I-set-up-Sync)

This supports describing the real identity model rather than inventing a browser-owned Account.
Omaweb differs because its provider login is a useful human identity, but the same distinction
applies: a GitHub login identifies the connected provider account, not an Omaweb account or a Space.

## Vivaldi

Vivaldi offers several routes into the same Sync settings: a status-bar Sync button, a Synced Tabs
button, `Settings > Sync`, and the User Profile button at the right of the address bar. Its page
asks for a Vivaldi account username first, then a separate encryption password. Once signed in,
backup encryption-key controls appear below the account information, before the detailed Sync
actions. [Vivaldi Sync guide](https://help.vivaldi.com/desktop/tools/sync/)

The toolbar identity changes when Sync is connected: Vivaldi says the account username and profile
image replace the local profile identity. That is direct precedent for Omaweb replacing its gear
with the provider avatar once an Account is connected.
[Vivaldi User Profiles](https://help.vivaldi.com/desktop/tools/user-profiles/)

The page explicitly warns that Sync is not a backup service and says what losing the encryption
password or every local copy means. Its troubleshooting guide asks for an error message, timestamp,
and Sync diagnostics when reconciliation fails. Those are useful patterns for placing recovery
information and diagnostics near account state.
[Vivaldi Sync guide](https://help.vivaldi.com/desktop/tools/sync/),
[Vivaldi Sync troubleshooting](https://help.vivaldi.com/desktop/troubleshoot/troubleshoot-sync-issues/)

## Direction for Omaweb

The outline control should be one opaque circular identity surface. It should show the cached
provider avatar when available and a monogram inside the same circle while the image is missing or
loading. The fallback must replace the image rather than remain visible underneath it.

Pausing may desaturate the avatar because it expresses an intentional inactive state without
suggesting identity loss. Grayscale must not be the only signal. Keep the status dot, give it a
distinct paused treatment, and include `Sync paused` in the control's accessible name. Do not lower
the avatar opacity. For a failure that needs action, preserve the avatar and add the urgent dot or
ring plus explicit status text in Settings. Chromium and Firefox both preserve identity and add a
separate state mark.

Put a prominent Account card first in the Sync section. A 56 to 64 pixel circular avatar is large
enough to anchor the page without competing with the section title. The card should contain:

- the provider login as the primary label;
- a provider-specific secondary label, such as `GitHub account`;
- an explicit state, such as `Sync is on`, `Sync is paused`, or `Sync needs attention`; and
- the most useful next action only when action is required.

Keep provider and state as separate fields in the interface even when they share one visual line.
That permits later Forgejo or Gitea adapters to supply their own provider name without changing the
layout or calling the identity an Omaweb account. The page title remains `Sync`; `Account` refers
only to the optional provider identity defined by Omaweb's domain model.

Follow the Account card with the privacy boundary, error or recovery notice, synchronized-data
choices, and finally pause, disconnect, or destructive actions. Put consequences beside destructive
actions. The general privacy statement should continue to say that passwords, cookies, browsing
history, and all Private-window information are never synchronized.

Private windows should keep the stricter existing contract: show neither the avatar nor monogram, do
not load provider identity, and present only the explanation that Sync is unavailable. A Space must
never be represented as the Account, even though Chrome and Edge combine browser profiles and remote
identities more closely.

The avatar control needs a full accessible name that includes the login, state, and destination, for
example `Settings for villekivela, Sync paused`. Status must not depend on color, saturation, or the
image itself. The monogram and decorative dot should be hidden from accessibility APIs when the
parent control already supplies that combined name.

## Evidence limits

Support pages can lag a browser release, and several replace icons with generic `Image` placeholders
in extracted text. Shape and error-treatment claims therefore rely primarily on current Chromium
source. Mozilla's documentation establishes avatar, initial, silhouette, and blue-dot states, but
not exact dimensions or crop geometry. Microsoft establishes placement and fallback content, but its
public support page does not fully specify error badges. Brave's documentation establishes that Sync
has no Account; it should not be used as evidence for account-avatar presentation. Vivaldi's
documentation establishes placement and settings order but does not document avatar shape or an
inactive visual state.
