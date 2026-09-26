# Web push research

This note records the research for #324 into what Web Push would cost on the engine Omaweb ships. It
is not an implementation contract. The engine is `omaweb-qtwebengine` 6.11.2, Qt's Chromium 140 fork
at submodule commit `5170777`, with the patch series of
[ADR 0049](../adr/0049-ship-omawebs-own-engine-build.md) applied. No patch in that series touches
the push or GCM code, so everything below holds for stock QtWebEngine 6.11.2 as well.

Source links point at QtWebEngine tag `v6.11.2` and at the Chromium commit that tag pins, so line
numbers stay valid.

## What `PushManager.subscribe` does today

In a Space it rejects at once, with no permission prompt and no network request.

QtWebEngine hands Chromium a push service only when the profile has push switched on, and it is off
by default. `ProfileQt::GetPushMessagingService` returns null otherwise.
[profile_qt.cpp](https://code.qt.io/cgit/qt/qtwebengine.git/tree/src/core/profile_qt.cpp?h=v6.11.2#n142),
[`QWebEngineProfile::isPushServiceEnabled`](https://doc.qt.io/qt-6/qwebengineprofile.html#isPushServiceEnabled)
Omaweb never sets `isPushServiceEnabled` on any Engine profile.

With no service, `PushMessagingManager::Register` answers `SERVICE_NOT_AVAILABLE` before the
notification permission is asked for, because Chromium asks for that permission inside the service.
[push_messaging_manager.cc](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/content/browser/push_messaging/push_messaging_manager.cc?id=5170777d28bee1ce92cc693a0dbf2ad01492e5cf#n303)
Blink turns that status into an `AbortError` whose message is "Registration failed - push service
not available".
[push_messaging_utils.cc](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/third_party/blink/renderer/modules/push_messaging/push_messaging_utils.cc?id=5170777d28bee1ce92cc693a0dbf2ad01492e5cf#n24)

A Private window is off the record, and there Chromium hides the missing service instead of
reporting it: it asks for the notification permission and, whatever the answer, rejects with a
permission error, so a site cannot tell a Private window apart by its push error.
[push_messaging_manager.cc](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/content/browser/push_messaging/push_messaging_manager.cc?id=5170777d28bee1ce92cc693a0dbf2ad01492e5cf#n307)

A site that feature-tests for `PushManager` finds it, because Blink exposes the interface whether or
not a service exists. It learns that push is unavailable only when `subscribe` fails.

## The endpoints push would add

The only push service in the engine is Google's. Qt's documentation for the switch says so: all push
messages go through Firebase Cloud Messaging and Google's servers.
[`QWebEngineProfile::setPushServiceEnabled`](https://doc.qt.io/qt-6/qwebengineprofile.html#setPushServiceEnabled)
Turning it on would add these requests to the [network request ledger](../network-requests.md):

| Destination                                         | When                                                                                        | What it carries                                                                                                                  |
| --------------------------------------------------- | ------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------- |
| `https://android.clients.google.com/checkin`        | When an Engine profile first holds a subscription, then at an interval Google's answer sets | A device check-in that creates and later reuses an Android ID and security token for that profile                                |
| `https://android.clients.google.com/c2dm/register3` | Each new subscription and each unsubscription                                               | The device credentials, the site's application server key as the sender, and an Instance ID for the subscribing origin           |
| `mtalk.google.com:5228`, falling back to `:443`     | For as long as a profile holds a subscription                                               | A persistent TLS connection in Google's MCS protocol, over which Google delivers each message                                    |
| `https://fcm.googleapis.com/wp/` or `/fcm/send/`    | Never contacted by Omaweb; the site's server posts to it                                    | The endpoint handed to the site is this address plus a token, so Google learns which site pushes to which installation, and when |

The check-in, registration and MCS defaults are compiled in
[gservices_settings.cc](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/google_apis/gcm/engine/gservices_settings.cc?id=5170777d28bee1ce92cc693a0dbf2ad01492e5cf#n32),
and a check-in answer may replace them with other Google addresses. The subscription endpoint is
built from the constants in
[push_messaging_constants.cc](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/chrome/browser/push_messaging/push_messaging_constants.cc?id=5170777d28bee1ce92cc693a0dbf2ad01492e5cf#n13)
by
[push_messaging_utils.cc](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/chrome/browser/push_messaging/push_messaging_utils.cc?id=5170777d28bee1ce92cc693a0dbf2ad01492e5cf#n36).

None of these is avoidable while push works. They start lazily: Chromium starts a profile's GCM
client only when the push service registers as a handler, which happens on the first subscription or
at startup for a profile that already holds one.
[gcm_driver_desktop.cc](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/components/gcm_driver/gcm_driver_desktop.cc?id=5170777d28bee1ce92cc693a0dbf2ad01492e5cf#n601),
[push_messaging_service_impl.cc](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/chrome/browser/push_messaging/push_messaging_service_impl.cc?id=5170777d28bee1ce92cc693a0dbf2ad01492e5cf#n204)
So a reader who never allows a subscription sends nothing, and one who does keeps a connection to
Google open from then on.

Each Space is its own Engine profile, and GCM state is per profile: each has its own `GCM Store`,
its own check-in identity, and its own MCS connection.
[gcm_profile_service_factory.cc](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/chrome/browser/gcm/gcm_profile_service_factory.cc?id=5170777d28bee1ce92cc693a0dbf2ad01492e5cf#n177)
Google would see one device per Space from the same address. That keeps Spaces apart in Google's
records only as far as an IP address and timing do not join them.

This contradicts two standing statements. The requirements list "no Google push service" under
[privacy and security](../product/requirements.md#privacy-and-security), and the README says Omaweb
has no push service. Any implementation over FCM needs an ADR that overturns the requirement first.

## Delivery to a Frozen tab, a Keep active tab, and an away Space

Chromium delivers a push message to the origin's service worker, not to a page. The worker is
started for the message whatever the tabs are doing, so freezing under
[ADR 0033](../adr/0033-stop-an-away-spaces-pages-instead-of-taking-them.md) does not stop delivery:
it stops pages, and a service worker is not a page. Three things in the engine and in Omaweb's shell
decide what happens after that.

**The Space's profile has to exist.** Omaweb builds a Space's Engine profile when a window first
shows that Space or when a Keep active tab in it is started, and keeps it until the Space is deleted
or the window closes (`src/ui/SpaceProfiles.qml`). A Space not shown since launch and holding no
Keep active tab has no profile, so no GCM client and no connection. Its messages wait at Google,
which holds them for the time-to-live the sending server chose, and are delivered once the profile
exists.

**The engine drops every message under Omaweb's permission policy.** Before dispatching, the push
service checks that the origin holds the notification permission, and it checks without a frame.
[push_messaging_service_impl.cc](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/chrome/browser/push_messaging/push_messaging_service_impl.cc?id=5170777d28bee1ce92cc693a0dbf2ad01492e5cf#n393)
Omaweb sets `PersistentPermissionsPolicy.AskEveryTime` so that its own store is the only place a
site's decision lives (`src/engine/qt/EngineProfile.qml`). Under that policy Qt keeps a grant in a
store keyed by the frame that asked, and the frameless check reads the persistent store, which is
empty and answers "ask".
[permission_manager_qt.cpp](https://code.qt.io/cgit/qt/qtwebengine.git/tree/src/core/permission_manager_qt.cpp?h=v6.11.2#n702)
The service treats "ask" as not granted, drops the message, and unsubscribes the site.
[push_messaging_service_impl.cc](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/chrome/browser/push_messaging/push_messaging_service_impl.cc?id=5170777d28bee1ce92cc693a0dbf2ad01492e5cf#n404)
Push delivery would need Omaweb to pre-grant notifications for each subscribed origin through
`WebEngineProfile.queryPermission`, which puts a decision back in the engine's store that
`EngineProfile.qml` exists to keep out of it.

**The shell refuses a notification no open page speaks for.**
`BrowserController::notificationTarget` lets a notification through only when a tab of the same
origin is open in the active Space, or is a Keep active tab in another Space
(`src/core/BrowserController.cpp`). The usual push case is a site the reader has closed, and that
notification is refused today.

Together, for a message that got past the permission check:

| Where the site is open                         | Worker runs | Notification shown |
| ---------------------------------------------- | ----------- | ------------------ |
| A tab in the active Space                      | Yes         | Yes                |
| A Frozen tab in the active Space               | Yes         | Yes                |
| A Keep active Pinned tab in an away Space      | Yes         | Yes                |
| A Frozen tab in an away Space                  | Yes         | No                 |
| No tab, in a Space with a profile              | Yes         | No                 |
| No tab, in a Space with no profile this launch | No, held    | No                 |

The "worker runs" column matters on its own. Chrome charges a budget for a push that shows no
notification and eventually shows one itself. Qt compiles that budget out and allows every silent
push.
[push_messaging_notification_manager.cc](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/chrome/browser/push_messaging/push_messaging_notification_manager.cc?id=5170777d28bee1ce92cc693a0dbf2ad01492e5cf#n206)
A subscribed site could wake its worker in an away Space and run script there as often as it pushes,
with nothing shown. That is the background activity ADR 0033 stops pages from doing.

Subscriptions also do not survive a restart. When a profile is destroyed, Qt empties Chromium's
table of subscriptions, deliberately, because Qt does not persist notification permissions either.
[profile_qt.cpp](https://code.qt.io/cgit/qt/qtwebengine.git/tree/src/core/profile_qt.cpp?h=v6.11.2#n78)
The emptied table reaches disk when the preference store is destroyed. On the next launch a message
for the old subscription finds no owner, and Chromium drops it and unsubscribes. A site's server
keeps a dead endpoint until its next send fails, and the page has to subscribe again after every
launch.

## A hook for a self-hosted or non-Google endpoint

There is none in QtWebEngine's API. The only control is the on/off switch.

Chromium reads three command-line switches that move the check-in, registration and MCS addresses:
`--gcm-checkin-url`, `--gcm-registration-url` and `--gcm-mcs-endpoint`.
[gservices_switches.cc](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/google_apis/gcm/engine/gservices_switches.cc?id=5170777d28bee1ce92cc693a0dbf2ad01492e5cf#n11)
They exist for testing. A server behind them must speak Google's private check-in and MCS protocols,
for which there is no published specification and no maintained server implementation. The switches
also leave the subscription endpoint alone: the page is still handed an `fcm.googleapis.com`
address, so a site's server would post to Google whatever the browser connects to. Each switch would
also have to pass the engine flag audit of
[ADR 0034](../adr/0034-audit-the-engine-flags-omaweb-adds-itself.md).

A standards-based self-hosted path is possible only as engine work. Mozilla's autopush speaks the
open Web Push protocol of [RFC 8030](https://www.rfc-editor.org/rfc/rfc8030) to sites and a
WebSocket protocol to Firefox, and its server is open source.
[autopush-rs](https://github.com/mozilla-services/autopush-rs) Using it would mean a new
`PushMessagingService` implementation in the patch series in place of Chromium's GCM one, and an
autopush deployment someone runs for every Omaweb reader. The README rules out a hosted Omaweb
service, and most readers would not run a push server of their own. UnifiedPush is the nearest
reader-run design. It defines a distributor on the device, with a D-Bus binding for Linux, but no
browser delivers Web Push through it, so there is no browser integration to borrow.
[UnifiedPush specification](https://unifiedpush.org/developers/spec/)

## Recommendation

Decline.

The engine's one push path runs every subscription through Google, which the requirements forbid,
and Qt offers no way to point it elsewhere. Reaching a push server Omaweb or its readers control
means replacing Chromium's push service in the engine patch series and operating a server, and the
README rules out a hosted Omaweb service. Even the FCM path would not work as shipped: Omaweb's
permission policy drops every message, the shell refuses the notification for a closed site, which
is the case push exists for, subscriptions end with each launch, and Qt lets a site run its worker
in an away Space on every silent push. Readers keep what Omaweb has today: a site open in the active
Space, or in a Keep active Pinned tab, can notify while it runs.

Revisit this if QtWebEngine gains a push service that takes an embedder-supplied endpoint, or if a
reader-run standard such as UnifiedPush gains a desktop browser binding.
