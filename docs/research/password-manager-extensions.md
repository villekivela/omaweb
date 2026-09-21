# Password-manager Known extension research

This note records what QtWebEngine 6.11.1 lacks for hosting Bitwarden and 1Password as Known
extensions on the Development engine. It answers
[issue #344](https://github.com/villekivela/omaweb/issues/344) with facts and buckets and makes no
go or no-go decision. The evidence was read on 2026-09-17 from the sources below. Every claim cites
a file, and a line number where one helps.

The patches this note reports are not in this repository. They are Chromium and Qt source under
their own licences, and an engine belongs outside Omaweb's tree, so they live in
[`omaweb-qtwebengine-patches`](https://github.com/villekivela/omaweb-qtwebengine-patches), on top of
the 6.11.1 tarball. The harness that measured all of this is
[`omaweb-extension-probe`](https://github.com/villekivela/omaweb-extension-probe). The series:

1. `extensions: bind EventRouter and RendererHost at every registration point`
2. `build: define the pure-computation seatbelt profile name ourselves` (a local SDK fix, not part
   of the series)
3. `extensions: keep ExtensionPrefs valid when the PrefService is rebuilt`
4. `extensions: offer the tabs, windows, permissions and event namespaces`
5. `extensions: offer the scripting namespace`
6. `extensions: connect an extension to a native messaging host`

## Sources

- `qt/`: the qtwebengine tag `v6.11.1` tarball (`.cmake.conf` sets `QT_REPO_MODULE_VERSION` to
  `6.11.1`).
- `fork/`: the Qt Chromium fork at commit `58c11ad487f8a237cf0ac71cc3e818b52db150df` on `140-based`,
  read file by file. Paths below are relative to its `chromium/` directory.
- `bw/`: Bitwarden `clients` at commit `51c775bd` (browser extension 2026.9.0, from
  `apps/browser/src/manifest.v3.json`).
- `op/`: the 1Password CRX manifest, version 8.12.37.1, and the scripts unpacked from the same CRX.
- Upstream Chromium tag `140.0.7339.80` only where a file the fork's build lists is absent from the
  fork itself. Those citations are marked `upstream/`.

## Buckets

The three buckets settled on #344:

- **compile with delegate**: Chromium's own `chrome/browser/extensions/api/<x>` files compile into
  QtWebEngine with their browser hooks satisfied by a delegate Qt implements against
  `WebContentsAdapter` and `ProfileAdapter`.
- **rewrite behind delegate**: Qt writes its own schema and implementation behind an embedder
  delegate, as it did for `tabs.update` in `fork/qtwebengine/common/extensions/api/tabs.json` and
  `fork/qtwebengine/browser/extensions/api/tabs/`.
- **stop**: either route needs `Browser`, `TabStripModel`, `Profile`, or anything under
  `chrome/browser/ui`.

One reading rule applies throughout. Qt compiles `chrome/browser/profiles/profile.cc` and derives
`ProfileQt` from that `Profile` (`qt/src/core/configure/BUILD.root.gn.in:342-343`,
`qt/src/core/profile_qt.h:24`), and `ExtensionsBrowserClientQt::GetPrefServiceForContext` already
answers through `Profile::GetPrefs` (`qt/src/core/extensions/extensions_browser_client_qt.cpp:416`).
A file that uses `Profile` only as `Profile::FromBrowserContext(context)->GetPrefs()` or
`->GetPath()` therefore compiles against what Qt has. This note counts such a file as
compile-with-delegate and says so where it matters. A file that needs `ProfileManager`, profile
keyed services that Qt does not build, `Browser`, `TabStripModel`, or `chrome/browser/ui` is a stop
on that route.

## How Qt 6.11.1 assembles its extension surface

These facts were established by an earlier pass and are re-verified here by reading.

- Schemas. `fork/chrome/common/extensions/api/api_sources.gni:167-175` replaces the whole
  chrome-layer schema list under `is_qtwebengine` with `enterprise_hardware_platform.idl` and
  `webrtc_logging_private.idl`, and empties `uncompiled_sources_` (which upstream holds
  `action.json`). Qt's own list is `pdf_viewer_private.idl`, `resources_private.idl`, `tabs.json`,
  and `webrtc_desktop_capture_private.idl`
  (`fork/qtwebengine/common/extensions/api/schema.gni:1-6`). The core list in
  `fork/extensions/common/api/schema.gni:14-34` includes `runtime.json`, `storage.json`, `alarms`,
  `idle.json`, `management.json`, `offscreen.idl`, `web_request.json`,
  `declarative_net_request.idl`, and `user_scripts.idl`.
- Qt's `tabs.json` defines the `Tab` type and one function, `update`
  (`fork/qtwebengine/common/extensions/api/tabs.json:35-110`). Qt's implementation ignores the
  `tabId` argument and navigates the sender's own `WebContents`
  (`fork/qtwebengine/browser/extensions/api/tabs/tabs_api.cc:105-128`).
- API features. At runtime the API feature provider is built by calling
  `ExtensionsAPIProvider::AddAPIFeatures` on each provider
  (`fork/extensions/common/extensions_client.cc:52-72`). Qt's provider calls `AddQtAPIFeatures`,
  generated from `extensions/common/api/_webengine_api_features.json` only
  (`qt/src/core/common/extensions/extensions_api_provider_qt.cpp:39-42`,
  `qt/src/core/configure/BUILD.root.gn.in:810-816`). That file lists `tabs`,
  `enterprise.hardwarePlatform`, `pdfViewerPrivate`, `resourcesPrivate`,
  `webrtcDesktopCapturePrivate`, `webrtcLoggingPrivate`, and `mimeHandlerViewGuestInternal`
  (`fork/extensions/common/api/_webengine_api_features.json`). Chrome's `_api_features.json` is
  loaded by `AddAPIJSONSources` (`extensions_api_provider_qt.cpp:44-47`), but the only consumer of
  that source is `ExtensionsClient::CreateAPIFeatureSource`
  (`fork/extensions/common/extensions_client.cc:75-80`), which only
  `fork/extensions/renderer/test_features_native_handler.cc:28` calls, for `chrome.test`. So
  `windows`, `action`, `commands`, `contextMenus`, `notifications`, `privacy`, `webNavigation`,
  `permissions`, `scripting`, and `sidePanel` are not API features in a 6.11.1 renderer at all.
- Permission features. Qt generates `AddQtPermissionFeatures` from both
  `chrome/common/extensions/api/_permission_features.json` and
  `extensions/common/api/_permission_features.json` (`BUILD.root.gn.in:817-823`), and registers
  Chrome's permission IDs (`extensions_api_provider_qt.cpp:75-80`). Every permission both manifests
  declare is therefore recognised at parse time; the manifests load.
- Manifest handlers. Qt registers none of its own (`extensions_api_provider_qt.cpp:33-35`). The core
  handler list includes `CommandsHandler` and `ExtensionActionHandler`
  (`fork/extensions/common/common_manifest_handlers.cc:11,26`), so the `commands` and `action`
  manifest keys parse.
- Browser hooks. `ExtensionsBrowserClientQt` answers `HasOffTheRecordContext` false,
  `IsExtensionIncognitoEnabled` false, `CanExtensionCrossIncognito` false, and returns no
  `ProcessManagerDelegate`
  (`qt/src/core/extensions/extensions_browser_client_qt.cpp:287-296, 332-341, 426-429`). It does not
  override the core tab hooks `GetTabAndWindowIdForWebContents`, `IsValidTabId`, or
  `GetScriptExecutorForTab`, whose base implementations return `-1`, `false`, and `nullptr`
  (`fork/extensions/browser/extensions_browser_client.cc:80-86, 136-147`).
- Messaging. `MessagingDelegateQt` overrides only `MaybeGetTabInfo`, returning `std::nullopt`
  (`qt/src/core/extensions/messaging_delegate_qt.cpp`). The base delegate returns `DISALLOW` from
  `IsNativeMessagingHostAllowed`, `nullptr` from `GetWebContentsByTabId`, and `nullptr` from
  `CreateReceiverForNativeApp`
  (`fork/extensions/browser/api/messaging/messaging_delegate.cc:13-45`).
  `MessageService::OpenChannelToNativeAppImpl` turns `DISALLOW` into the disconnect message "Access
  to the native messaging host was disabled by the system administrator."
  (`fork/extensions/browser/api/messaging/message_service.cc:88-90, 691-697`).
- Popups. `ExtensionManager::actionPopupUrl` reads the popup URL from the core
  `ExtensionActionManager` (`qt/src/core/extensions/extension_manager.cpp:129-138`) and exposes it
  as `QWebEngineExtensionInfo::actionPopupUrl` (`qt/src/core/api/qwebengineextensioninfo.cpp:184`).
  The embedder loads it into an ordinary view
  (`qt/tests/manual/widgets/extensions/main.cpp:242-254`).
- Off-the-record. `ExtensionLoader` accepts manifest version 3 only and refuses off-the-record
  contexts with "Can't load in off-the-record mode"
  (`qt/src/core/extensions/extension_loader.cpp:24, 63, 74-75`).
- Service workers run: `qt/tests/auto/widgets/extensions/tst_qwebengineextension.cpp:43, 390-394`
  loads `service_worker_ext` and round-trips a message.
- Network. `ContentBrowserClientQt::WillCreateURLLoaderFactory` installs
  `ProxyingURLLoaderFactoryQt` and never calls `WebRequestAPI::MaybeProxyURLLoaderFactory`
  (`qt/src/core/content_browser_client_qt.cpp:1280-1327`). The app_shell embedder shows the call a
  delegate would add (`fork/extensions/shell/browser/shell_content_browser_client.cc:283-307`).
- Fork pruning. The fork's `chrome/browser/extensions/` tree contains only `BUILD.gn`, `api/`,
  `default_extensions/`, and `keyed_services/`
  ([tree at the commit](https://github.com/qt/qtwebengine-chromium/tree/58c11ad487f8a237cf0ac71cc3e818b52db150df/chromium/chrome/browser/extensions)).
  `extension_tab_util.*`, `menu_manager.*`, `window_controller.*`, `commands/command_service.*`,
  `chrome_extension_function_details.*`, `extension_install_prompt.*`, `pref_mapping.*`, and
  `chrome/browser/ui/browser.h`, `chrome/browser/ui/tabs/tab_strip_model.h`,
  `chrome/browser/profiles/profile_manager.h`, `chrome/browser/browser_process.h` all answer 404
  from the raw endpoint, although `fork/chrome/browser/extensions/BUILD.gn` still lists them (lines
  169-175, 238-239, 322-323, 403-406, 494-498). Qt depends only on the `api:api_registration`
  target, whose deps it strips under `is_qtwebengine`
  (`fork/chrome/browser/extensions/api/BUILD.gn:148-159`). Any compile-with-delegate route therefore
  begins by importing files from upstream, not by switching a build flag.

## The empty popup and the FATAL hypothesis

The earlier pass proposed that a chrome-layer namespace whose feature loaded but whose schema did
not compile reaches `LOG_IF(FATAL, !schema) << "Unknown API"` in the renderer
(`fork/extensions/renderer/native_extension_bindings_system.cc:210-216`), killing the popup's
renderer and leaving an empty surface. The path is real: `ExtensionAPI::GetSchema` returns `nullptr`
when the provider's schema string is empty (`fork/extensions/common/extension_api.cc:241-259`), and
the bindings system reaches it from the lazy accessor for any API name it has set on `chrome`
(`native_extension_bindings_system.cc:586-593, 841-870`).

Reading the feature assembly above, the condition does not hold for the namespaces these two
extensions touch. `FeatureCache` sets an accessor only for features the API feature provider holds
and finds available to the context (`fork/extensions/renderer/feature_cache.cc:133-176`), and
Chrome's `_api_features.json` is not part of that provider in Qt. `chrome.windows`, `chrome.action`,
and the rest are `undefined`; `chrome.tabs` exists with `update` only, so `chrome.tabs.query` is
`undefined`. The likelier mechanism for the empty popup is a JavaScript `TypeError` early in the
popup's bootstrap. Bitwarden's `BrowserApi.getCurrentWindow` calls `chrome.windows.getCurrent`
unguarded (`bw/apps/browser/src/platform/browser/browser-api.ts:136-138`) and `permissionsGranted`
calls `chrome.permissions.contains` unguarded (`browser-api.ts:840-846`).

Both readings put the same first step on the prototype: open the popup with DevTools attached and
read the console and the renderer log before touching any API.

## Per-API findings

Each entry names who calls it, what Chromium's implementation needs, what it needs from Omaweb's
model rather than from Chromium's browser services, and the bucket.

### tabs

Bitwarden calls `query`, `get`, `getCurrent`, `create`, `update`, `remove`, `sendMessage`,
`captureVisibleTab`, and listens to `onUpdated`, `onRemoved`, `onActivated`, `onCreated`, and
`onReplaced` (`bw/.../browser-api.ts:261-431, 601, 910`; counts across `apps/browser/src`).
1Password calls `query` (5), `get` (3), `create` (29), `update` (11), `remove` (9), `sendMessage`
(21), `captureVisibleTab` (1) and listens to `onUpdated`, `onActivated`, `onRemoved`
(`op/background/background.js` and `op/chunks/*.js`).

Qt 6.11.1 status: `update` only, acting on the sender
(`fork/qtwebengine/browser/extensions/api/tabs/tabs_api.cc:113`).

Chromium's implementation: `fork/chrome/browser/extensions/api/tabs/tabs_api_non_android.cc:41-97`
includes `chrome/browser/ui/browser.h`, `browser_finder.h`, `browser_list.h`, `browser_navigator.h`,
`browser_window.h`, `chrome/browser/ui/tabs/tab_strip_model.h`, `extension_service.h`,
`extension_tab_util.h`, and `chrome/browser/web_applications/*`. The shared `tabs_api.cc:11-30`
needs `extension_tab_util.h`, `window_controller.h`,
`chrome/browser/ui/browser_window/public/browser_window_interface.h`, and
`chrome/browser/ui/tabs/tab_list_interface.h`. `tabs_event_router.cc:26-33` is a
`TabStripModelObserver`. `windows_util.cc:12-19` needs `extension_tab_util.h`,
`window_controller_list.h`, and `chrome/browser/ui/browser_navigator.h`.

`tabs.sendMessage` is different. Its renderer half is
`fork/chrome/renderer/extensions/api/tabs_hooks_delegate.cc:28-29, 108-109`, which is present in the
fork and has no browser dependency; Qt registers no hooks delegate
(`qt/src/core/renderer/extensions/extensions_renderer_api_provider_qt.h:24-26`). Its browser half is
core: `MessageService::OpenChannelToTabImpl` asks `MessagingDelegate::GetWebContentsByTabId` and
disconnects with "Receiving end does not exist" when that returns `nullptr`
(`fork/extensions/browser/api/messaging/message_service.cc:743-778`).

From Omaweb's model: a stable integer tab id per `WebContents`, the window id, the active tab of a
window, tab creation and closing, and tab-state change notifications. From Chromium's services:
nothing that the core hooks do not already abstract (`GetTabAndWindowIdForWebContents`,
`IsValidTabId`, `GetWebContentsByTabId`). `captureVisibleTab` needs only
`RenderWidgetHostView::CopyFromSurface` on the active `WebContents`.

Bucket: **rewrite behind delegate**. The compile route is a stop (`Browser`, `TabStripModel`,
`chrome/browser/ui`). Qt's existing `tabs.json` and `tabs/tabs_api.cc` are the pattern.

### windows

Bitwarden calls `getAll`, `get`, `getCurrent`, `create`, `update`, `remove` and listens to
`onCreated`, `onFocusChanged`, `onRemoved` (`bw/.../browser-api.ts:116-196, 460`). 1Password calls
`getCurrent`, `get`, `getAll`, `create` and listens to `onCreated`, `onFocusChanged`
(`op/background/background.js`). Neither manifest needs a permission for it.

Qt 6.11.1 status: absent. No schema, no feature.

Chromium's implementation lives in the same `tabs_api_non_android.cc` and `windows_event_router.cc`
(`fork/chrome/browser/extensions/api/tabs/windows_event_router.cc:12-20` needs `browser_process.h`,
`extension_service.h`, `extension_tab_util.h`, `window_controller.h`,
`chrome/browser/ui/browser.h`).

From Omaweb's model: window ids, the focused window, window bounds and state, and the tab list per
window. `windows.create` with a `url` is how both vendors open sign-in and settings pages; a
delegate can map it to a new tab or a Glance.

Bucket: **rewrite behind delegate**.

### action

Bitwarden calls `setPopup`, `openPopup` when present
(`bw/apps/browser/src/background/main.background.ts:2138-2194`) and the manifest declares `action`
with `default_popup`. 1Password calls `setPopup`, `setIcon`, `openPopup`, `getUserSettings`, and
listens to `onClicked` (`op/background/background.js`).

Qt 6.11.1 status: the manifest key parses and the popup URL is readable through
`QWebEngineExtensionInfo::actionPopupUrl`. No `chrome.action` namespace: `action.json` is an
uncompiled source that the `is_qtwebengine` block empties (`api_sources.gni:51-56, 174`).

Chromium's implementation:
`fork/chrome/browser/extensions/api/extension_action/extension_action_api.cc:17-20` includes
`extension_action_dispatcher.h`, `extension_tab_util.h`, `profile.h`, and
`chrome/browser/ui/toolbar/toolbar_actions_model.h`; `ActionGetUserSettingsFunction::Run` asks
`ToolbarActionsModel` whether the action is pinned (`extension_action_api.cc:398`); the non-Android
`openPopup` needs `chrome/browser/ui/browser.h`, `extensions_container.h`, and `tab_strip_model.h`
(`extension_action_api_non_android.cc:9-16`). The per-tab state itself (`ExtensionAction`,
`ExtensionActionManager`) is core (`fork/extensions/browser/extension_action_manager.h`), which is
how Qt already reads the popup URL.

From Omaweb's model: the surface that opens the popup (the Glance-shaped surface #344 names) and a
change signal for icon, title, badge, and popup path per tab. From Chromium's services: nothing once
the toolbar model is replaced by an embedder signal.

Bucket: **rewrite behind delegate**. The compile route is a stop (`chrome/browser/ui`).

### commands

Bitwarden listens to `onCommand` and calls `getAll`
(`bw/apps/browser/src/background/commands.background.ts:46-47`,
`bw/apps/browser/src/platform/services/platform-utils/browser-platform-utils.service.ts:327`). Its
manifest declares `_execute_action`, `autofill_login`, `autofill_card`, `autofill_identity`,
`generate_password`, and `lock_vault`. 1Password listens to `onCommand` and calls `getAll` and
`update` (`op/background/background.js`); its manifest declares `_execute_action` and `lock`.

Qt 6.11.1 status: the manifest key parses through the core `CommandsHandler`. No `chrome.commands`
namespace; `commands.json` is chrome-layer.

Chromium's implementation: `fork/chrome/browser/extensions/api/commands/commands.cc:10-11` needs
`CommandService`, whose source is absent from the fork and upstream includes
`extension_commands_global_registry.h`, `extension_keybinding_registry.h`, `profile.h`, and
`chrome/browser/ui/accelerator_utils.h`
(`upstream/chrome/browser/extensions/commands/command_service.cc:19-22`). `onCommand` is fired by
the keybinding registry, which is `chrome/browser/ui`.

From Omaweb's model: everything. #344 settles that a Known extension's commands become Omaweb
commands, so `getAll` reports Omaweb's bindings and `onCommand` is dispatched by Omaweb's command
layer through the core `EventRouter`.

Bucket: **rewrite behind delegate**.

### scripting

Bitwarden calls `executeScript`, `registerContentScripts`, `unregisterContentScripts`
(`bw/.../browser-api.ts:940-1070`). 1Password calls `executeScript` (4)
(`op/background/background.js`). Both manifests declare `scripting`.

Qt 6.11.1 status: absent. The schema `scripting.idl` is chrome-layer and stripped.

Chromium's implementation: `fork/chrome/browser/extensions/api/scripting/scripting_api.cc:15-43`
includes nothing from `chrome/browser` except its own header and the generated
`chrome/common/extensions/api/scripting.h`. Tab resolution goes through the core
`scripting_utils.cc:335-357`, which calls `ExtensionsBrowserClient::IsValidTabId` and
`GetScriptExecutorForTab`. The only chrome dependency is the `//chrome/browser/extensions` target in
its `BUILD.gn:18`, a target whose sources the fork does not carry.

From Omaweb's model: tab ids, and a `ScriptExecutor` per `WebContents`, which is a core class
(`fork/extensions/browser/script_executor.h`) that Qt does not instantiate anywhere
(`grep ScriptExecutor qt/src/core` is empty). From Chromium's services: nothing.

Bucket: **compile with delegate**. The delegate is the two `ExtensionsBrowserClientQt` overrides
plus a `ScriptExecutor` owned by `ExtensionWebContentsObserverQt`.

### contextMenus

Bitwarden calls `create`, `update`, `remove`, `removeAll` and listens to `onClicked`, guarded by
`if (!chrome.contextMenus)`
(`bw/apps/browser/src/autofill/browser/main-context-menu-handler.ts:274-275`). 1Password calls
`create`, `update`, `removeAll` and listens to `onClicked` (`op/background/background.js`). Both
manifests declare `contextMenus`.

Qt 6.11.1 status: absent.

Chromium's implementation:
`fork/chrome/browser/extensions/api/context_menus/context_menus_api.cc:12-15` needs
`context_menu_helpers.h`, `menu_manager.h`, and `profile.h`. `MenuManager` is absent from the fork;
upstream it needs `extension_tab_util.h`, `menu_manager_factory.h`,
`permissions/active_tab_permission_granter.h`, and guest-view headers
(`upstream/chrome/browser/extensions/menu_manager.cc:22-31`). Showing the items is
`chrome/browser/renderer_context_menu`, which is `chrome/browser/ui`.

From Omaweb's model: the context menu itself. Qt already hands the embedder a
`QWebEngineContextMenuRequest` (`qt/src/webenginewidgets/api/qwebengineview.cpp:468`); a delegate
stores items and Omaweb draws and dispatches them.

Bucket: **rewrite behind delegate**. The compile route is a stop.

### notifications

Bitwarden calls `create`, `clear` and listens to `onClicked`, `onButtonClicked`, guarded by
`"notifications" in chrome`
(`bw/apps/browser/src/platform/system-notifications/browser-system-notification.service.ts:22-36`).
1Password calls `create` (9), `update`, `getAll` and listens to `onClosed`, `onClicked`,
`onButtonClicked` (`op/background/background.js`). Both manifests declare `notifications`.

Qt 6.11.1 status: absent. `notifications.idl` is chrome-layer.

Chromium's implementation:
`fork/chrome/browser/extensions/api/notifications/notifications_api.cc:21-29` needs
`browser_process.h`, `notifier_state_tracker.h`, `notification_common.h`, and the display helper,
which routes to `NotificationDisplayService`; `browser_process.h` and
`chrome/browser/notifications/*` are absent from the fork.

From Omaweb's model: a presenter. Qt already routes web notifications through
`QWebEngineProfile::setNotificationPresenter` (`qt/src/core/api/qwebengineprofile.cpp:729`).

Bucket: **rewrite behind delegate**. Implemented on 2026-09-21, exactly that way. Its absence stops
1Password's worker starting; see the log entry.

### privacy

Bitwarden reads and writes `privacy.services.passwordSavingEnabled`, `autofillAddressEnabled`, and
`autofillCreditCardEnabled`, guarded by `permissions.contains(["privacy"])`
(`bw/.../browser-api.ts:981-1030`). 1Password touches `chrome.privacy.services` eleven times and
declares `privacy` as a required permission (`op/manifest.json`).

Qt 6.11.1 status: absent. The API is `preference_api.cc` in the chrome layer.

Chromium's implementation: `fork/chrome/browser/extensions/api/preference/preference_api.cc:20-28`
needs `extension_service.h`, `pref_mapping.h`, `preference_helpers.h`, and `PrefService`.
`pref_mapping` maps the three settings to `autofill::prefs::kAutofillProfileEnabled`,
`kAutofillCreditCardEnabled`, and `password_manager::prefs::kCredentialsEnableService`
(`upstream/chrome/browser/extensions/pref_mapping.cc:44-56`). Qt registers the two autofill prefs as
`false` and not the password one (`qt/src/core/pref_service_adapter.cpp:140-143`).

From Omaweb's model: nothing. These settings control Chrome's own autofill and password saving,
which Omaweb does not have. The vendors call them to switch the host browser's autofill off; on
Omaweb the honest answer is a `ChromeSetting` whose `get` reports `not_controllable` or a fixed
`false` and whose `set` succeeds.

Bucket: **rewrite behind delegate**, with the smallest possible schema.

### webNavigation

Bitwarden listens to `onCompleted`, `onCommitted`, `onErrorOccurred` and calls `getFrame`,
`getAllFrames` (`bw/.../browser-api.ts:611-624`; counts across `apps/browser/src`). 1Password
listens to `onBeforeNavigate`, `onCommitted`, `onDOMContentLoaded`, `onCreatedNavigationTarget` and
calls `getFrame`, `getAllFrames` (`op/background/background.js`). Both manifests declare
`webNavigation`.

Qt 6.11.1 status: absent. `web_navigation.json` is chrome-layer.

Chromium's implementation:
`fork/chrome/browser/extensions/api/web_navigation/web_navigation_api.cc:14-16, 69-84` includes
`extension_tab_util.h` and `browser_window_interface.h`, and `WebNavigationEventRouter` is a
`BrowserTabStripTracker` client with `OnTabStripModelChanged`. The per-tab observer and the event
helpers need only `ExtensionTabUtil::GetTabId` and the core `ExtensionApiFrameIdMap`
(`web_navigation_api_helpers.cc:65, 175, 207`).

From Omaweb's model: tab ids, and the "which tab opened which" relation for
`onCreatedNavigationTarget`. From Chromium's services: nothing beyond `WebContentsObserver`.

Bucket: **rewrite behind delegate**. The compile route is a stop (`TabStripModel`).

Corrected on 2026-09-21: the stop holds for the events and not for the functions. `getFrame` and
`getAllFrames` are implemented on the tab registry, reading each frame's own committed URL, and need
no tab-strip observer. See the log entry.

### webRequest and webRequestAuthProvider

Bitwarden listens to `onBeforeRequest`, `onCompleted`, `onHeadersReceived`, `onBeforeRedirect`,
`onAuthRequired` (counts across `apps/browser/src`). 1Password listens to `onHeadersReceived`,
`onBeforeRedirect` (`op/background/background.js`). Both manifests declare `webRequest` and
`webRequestAuthProvider`.

Qt 6.11.1 status: schema, feature, permission, and `WebRequestAPI` are core and compile. Nothing
proxies. `ContentBrowserClientQt::WillCreateURLLoaderFactory` installs only
`ProxyingURLLoaderFactoryQt` (`qt/src/core/content_browser_client_qt.cpp:1280-1327`).

What the plumbing needs: a call to `WebRequestAPI::MaybeProxyURLLoaderFactory` from
`WillCreateURLLoaderFactory` and to `MaybeProxyAuthRequest` for `onAuthRequired`
(`fork/extensions/browser/api/web_request/web_request_api.h:225-250`), as
`fork/extensions/shell/browser/shell_content_browser_client.cc:283-307` does, plus the core
`GetTabAndWindowIdForWebContents` hook so event details carry a tab id
(`fork/extensions/browser/extensions_browser_client.h:395-403`). Both factories append to the same
`URLLoaderFactoryBuilder`, so their order is a prototype question.

Bucket: **compile with delegate**. Nothing chrome-layer is involved.

### offscreen

Bitwarden calls `createDocument`, `closeDocument`, `hasDocument` behind
`typeof chrome.offscreen !== "undefined"`
(`bw/apps/browser/src/platform/offscreen-document/offscreen-document.service.ts:12-59`) and declares
`offscreen`. 1Password does not use it.

Qt 6.11.1 status: present in the build and unverified. `offscreen.idl`, the `offscreen` feature, and
the permission are core (`fork/extensions/common/api/schema.gni:23`, `_api_features.json:537-540`,
`_permission_features.json:524-528`). The implementation target is in the core group Qt depends on
(`fork/extensions/browser/api/BUILD.gn:43`), and `OffscreenDocumentHost` is a core `ExtensionHost`
(`fork/extensions/browser/BUILD.gn:430-431`). `OffscreenDocumentManager::CreateOffscreenDocument`
needs only `ExtensionsBrowserClient` and the browser context
(`fork/extensions/browser/api/offscreen/offscreen_document_manager.cc:110-134`), and the host uses
Qt's `ExtensionHostDelegateQt` (`qt/src/core/extensions/extensions_browser_client_qt.cpp:431-434`).
That delegate's `CreateTab` is `Q_UNREACHABLE`
(`qt/src/core/extensions/extension_host_delegate_qt.cpp:10-22`), which an offscreen document never
calls.

Bucket: **compile with delegate**, with nothing to write until a test says otherwise.

### permissions

Bitwarden calls `contains` on every native-messaging and privacy path and `request` from settings
(`bw/.../browser-api.ts:826-846`,
`bw/apps/browser/src/background/nativeMessaging.background.ts:100`). Its `nativeMessaging` and
`privacy` are optional permissions. 1Password calls `contains` once and declares everything as
required.

Qt 6.11.1 status: absent. `permissions.json` is chrome-layer.

Chromium's implementation: `fork/chrome/browser/extensions/api/permissions/permissions_api.cc:15-20`
needs `extension_install_prompt.h`, `extension_management.h`, `extension_tab_util.h`,
`permissions/permissions_updater.h`, and `chrome_extension_function_details.h` for the native window
of the prompt (`permissions_api.cc:324, 476-484`). None of those files is in the fork. `contains`
and `getAll` need only `PermissionsData`.

From Omaweb's model: the Known extension entry, which #344 says names the native hosts it may
launch. `request` becomes "is this permission in the entry" and `contains` reads the granted set
through `ExtensionPrefs`, which Qt builds (`qt/src/core/profile_qt.cpp:38-39`).

Bucket: **rewrite behind delegate**.

### sidePanel

Bitwarden calls `open` and `setOptions` behind `typeof chrome.sidePanel !== "undefined"`
(`bw/.../browser-api.ts:720-746`) and declares `sidePanel`. 1Password does not use it.

Qt 6.11.1 status: absent.

Chromium's implementation:
`fork/chrome/browser/extensions/api/side_panel/side_panel_service.cc:13-15` needs
`extension_tab_util.h`, `profile.h`, and
`chrome/browser/ui/extensions/extension_side_panel_utils.h`.

Bucket: **stop**, and it does not matter: Bitwarden treats the namespace as optional.

### declarativeNetRequestWithHostAccess

1Password declares the permission and references `chrome.declarativeNetRequest` once without calling
a method (`op/manifest.json`, `op/background/background.js`). Bitwarden does not declare it.

Qt 6.11.1 status: schema, feature, permission, and `RulesMonitorService` are core
(`fork/extensions/common/api/_api_features.json:156-162`, `_permission_features.json:207-210`,
`fork/extensions/browser/BUILD.gn:120-154`). Rules are evaluated inside the webRequest event router
(`fork/extensions/browser/api/web_request/extension_web_request_event_router.cc:970-972`), so the
API is inert until the webRequest proxy is installed.

Bucket: **compile with delegate**, the same plumbing as webRequest.

### nativeMessaging

Bitwarden calls `runtime.connectNative` after `permissions.contains(["nativeMessaging"])`
(`bw/.../browser-api.ts:818-823`, `nativeMessaging.background.ts:100`). 1Password calls
`runtime.connectNative` twice (`op/background/background.js`). Both manifests declare
`nativeMessaging`, Bitwarden as optional.

Qt 6.11.1 status: the schema and feature are core (`runtime.json`, `runtime.connectNative` depends
on `permission:nativeMessaging`), so the function exists and disconnects with the "disabled by the
system administrator" message. No host is ever looked up.

Chromium's implementation, all in `fork/chrome/browser/extensions/api/messaging/` and present in the
fork's tree (its `BUILD.gn:643-656` lists them for non-ChromeOS builds):

- `chrome_messaging_delegate.cc:35-73` answers `IsNativeMessagingHostAllowed` from three
  `PrefService` policies (`kNativeMessagingUserLevelHosts`, `kNativeMessagingBlocklist`,
  `kNativeMessagingAllowlist`) read through `Profile::FromBrowserContext(...)->GetPrefs()`, and
  `:111-129` builds the receiver with `NativeMessageHost::Create` and `NativeMessagePort`, both core
  (`fork/extensions/browser/api/messaging/native_message_host.h:44-50`).
- `native_message_process_host.cc:105-121` is `NativeMessageHost::Create`; it calls
  `NativeProcessLauncher::CreateDefault` with a profile path from
  `ExtensionSupportsConnectionFromNativeApp` (`:54-61`), which lives in
  `native_messaging_launch_from_native.cc:17-23` and needs `browser_process.h` and
  `keep_alive_registry`. That path exists only for host-initiated connections
  (`launch_context.cc:173-176`), which neither vendor's manifest declares.
- `native_process_launcher.cc:129-142` reads one `PrefService` boolean,
  `kNativeHostsExecutablesLaunchDirectly`, and only for Windows.
- `launch_context.cc:139-200` validates the host name, finds the manifest, loads it, checks `name`,
  checks `allowed_origins` against the extension origin, requires an absolute `path` on POSIX, and
  checks the binary exists. `launch_context_posix.cc:40-56` looks the manifest up in
  `chrome::DIR_USER_NATIVE_MESSAGING` then `chrome::DIR_NATIVE_MESSAGING`; `:59-` launches with
  stdin and stdout pipes.
- `native_messaging_host_manifest.cc:19-27, 75-105` accepts names of `[a-z0-9._]`, requires `type`
  equal to `stdio`, a `path`, and an `allowed_origins` list.

Host manifest directories on Linux, from `fork/chrome/common/chrome_paths.cc:495-520`: the system
directory is `/etc/opt/chrome/native-messaging-hosts` under Google branding and
`/etc/chromium/native-messaging-hosts` otherwise; the user directory is
`<DIR_USER_DATA>/NativeMessagingHosts`, which for Chromium resolves under `~/.config/chromium/`. Qt
does not link `chrome_paths.cc` and runs its own path provider starting at `PATH_QT_START = 1000`
(`qt/src/core/profile_qt.cpp:49`), so a delegate supplies the lookup directories itself.

Dependencies summarised: `Profile` as `GetPrefs` and `GetPath` only, which `ProfileQt` satisfies;
`PrefService` for four policy prefs, which a delegate can answer as "no policy"; no `Browser`, no
`TabStripModel`, no `chrome/browser/ui`. `chrome_messaging_delegate.cc` itself also needs
`extension_tab_util.h` for `MaybeGetTabInfo` and `GetWebContentsByTabId` (`:14, 79-107`), so the Qt
delegate keeps its own class and takes only the two native methods.

Bucket: **compile with delegate** for `native_message_process_host.cc`,
`native_process_launcher.cc`, `launch_context*.cc`, and `native_messaging_host_manifest.cc`, with
`MessagingDelegateQt` gaining `IsNativeMessagingHostAllowed` and `CreateReceiverForNativeApp` and
the Known extension entry supplying the host name allow-list and manifest directories. This is the
narrow reading of the `Profile` stop rule described under Buckets.

Vendor host manifests:

- Bitwarden writes `com.8bit.bitwarden` with `type: "stdio"` and `allowed_origins` listing four Web
  Store extension ids (`nngceckbapebfimnlniiiahkandclblb` for Chrome)
  (`bw/apps/desktop/src/main/native-messaging.main.ts:143-150, 450-460`). On Linux it writes
  `<browser dir>/NativeMessagingHosts/com.8bit.bitwarden.json` and hard-links a proxy binary next to
  it, for each of `~/.config/google-chrome/`, `~/.config/chromium/`, `~/.config/microsoft-edge/`,
  `~/.config/vivaldi/`, `~/.config/BraveSoftware/Brave-Browser/`, and `~/.config/net.imput.helium/`
  that exists (`:212-247, 420-428`), plus Flatpak paths (`:431-436`). It writes nowhere Omaweb
  reads; a Known extension entry must either name one of those files or Omaweb must accept a copied
  manifest.
- 1Password's host is `com.1password.1password`, `type: "stdio"`, `path`
  `/usr/lib/opt/1Password/1Password-BrowserSupport`, with five allowed extension ids, written to
  `~/.config/google-chrome/NativeMessagingHosts` for Chrome. This comes from a community README that
  quotes the file
  ([FlyinPancake/1password-flatpak-browser-integration](https://github.com/FlyinPancake/1password-flatpak-browser-integration/blob/main/README.md),
  lines 20-40) and is unverified against 1Password's own documentation. 1Password's support page
  states that on Linux the app accepts integration requests only from browsers on its internal list
  or named by binary name, one per line, in `/etc/1password/custom_allowed_browsers`, edited as root
  ([Additional browsers](https://support.1password.com/additional-browsers/)). The same community
  README notes the host verifies the calling process (`README.md:95`). Omaweb's binary name would
  need to be in that file.

One consequence for both: `allowed_origins` names Web Store extension ids. Qt loads extensions
unpacked from a directory or zip (`qt/src/core/extensions/extension_installer.cpp:67-75`) as
`ManifestLocation::kUnpacked` (`qt/src/core/extensions/extension_loader.cpp:57`), and neither vendor
manifest carries a `key` field (`bw/manifest.v3.json`, `op/manifest.json`). An unpacked extension
without `key` gets an id derived from its path, which no vendor host will accept. The package
acquisition question deferred on #344 has to produce the Web Store id, which means carrying the
store public key as `key` in the loaded manifest.

## Issue #288's five facts against Qt 6.11.1

[Issue #288](https://github.com/villekivela/omaweb/issues/288) recorded five facts when #272 closed.
Against qtwebengine `v6.11.1` and fork commit `58c11ad4`:

1. **Holds.** The `chrome.*` surface is the core layer plus Qt's `tabs` with `update` only:
   `fork/chrome/common/extensions/api/api_sources.gni:167-175`,
   `fork/qtwebengine/common/extensions/api/tabs.json:35-110`,
   `fork/extensions/common/api/_webengine_api_features.json`. One refinement: Chrome's
   `_api_features.json` is loaded
   (`qt/src/core/common/extensions/extensions_api_provider_qt.cpp:44-47`) but only into the
   test-only feature source, so those namespaces are `undefined` rather than present-but-broken.
2. **Holds.** `qt/src/core/extensions/messaging_delegate_qt.cpp` overrides `MaybeGetTabInfo` only;
   `fork/extensions/browser/api/messaging/messaging_delegate.cc:13-19` returns `DISALLOW`;
   `message_service.cc:88-90, 691-697` disconnects with the administrator message.
3. **Holds.** `qt/src/core/extensions/extension_loader.cpp:24, 63` accepts manifest version 3 only;
   `:74-75` refuses off-the-record contexts.
4. **Holds** as far as this pass could read.
   `qt/src/core/authenticator_request_client_delegate_qt.cpp` adds no platform authenticator or
   hybrid handling; `OnTransportAvailabilityEnumerated` starts the dialog without inspecting the
   transports (`:168-180`), and the shipped dialog copy speaks only of a security key
   (`qt/examples/webenginewidgets/simplebrowser/webauthdialog.cpp:120-146`). No line in the 6.11.1
   `qdoc` sources names the Linux transport list, so the Qt documentation citation #288 relied on
   was not re-verified here.
5. **Holds.** 1Password's manifest injects a `world: "MAIN"` content script on `https://*/*`
   (`op/manifest.json`, `content_scripts[2]`), and Bitwarden ships `content/fido2-page-script.js` as
   a web-accessible resource (`bw/manifest.v3.json`, `web_accessible_resources`). Both provide
   passkeys from inside the extension; neither manifest or source names a Linux platform provider.

Branches: qtwebengine `dev` (6.12) has byte-identical `messaging_delegate_qt.cpp`,
`extensions_api_provider_qt.cpp`, and `extension_loader.cpp`; the fork's `146-based` branch has
identical `tabs.json` and `_webengine_api_features.json`, and its `api_sources.gni` differs only in
upstream list reshuffles with the `is_qtwebengine` block unchanged, checked by hash on 2026-09-17.

## Summary

| API                                 | Needed by               | Qt 6.11.1 status                                              | Bucket                                |
| ----------------------------------- | ----------------------- | ------------------------------------------------------------- | ------------------------------------- |
| tabs (query, get, create, ...)      | both                    | `update` only, sender-scoped                                  | rewrite behind delegate               |
| tabs.sendMessage                    | both                    | renderer hook in fork, unregistered; browser needs tab lookup | rewrite behind delegate               |
| windows                             | both                    | absent                                                        | rewrite behind delegate               |
| action                              | both                    | manifest parses, popup URL exposed, no namespace              | rewrite behind delegate               |
| commands                            | both                    | manifest parses, no namespace                                 | rewrite behind delegate               |
| scripting                           | both                    | absent; implementation has no browser includes                | compile with delegate                 |
| contextMenus                        | both (Bitwarden guards) | absent                                                        | rewrite behind delegate               |
| notifications                       | both (Bitwarden guards) | absent                                                        | implemented, see 2026-09-21           |
| privacy                             | both (Bitwarden guards) | absent                                                        | rewrite behind delegate               |
| webNavigation                       | both                    | absent                                                        | functions implemented, see 2026-09-21 |
| webRequest, webRequestAuthProvider  | both                    | compiled, no proxy installed                                  | compile with delegate                 |
| offscreen                           | Bitwarden (guarded)     | compiled, host delegate present, untested                     | compile with delegate                 |
| permissions                         | both                    | absent                                                        | rewrite behind delegate               |
| sidePanel                           | Bitwarden (guarded)     | absent                                                        | stop                                  |
| declarativeNetRequestWithHostAccess | 1Password               | compiled, inert without the webRequest proxy                  | compile with delegate                 |
| nativeMessaging                     | both                    | function exists, delegate refuses                             | compile with delegate                 |

Counts over the rows: compile with delegate 5, rewrite behind delegate 10, stop 1. Counting `tabs`
and `tabs.sendMessage` as one API, the rewrite bucket holds 9.

Six of those rows are now implemented and measured rather than predicted, and every bucket held:
`scripting` and `nativeMessaging` compiled from Chrome with their includes rewritten, `tabs`,
`windows` and `permissions` were rewritten behind the tab delegate, and `webNavigation`,
`contextMenus`, `notifications`, `privacy`, `commands` and `action` exist as schemas whose events an
extension can register for. Nothing needed `Browser`, `TabStripModel` or `chrome/browser/ui`.

## Prototype log

Findings from running the extension against stock Homebrew Qt 6.11.1 on macOS, before any patch. The
harness is a forty-line Qt Widgets program that creates a named `QWebEngineProfile`, loads an
unpacked extension through `QWebEngineExtensionManager`, enables it on `loadFinished`, loads
`actionPopupUrl()` in a `QWebEngineView`, and attaches to every DevTools target over the remote
debugging port to read consoles and execution contexts.

### 2026-09-18

1. Bitwarden 2026.9.0 loads with no manifest error. Its id is `fjifgjjmkmpmjjmniponjainkdijegam`,
   derived from the unpacked path, which confirms that a host manifest allowing only the store id
   would refuse it.
2. Calling `QWebEngineProfile::setPersistentStoragePath` after constructing the profile makes
   `setExtensionEnabled` segfault in `PrefService::GetPreferenceValue` under
   `ExtensionRegistrar::EnableExtension` → `blocklist_prefs::IsExtensionBlocklisted` →
   `ExtensionPrefs::GetExtensionPref`. `ProfileQt::setupPrefService` (`qt/src/core/profile_qt.cpp`)
   rebuilds the `PrefService` on a path change and re-creates `ExtensionPrefs` through
   `SetInstanceForTesting`, but the `ExtensionPrefs` the registrar reads still points at the old
   service. Omaweb sets a storage path per Space, so this is on the critical path.
3. The empty popup is a JavaScript error, not a renderer fatal, which settles the hypothesis above.
   The popup document loads (`loadFinished` true) and its console reports
   `TypeError: Cannot read properties of undefined (reading 'getBackgroundPage')` from
   `chrome.extension`, then `chrome.tabs.getCurrent is not a function`. The `chrome` object in the
   popup holds `alarms`, `clipboard`, `i18n`, `idle`, `management`, `offscreen`, `runtime`,
   `storage`, `tabs` (`update` and `TAB_ID_NONE` only), and `webRequest`.
4. The content script injects: the fixture login page served over HTTP gains an isolated world named
   "Bitwarden Password Manager". Served over `file://` it does not, which is the unpacked
   extension's default file-access setting rather than a Qt gap.
5. The service worker never starts. Its DevTools target exists but answers nothing, and at shutdown
   Chromium reports "Service worker registration failed. Status code: 2" (`kErrorAbort`), then the
   browser process traps in `~ServiceWorkerTaskQueue` destroying the popup's pending
   `runtime.sendMessage` channels. Qt's own `service_worker_ext` fixture starts, Bitwarden's
   manifest with a trivial worker starts, a 3.4 MB worker starts, and a worker that only calls
   `importScripts` starts, so the script's content is what blocks. Sampling the renderer's
   "ServiceWorker thread" shows it inside `V8ScriptRunner::CompileAndRunScript` →
   `I18nHooksDelegate::HandleGetMessage` → `SharedL10nMap::GetMapForExtension` →
   `mojom::RendererHostProxy::GetMessageBundle` → `mojo::SyncHandleRegistry::Wait`. The worker calls
   `chrome.i18n.getMessage` at the top level, that call is a synchronous mojo request to the
   browser, and nothing in the browser answers it. Upstream registers two extension interfaces per
   renderer in `ExposeInterfacesToRenderer`, `EventRouter::BindForRenderer` and
   `RendererStartupHelper::BindForRenderer` for `mojom::RendererHost`
   (`fork/extensions/browser/renderer_startup_helper.cc:546`). Qt registers only the first
   (`qt/src/core/content_browser_client_qt.cpp:520-522`). This is the first patch: any extension
   that localises from its worker hangs on stock Qt.

### 2026-09-18, patched engine

QtWebEngine 6.11.1 built from the source tarball on this Mac against Homebrew's Qt, with the series
from the series named above applied. Each patch carries a `tst_qwebengineextension` case that fails
on the Homebrew build and passes on the patched one; the whole suite passes, 20 of 20.

1. `RendererHost` has to be offered twice. A frame reaches the browser through the render process's
   associated interface registry, but a service worker reaches it through its own provider, which
   `RegisterAssociatedInterfaceBindersForServiceWorker` serves. Qt registered only
   `ServiceWorkerHost` there, where Chrome also registers `EventRouter` and `RendererHost`. Binding
   `RendererHost` on the process registry alone left the worker blocked exactly as before.
2. With both bound, Bitwarden's worker evaluates. `chrome.i18n.getMessage` is answered and the
   script runs until `chrome.webNavigation.onCommitted`, which is `undefined`, so registration fails
   with status 15 (`kErrorScriptEvaluateFailed`). Every failure from here is a missing chrome-layer
   namespace from the rewrite bucket rather than a broken piece of plumbing.
3. The `ExtensionPrefs` fix re-points the existing instance through a hook on `ExtensionPrefs`
   guarded by `IS_QTWEBENGINE`, since `ExtensionRegistrar` and `EventRouter` cache the pointer at
   construction and nothing short of tearing down every keyed service would refresh a replacement.
4. The Xcode 27 SDK dropped `kSBXProfilePureComputation`; a local build fix outside the series
   defines the profile name itself.

### 2026-09-18, the namespaces

Four patches now, each with a `tst_qwebengineextension` case that fails on the Homebrew build and
passes on the patched one. The suite passes 21 of 21.

1. `RendererHost` and `EventRouter` have to be offered three times, not once. A frame reaches the
   browser through its own associated interface registry, a service worker through a third, and Qt
   registered `EventRouter` only for the render process and `RendererHost` nowhere. Binding all
   three at every registration point is what let Bitwarden's worker finish evaluating and its popup
   receive `storage.onChanged`, without which the popup boots Angular and no route activates.
2. The rewrite bucket cost one 150-line registry. `TabRegistryQt` assigns a tab id that lives on the
   `WebContents`, and a `TabsDelegateQt` answers which of a context's pages are tabs and which is
   active. `ExtensionsBrowserClientQt` implements it from the profile's adapter clients, and the
   core layer's `GetTabAndWindowIdForWebContents`, `IsValidTabId` and the messaging delegate answer
   from the same registry, so `sender.tab` and `tabs.query` agree.
3. The schemas are Chrome's own. Nine of them compile through QtWebEngine's list with the functions
   that have no implementation marked `nocompile`, which is how Chrome itself ships partial
   namespaces. `action`, `extension` and their JSON-only siblings follow Chrome's
   `uncompiled_sources_` split. Implemented so far: `tabs.get`, `getCurrent`, `query`, `update`,
   `windows.get`, `getCurrent`, `getLastFocused`, `getAll`, `permissions.contains` and `getAll`.
4. The compile-with-delegate bucket held. `chrome/browser/extensions/api/scripting/scripting_api.cc`
   compiles unchanged once its own two includes point at QtWebEngine's copies; it reaches a tab
   through `IsValidTabId` and `GetScriptExecutorForTab`, and the registry answers the second with a
   `ScriptExecutor` owned by the tab's `WebContents`. No other Chrome browser file was needed.

Bitwarden on the patched engine: the extension loads, its service worker runs, the popup renders its
real interface (account carousel, "Create account", "Log in"), `chrome.runtime` messages and ports
round-trip in both directions, `tabs.query` and `windows.getAll` report the browser's own pages, and
`chrome.scripting.executeScript` injects a file or a function into a tab. Bitwarden's own autofill
script is injected only for a logged-in account, so the collector was exercised directly instead: a
script injected into the fixture login page answers a `tabs.sendMessage` with the page's fields
(`username`, `current-password`) and fills both, which the page's own world reads back. The
remaining gap to real autofill is a vault, not an engine capability.

### 2026-09-18, native messaging

`runtime.connectNative` answers. An extension declaring `nativeMessaging` opens a port to
`com.example.echo`, QtWebEngine launches the program the host manifest names, and the reply reaches
the extension and then the page through its content script. The autotest installs the manifest for
the running application, so it exercises the lookup rather than assuming it.

What that cost, against the bucket the note predicted:

- Chrome's `native_messaging_host_manifest`, `native_process_launcher`, `launch_context`,
  `launch_context_posix`, `native_message_process_host` and `chrome_native_message_port_dispatcher`
  compile with their includes rewritten. Two changes were needed. The manifest lookup uses
  QtWebEngine path keys of its own, because `chrome_paths` is not compiled and its key range is the
  range QtWebEngine already uses for storage paths. And `NativeMessageHost::Create` passes no
  profile directory, dropping `native_messaging_launch_from_native` and with it the
  `g_browser_process` dependency the survey flagged; what it drops is a host starting a connection
  by relaunching the browser, which neither vendor declares.
- `chrome_features` is not compiled either, so `kOnConnectNative` is declared in an eighteen-line
  shim, disabled by default as it is in Chrome.
- `MessagingDelegateQt` answers `ALLOW_ALL` and builds the port. Which hosts may run stays the
  application's decision, which is the shape
  [issue #344](https://github.com/villekivela/omaweb/issues/344) settled: a Known extension names
  its hosts, and a host manifest already names the extensions it answers.
- The per-user lookup directory is the application's own `NativeMessagingHosts`. The machine-wide
  one is Chromium's, which is where the hosts that exist today install themselves.

### 2026-09-18, extension identity

Neither vendor's host answers an unpacked build, because both list Web Store extension ids in
`allowed_origins` and an unpacked extension's id is derived from its path. Carrying the store's
public key in the manifest fixes that, and the key is public: it is in the CRX the Web Store serves.

Bitwarden's CRX header holds two RSA proofs, Google's signing key and the publisher's. The
publisher's key hashes to `nngceckbapebfimnlniiiahkandclblb`, the id the store lists, so adding it
to an unpacked build as `key` makes QtWebEngine load that build under the store id. Measured both
ways against a host manifest named `com.8bit.bitwarden` that allows only that id:

| Build              | Id QtWebEngine derives             | `connectNative("com.8bit.bitwarden")`                         |
| ------------------ | ---------------------------------- | ------------------------------------------------------------- |
| With the store key | `nngceckbapebfimnlniiiahkandclblb` | the host answers                                              |
| Without it         | path-derived, differs per machine  | "Access to the specified native messaging host is forbidden." |

So a Known extension acquired as a store package keeps its identity and the vendor's desktop
application answers it, and an extension assembled some other way is refused by the vendor's own
rule rather than by anything Omaweb decides. The refusal is covered by an autotest, since it is the
half that matters for security.

### 2026-09-18, autofill

Bitwarden fills a login form on the patched engine, from a real vault. Signed in against
`bitwarden.eu` in the probe's own profile, with one item saved for the fixture's address, the
extension's own code collects the page's fields and writes both of them. The trace is Bitwarden's,
not the prototype's: `collectPageDetailsResponse` carries 1683 bytes of page details to the worker,
and `updateIsFieldCurrentlyFilling` and the inline-menu visibility checks follow.

Two failures on the way, both worth recording because neither was an engine gap and both cost time:

1. A page loaded before the extension was enabled has no autofill script in it, and Bitwarden's fill
   then ends in its own "Unable to autofill". The content script answers
   `collectPageDetailsImmediately` with nothing while `isMonitoring` is false, which is the cheapest
   way to tell this state apart from a broken message path. Reloading the page fixes it. An
   application that enables a Known extension after a page is already open has to reload or inject
   into the pages that were open, which is a real integration requirement rather than a test
   artefact.
2. The inline menu is drawn only when the reader has turned it on.
   `getBootstrapAutofillContentScript` chooses between four content scripts from
   `inlineMenuVisibility` and the two notification prompts, so with the menu off Bitwarden injects
   `bootstrap-autofill-overlay-notifications.js` and no menu appears in the field. Nothing to fix
   here, but it means the in-field menu cannot be used as a signal that autofill is working.

One engine defect the session did find: the extension's own popup was listed as a tab, because every
page of the profile was. `chrome.tabs.query` then reported the popup alongside the page, and an
extension asking for the active tab could be told about its own popup. Extension pages are now
excluded, which is what Chrome does, and `tabs.query` from the popup reports one tab, the page.

`chrome.action` is the remaining rough edge in ordinary use: the namespace exists but its functions
do not, so Bitwarden logs "Failed to set badge state" whenever it would update its badge. Harmless,
and the badge is a surface an application would draw itself anyway.

### 2026-09-18, rebasing onto 6.11.2

Qt 6.11.2 released during the session. All six patches apply to it with no conflicts and no fuzz, so
the rebase cost for this release is no one's time at all. One collision was reported and it was the
series' own fault: patch 0001 carried a build ignore file, which any checkout that ignores its own
build directory already has. That is fixed in the series rather than in the checkout.

Nothing this note reports has been fixed upstream. Against 6.11.2, 6.12.0-beta3 and `dev`:
`content_browser_client_qt.cpp` still mentions `RendererHost` nowhere, the service worker registry
still carries `ServiceWorkerHost` alone, `messaging_delegate_qt.cpp` still implements neither
`IsNativeMessagingHostAllowed` nor `CreateReceiverForNativeApp`, `ProfileQt::setupPrefService` still
replaces the `ExtensionPrefs` instance other services hold, and the Qt schema list is still the same
four files with the chrome-layer schemas stripped to two private IDLs. Three releases on, the
decision about what to enable has not moved, which is worth weighing when judging how a proposal to
widen it will be received.

### 2026-09-21, one bucket wrong, one severity missed

Running Omaweb on real hardware answered two per-API findings above differently.

**`webNavigation` was bucketed "rewrite behind delegate", and the compile route called a stop
because of `TabStripModel`. That was wrong for half the namespace.** `getFrame` and `getAllFrames`
need neither. Chrome answers them from a per-document `FrameNavigationState` its tab-strip observer
maintains, but a `RenderFrameHost` already knows its committed URL and whether it is an error
document, and the core `ExtensionApiFrameIdMap` numbers frames and documents. Both functions are
about sixty lines over the tab registry patch 0004 already provides. The events are the part that
needs the observer, and they are declared and unraised.

It mattered more than an API bucket usually does. Bitwarden's worker registers `runtime.onMessage`
inside asynchronous set-up, so the first page's message is dropped, in Chrome as well; Chrome users
never notice because the worker then reaches back into every open tab with `tabs.query`,
`webNavigation.getAllFrames` and `scripting.executeScript`. With `getAllFrames` unknown, the
callback never fires and the first page after a launch has no autofill until it is reloaded. The gap
was read as a message-delivery problem for a day before the reach-back was found.

**`notifications` was bucketed correctly and its severity was missed.** The finding named the
presenter and the route, and the implementation is what it said it would be. What the survey did not
say is that this namespace is not a feature an extension degrades without: 1Password registers
`notifications.onClicked` at the top level of its worker, so an absent namespace throws on the
worker's first line and it never starts at all. A namespace an extension touches before it does
anything belongs in a class of its own, separate from one it calls when it has something to say.

The general lesson for the remaining buckets: what Chrome's implementation includes is evidence
about Chrome, not about the API. `webNavigation`'s stop was reached by reading Chrome's includes,
and it did not survive being tried. The others were reached the same way.

### 2026-09-21, 1Password unlocks, and the one gap that still stops the fill

Run against the round five engine, patches 0001 to 0013, with 1Password 8.12.36 installed on the
test machine for the first time.

**The twenty-one functions answer.** Startup refusals fell from 225 in one session to two, both
`Unknown Extension API - types.ChromeSetting.set` from 1Password, which the series leaves
deliberately unregistered. Each throws as `Uncaught (in promise)` at the top of `background.js` and
the worker starts anyway, so `privacy` is survivable in a way `notifications` was not. `action` was
checked by calling it from Bitwarden's running worker rather than by reading the patch:
`setBadgeText("7")` then `getBadgeText` returns `7`, `getTitle` returns the manifest title,
`getPopup` returns the popup URL, `isEnabled` returns true. `contextMenus.removeAll`,
`commands.getAll` and `notifications.getPermissionLevel` all answer. Bitwarden's
`Failed to set badge state`, logged on every badge update before, is gone.

**1Password ships an aarch64 Linux desktop build.** The AUR package is `arch=('x86_64')`, which is a
fact about that package and not about what the vendor publishes;
`downloads.1password.com/linux/tar/stable/aarch64/1password-latest.tar.gz` answers 200 with 213 MB.
This is the same reasoning error the previous entry recorded for `webNavigation`, where Chrome's
includes were read as the API's requirements.

**No installer writes a manifest where the engine looks, and none ever will.** `after-install.sh` in
the tarball writes the polkit policy, `/etc/1password/custom_allowed_browsers`, the desktop file and
icons, the `onepassword` group and the setgid bit on the helper, and no native messaging manifest at
all. The desktop app writes fifteen on first launch, every one of them user level and per browser:
`~/.config/chromium/`, `google-chrome`, `google-chrome-beta`, `google-chrome-unstable`,
`microsoft-edge-dev`, `vivaldi`, `vivaldi-snapshot`, and eight `BraveSoftware` variants. It writes
no machine wide copy. The string `/etc/chromium/native-messaging-hosts` is in the app binary, and
reading it as evidence that the app writes there was wrong; patch 0006 reads that directory, the app
does not fill it.

Two details in the section above are corrected by the installed copy. The host path is
`/opt/1Password/1Password-BrowserSupport`, not `/usr/lib/opt/1Password/...` as the community README
quoted, and `allowed_origins` lists six extension ids rather than five. One of them is
`aeblfdkhhhdcdjpifhhbdiojplfjncoa`, the id the Known extension package already loads under, so the
manifest needs no editing.

**The recipe that works.** Install the desktop app from the official tarball, then
`chown -R root:root /opt/1Password` and re-run `after-install.sh`. Install the browser binary root
owned and put its filename, not its path, in `/etc/1password/custom_allowed_browsers`. Copy
`com.1password.1password.json` from any `~/.config/*/NativeMessagingHosts/` into the profile's
`<AppDataLocation>/NativeMessagingHosts/`.

Two traps cost time. Moving the unpacked tarball into `/opt` from a user directory leaves it user
owned, and `1Password-BrowserSupport` verifies both the browser binary's permissions and its own, so
a setgid helper its owner can rewrite is refused. The shipped `custom_allowed_browsers` ends without
a newline, so appending with `echo name | tee -a` produces `#name` and a silently empty allowlist.

With all three in place the helper launches as a child of the browser and the extension connects:

```text
[AppIntegration] 💫 Looking for desktop app com.1password.1password
[AppIntegration] 📤 Sending <NmRequestAccounts> message to native core <3423775968>
[AppIntegration] 📥 Received message <NmRequestAccounts>. Duration: 283.7ms
[AppIntegration] [DesktopApp] Initiation complete - B5X is connected to desktop app
[AccountHandlers] Hooray!; Unlocked account NKSYUUVPY5AOHB4NUUHF76K6HU with MUK; 🎉
[Sls] [SLS] Desktop connection changed from Connecting to Connected
[Syncer] ✅ Sync completed for account NKSYUUVPY5AOHB4NUUHF76K6HU - took 833ms
```

`decryptKeysets OperationError` and `User is not connected to the desktop app` are gone, and the
vault holds three items.

**The form is still empty.** Every load of the fixture logs four `net::ERR_FAILED` and then
`[InjectContentScripts]`, on a fresh launch and on a reload, so it is not the stale page state the
autofill entry above records for Bitwarden.

The cause is that the engine does not serve `chrome-extension://` subresources to a page.
Bitwarden's `images/icon38.png`, `content/fido2-page-script.js` and `notification/bar.html` all
exist on disk and are all declared under `web_accessible_resources` with `matches: ["<all_urls>"]`.
None of them loads from a page, by `fetch` or by an `Image` element, while a `data:` image in the
same context loads. `manifest.json` failing is correct, it is not web accessible; the three declared
ones failing is not.

The gap is the subresource path alone, not the policy check. In the same page, a subframe navigation
to `chrome-extension://<bitwarden>/notification/bar.html` loads, while a `script` element pointing
at `chrome-extension://<bitwarden>/content/fido2-page-script.js` fails. Both are declared web
accessible and both belong to the same extension, so the scheme is registered and
`WebAccessibleResourcesInfo` already answers correctly for navigations. What is missing is the
extension URL loader factory on the subresource path, which Chromium registers in
`RegisterNonNetworkSubresourceURLLoaderFactories`. The navigation side is implemented and the
subresource side is not.

That is enough to stop 1Password on its own. Its manifest declares one content script on
`<all_urls>`, `inline/inject-content-scripts.js`, which builds URLs with `chrome.runtime.getURL` and
pulls the real content scripts in with `import(r)` behind a retry wrapper that throws
`Import failed ${t} times`. The four `ERR_FAILED` lines are the retries and the throw is what
`[InjectContentScripts]` reports. No content script reaches the document, so there is nothing to
fill the form.

Bitwarden is unaffected because it declares its autofill scripts statically and imports nothing at
runtime. Asked for page details in the same document, its content script answers with the form:

```text
bitwarden → collectPageDetailsImmediately → REPLIED {"title":"Sign in — fill probe","forms":{...}}
```

**The three fill diagnostics proposed in round five all come back negative.** `tabs.sendMessage`
from 1Password's worker returns `Could not establish connection. Receiving end does not exist.`,
which is an absent content script rather than a closed port. The four argument form carrying
`frameId: 0` returns the identical error, so the routing difference between the two extensions' fill
paths is not the cause. `webNavigation.getAllFrames` answers and reports one frame,
`frameType: "outermost_frame"`, so the field is not in an iframe. `scripting.executeScript` runs in
the page and counts its two inputs, which proves the worker can reach the document.

**`tabs.create` has no application hook to refuse it.** `ExtensionsBrowserClientQt::OpenPage` and
`TabRegistryQt::OpenPage` are in the engine binary, but `qwebengineextensionmanager.h` declares only
the five load and install methods and four signals, `qwebengineprofile.h` adds only
`extensionManager()`, and `plugins.qmltypes` registers no page opening signal on any QML type.
Omaweb has no handler because there is nothing to connect one to, so
`The application did not open a tab.` is the delegate lacking a public surface rather than
`TabsDelegateQt` declining. No Glance was open when it was reproduced.

## What the prototype verifies first

1. The empty-popup cause. Answered in the prototype log: a `TypeError` on an `undefined` namespace.
2. Bitwarden loads with no manifest error. Passes on stock Qt.
3. The service worker starts and `runtime` messaging round-trips. Passes on the patched engine.
4. The popup renders its unlock screen in an Omaweb-hosted view. Passes on the patched engine.
5. A content script reports the fields of a fixture login page and fills them. Passes on the patched
   engine, with the script injected by the test rather than by Bitwarden, which gates its own
   injection on a logged-in account.
6. The popup renders its unlock screen in an Omaweb-hosted view. This is where `windows`,
   `tabs.query`, and `permissions.contains` are first hit.
7. Sign-in to a throwaway bitwarden.com account succeeds from that popup. This is where
   `windows.create` or `tabs.create` opens the sign-in page.
8. The content script reports the fields of a `tests/ui` fixture login page. This is where
   `tabs.sendMessage`, `scripting`, and `webNavigation` are first hit.
9. The patch series rebases across one Qt patch release.
