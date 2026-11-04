# Widevine research

This note records the research for #911 into whether Widevine can run on the engine Omaweb ships. It
is not an implementation contract. The engine is `omaweb-qtwebengine` 6.11.3, Qt's Chromium fork at
version 140.0.7339.264
([VERSION](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/chrome/VERSION?id=bcc09a68b50a40c60d8dc83f0e6b6dc872614812)),
submodule commit `bcc09a6`, with the patch series of
[ADR 0049](../adr/0049-ship-omawebs-own-engine-build.md) applied. No patch in the series, at its
commit `dd5b0ab`, touches the CDM code, so the source claims below hold for the shipped engine as
well as for stock QtWebEngine 6.11.3. [What this engine does here](#what-this-engine-does-here)
tests that on the packages themselves.

Source links point at QtWebEngine tag `v6.11.3` and at the Chromium commit that tag pins, so line
numbers stay valid. Links to Firefox, Brave, castLabs, Electron and Asahi code point at their head
commits on 2026-11-04. Web pages with no version were read on 2026-11-04. Nothing here is legal
advice: the licence section reports what the texts say.

## Engine capability

The engine can load Widevine with no build change on x86_64. Qt turns it on for every build, it
reads a file already on disk, and it finds one by itself in several places.

### Build time

Qt's configure step has no Widevine feature, and neither `configure.cmake` nor
`src/core/configure/BUILD.root.gn.in` sets a flag for it. The GN arguments in
`src/core/CMakeLists.txt` set `enable_library_cdms=true` and `enable_widevine=true` for every build,
on no condition.
[CMakeLists.txt](https://code.qt.io/cgit/qt/qtwebengine.git/tree/src/core/CMakeLists.txt?h=v6.11.3#n360),
[CMakeLists.txt](https://code.qt.io/cgit/qt/qtwebengine.git/tree/src/core/CMakeLists.txt?h=v6.11.3#n378)
Chromium's own default for `enable_widevine` is off outside branded Chrome and Android, so this is
Qt's choice.
[widevine.gni](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/third_party/widevine/cdm/widevine.gni?id=bcc09a68b50a40c60d8dc83f0e6b6dc872614812#n15)

Qt patched `enable_widevine_cdm_component` to be false with `!is_qtwebengine`, so the engine has no
component installer.
[widevine.gni](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/third_party/widevine/cdm/widevine.gni?id=bcc09a68b50a40c60d8dc83f0e6b6dc872614812#n44)
`bundle_widevine_cdm` needs a branded build, so nothing is bundled either. That combination defines
`WIDEVINE_CDM_AVAILABLE_NOT_COMPONENT`, which compiles Qt's own lookup.
[content_client_qt.cpp](https://code.qt.io/cgit/qt/qtwebengine.git/tree/src/core/content_client_qt.cpp?h=v6.11.3#n44)

### Run time

`IsWidevineAvailable` builds a list of candidate paths and takes the first file that exists. If
`--widevine-path` is set, that path is the only candidate.
[content_client_qt.cpp](https://code.qt.io/cgit/qt/qtwebengine.git/tree/src/core/content_client_qt.cpp?h=v6.11.3#n195),
[content_switches.cc](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/content/public/common/content_switches.cc?id=bcc09a68b50a40c60d8dc83f0e6b6dc872614812#n1001)
There is no environment variable of its own. The switch travels in `QTWEBENGINE_CHROMIUM_FLAGS`.
Without the switch, on 64-bit Linux, candidates come from one plugin path, then three families of
directories, then fixed paths. The order is:

1. `libwidevinecdm.so` in Qt's plugins path plus `/webengine`, or in the application directory when
   that does not exist.
   [content_client_qt.cpp](https://code.qt.io/cgit/qt/qtwebengine.git/tree/src/core/content_client_qt.cpp?h=v6.11.3#n68)
1. `~/.config/google-chrome/WidevineCdm`, following the `latest-component-updated-widevine-cdm` hint
   file when there is one.
   [content_client_qt.cpp](https://code.qt.io/cgit/qt/qtwebengine.git/tree/src/core/content_client_qt.cpp?h=v6.11.3#n289)
1. `gmp-widevinecdm` in every profile under `~/.mozilla/firefox`.
   [content_client_qt.cpp](https://code.qt.io/cgit/qt/qtwebengine.git/tree/src/core/content_client_qt.cpp?h=v6.11.3#n294)
1. `~/.config/chromium/WidevineCdm`, again following the hint.
   [content_client_qt.cpp](https://code.qt.io/cgit/qt/qtwebengine.git/tree/src/core/content_client_qt.cpp?h=v6.11.3#n305)
1. In each of those directories and their version subdirectories, a bare `libwidevinecdm.so`, then
   `_platform_specific/linux_x64/libwidevinecdm.so`.
   [content_client_qt.cpp](https://code.qt.io/cgit/qt/qtwebengine.git/tree/src/core/content_client_qt.cpp?h=v6.11.3#n315)
1. Fixed paths: `/usr/lib/chromium/libwidevinecdm.so`,
   `/usr/lib/chromium-browser/libwidevinecdm.so`, `/usr/lib64/chromium/libwidevinecdm.so`,
   `/usr/lib64/chromium-browser/WidevineCdm/_platform_specific/linux_x64/libwidevinecdm.so`,
   `/opt/google/chrome/WidevineCdm/_platform_specific/linux_x64/libwidevinecdm.so` and
   `/opt/google/chrome/libwidevinecdm.so`.
   [content_client_qt.cpp](https://code.qt.io/cgit/qt/qtwebengine.git/tree/src/core/content_client_qt.cpp?h=v6.11.3#n326)

Omaweb does not stand in the way. Its launch audit refuses debugging, security-lowering and
sandbox-disabling switches, and `--widevine-path` is none of those, so a reader's
`QTWEBENGINE_CHROMIUM_FLAGS` reaches the engine.
[DevelopmentLaunch.cpp](../../src/core/DevelopmentLaunch.cpp), [main.cpp](../../src/main.cpp)

One consequence. On a reader's machine with Google Chrome installed, or a Firefox profile that has
fetched its CDM, stock engine code finds a CDM with no action by Omaweb or the reader. Finding a CDM
also loads it. On Linux, `PreloadLibraryCdms` loads every registered CDM in the zygote before the
sandbox starts, without initializing it.
[content_main_runner_impl.cc](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/content/app/content_main_runner_impl.cc?id=bcc09a68b50a40c60d8dc83f0e6b6dc872614812#n393)
The shipped engine does this, as [the probes](#what-this-engine-does-here) show.

Qt's documentation describes the same behaviour. Its HTML5 DRM section, quoted from the
[source at the tag](https://code.qt.io/cgit/qt/qtwebengine.git/tree/src/core/doc/src/qtwebengine-features.qdoc?h=v6.11.3#n430)
and matching the [6.11 page](https://doc.qt.io/qt-6.11/qtwebengine-features.html#html5-drm), says:

> Qt WebEngine supports displaying DRM-protected videos if the Widevine CDM plugin has been
> installed. The plugin comes only in a binary format, so it can hide DRM decryption implementation
> details. You can obtain it from a third party, or from a Google Chrome installation.
>
> The support for HTML5 DRM is provided by Chromium.
>
> Note: The Widevine plugin is not shipped with Qt WebEngine.
>
> On startup, Qt WebEngine looks for the Widevine CDM plugin in several well-known locations, like
> the default Google Chrome installation directory, or Linux distro-specific paths. You can also
> pass the plugin location with `QTWEBENGINE_CHROMIUM_FLAGS` using `widevine-path`.
>
> The video format most commonly used by DRM services, H.264, requires proprietary audio and video
> codecs.

That last sentence matters here too: Qt adds H.264 to the CDM's capabilities only when
`USE_PROPRIETARY_CODECS` is set.
[content_client_qt.cpp](https://code.qt.io/cgit/qt/qtwebengine.git/tree/src/core/content_client_qt.cpp?h=v6.11.3#n346)
[#914](https://github.com/villekivela/omaweb/issues/914) records that flag on for x86_64. The
aarch64 package 6.11.3-1 lists `webengine_proprietary_codecs` among its enabled features too.

### Which CDM versions load

The pin accepts CDM module version 4, CDM interfaces 10 and 11, and host interfaces 10 to 12.
Interface 12 is supported but off by default.
[content_decryption_module.h](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/media/cdm/api/content_decryption_module.h?id=bcc09a68b50a40c60d8dc83f0e6b6dc872614812#n16),
[supported_cdm_versions.h](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/media/cdm/supported_cdm_versions.h?id=bcc09a68b50a40c60d8dc83f0e6b6dc872614812#n37)

Chrome checks `manifest.json` first. It reads `x-cdm-module-versions`, `x-cdm-interface-versions`
and `x-cdm-host-versions` for compatibility, then `version`, `x-cdm-codecs`,
`x-cdm-persistent-license-support` and `x-cdm-supported-encryption-schemes` for capability.
[cdm_manifest.cc](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/components/cdm/common/cdm_manifest.cc?id=bcc09a68b50a40c60d8dc83f0e6b6dc872614812#n287)

Qt's path skips all of that. It never opens `manifest.json`, and a comment says so.
[content_client_qt.cpp](https://code.qt.io/cgit/qt/qtwebengine.git/tree/src/core/content_client_qt.cpp?h=v6.11.3#n314)
It hardcodes VP8, VP9, AV1, H.264, HEVC when built, `cenc`, `cbcs` and temporary sessions, "as if
they came from the component manifest", and registers an empty version.
[content_client_qt.cpp](https://code.qt.io/cgit/qt/qtwebengine.git/tree/src/core/content_client_qt.cpp?h=v6.11.3#n343),
[content_client_qt.cpp](https://code.qt.io/cgit/qt/qtwebengine.git/tree/src/core/content_client_qt.cpp?h=v6.11.3#n387)
So a CDM from another Chrome version registers whatever it is. Compatibility is decided later, by
the library itself. A wrong module version shows as a missing `InitializeCdmModule_4` entry point,
and `CdmWrapper::Create` walks interfaces 12, 11, 10, skips the disabled 12, and fails if the CDM
offers neither of the others.
[cdm_module.cc](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/media/cdm/cdm_module.cc?id=bcc09a68b50a40c60d8dc83f0e6b6dc872614812#n141),
[cdm_wrapper.h](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/media/cdm/cdm_wrapper.h?id=bcc09a68b50a40c60d8dc83f0e6b6dc872614812#n362)
A mismatch would therefore look like Widevine being offered to a page and then failing.

Chromium `main` still enables interfaces 10 and 11.
[supported_cdm_versions.h](https://chromium.googlesource.com/chromium/src/+/4c7244a405d326e284710231ff609168dceb9044/media/cdm/supported_cdm_versions.h#38)
A CDM that current Chrome can load therefore offers an interface the pin also accepts, as far as the
interface check goes. That is an inference from the two headers. Nobody has loaded one here, and the
module version on `main` was not confirmed because that header did not fetch at the same path.

The hardcoded list also says what the CDM can do, whatever it really does. Qt's comment says the
list "must match the CDM that is being bundled with Chrome", and nothing checks that it does.

### Security level and sessions

The CDM registers as `Robustness::kSoftwareSecure`, and on Linux the highest video robustness the
key system offers is `SW_SECURE_DECODE`.
[content_client_qt.cpp](https://code.qt.io/cgit/qt/qtwebengine.git/tree/src/core/content_client_qt.cpp?h=v6.11.3#n388),
[key_system_support_update.cc](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/components/cdm/renderer/key_system_support_update.cc?id=bcc09a68b50a40c60d8dc83f0e6b6dc872614812#n383)
Persistent licences are out twice over. Qt passes `can_persist_data=false`, and
`CanSupportPersistentLicense` needs CDM host verification, which Linux does not build.
[content_renderer_client_qt.cpp](https://code.qt.io/cgit/qt/qtwebengine.git/tree/src/core/renderer/content_renderer_client_qt.cpp?h=v6.11.3#n521),
[key_system_support_update.cc](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/components/cdm/renderer/key_system_support_update.cc?id=bcc09a68b50a40c60d8dc83f0e6b6dc872614812#n259)

### Updates

There are none. The engine has no component updater for the CDM, reads a file already on disk, and
because the zygote loads it before the sandbox, the file must be there when the browser starts. A
new CDM takes effect at the next launch. Anything that refreshes it is outside the engine.

Chromium keeps the licence at `third_party/widevine/LICENSE` and the readme at
`third_party/widevine/README.chromium`, both read below. There is no `cdm/LICENSE` or `cdm/README`
at the pin. The installer `widevine_cdm_component_installer.cc` is absent from Qt's tree at the
paths tried. A search of every `*.gn` and `*.gni` at the pin, through the GitHub mirror of the same
commit, found `enable_library_widevine_cdm` only in `widevine.gni`.

## aarch64

**The commonly repeated story is out of date.** Chrome has an arm64 Linux build now, and Google
appears to publish a Linux arm64 CDM. Qt's lookup does not find it.

### What Google publishes

No Linux arm64 CDM from Google was known before 2026.

- Bryce Seager van Dyk wrote on the Firefox bug in February 2020 that "CDM vendors are not shipping
  CDMs for these architectures".
  [Bug 1612995, comment 2](https://bugzilla.mozilla.org/show_bug.cgi?id=1612995#c2)
- The CDMs in circulation did come from ChromeOS. Asahi's installer downloads a ChromeOS Lacros
  image from Google's `chromeos-localmirror` and extracts
  `WidevineCdm/_platform_specific/cros_arm64/libwidevinecdm.so`. It then rewrites the file, and
  tells the reader these are "ARM64 builds intended for ChromeOS images" and "not supported nor
  endorsed by Google".
  [widevine-installer](https://github.com/AsahiLinux/widevine-installer/blob/eab8c668ad3f9db27a444ee1f94b82d8f3ab5336/widevine-installer)
- Raspberry Pi ships such a CDM as `libwidevinecdm0` 4.10.2662.3+1 for arm64, the same version that
  installer pins.
  [Package index](https://archive.raspberrypi.com/debian/dists/trixie/main/binary-arm64/Packages.gz)
  Its maintainer says they "did have to bodge the widevine binaries" for the Pi 5's page size, using
  Asahi's method.
  [Bug 1612995, comment 22](https://bugzilla.mozilla.org/show_bug.cgi?id=1612995#c22)

That changed in 2026.

- Google
  [announced on 12 March 2026](https://blog.google/chromium/bringing-chrome-to-arm64-linux-devices/)
  that it would "launch Chrome for ARM64 Linux devices in Q2 2026".
- Google's own apt repository now lists `arm64` beside `amd64`, with `google-chrome-stable`
  157.0.8103.38-1 for arm64. [Release](https://dl.google.com/linux/chrome/deb/dists/stable/Release),
  [Packages](https://dl.google.com/linux/chrome/deb/dists/stable/main/binary-arm64/Packages.gz)
- Chromium `main` adds `target_os == "linux" && target_cpu == "arm64"` to
  `library_widevine_cdm_available`, and bundling follows that value in a branded build.
  [widevine.gni](https://chromium.googlesource.com/chromium/src/+/4c7244a405d326e284710231ff609168dceb9044/third_party/widevine/cdm/widevine.gni#28)
- David Turner of Raspberry Pi wrote on 30 July 2026, "I've confirmed that Google are now providing
  Widevine CDM for Linux aarch64", and that it played in Firefox on a Pi 5. He later said the
  binaries "now seem to be linked with 64k alignment" and worked with the Pi 5's 16k pages.
  [Bug 1612995, comment 17](https://bugzilla.mozilla.org/show_bug.cgi?id=1612995#c17),
  [comment 22](https://bugzilla.mozilla.org/show_bug.cgi?id=1612995#c22)

Two limits on that evidence. No Google page found says a Linux arm64 CDM exists; the build file and
an engineer's comment do, and the arm64 deb was deliberately not opened. And Firefox has not shipped
it. At head, `eme_choices` still allows Widevine on Linux only for `x86` and `x86_64`, and
`widevinecdm.json` has no Linux aarch64 entry.
[moz.configure](https://github.com/mozilla-firefox/firefox/blob/98bddef2a90f3746379bea72892214ac35b825bd/toolkit/moz.configure#L1033),
[widevinecdm.json](https://github.com/mozilla-firefox/firefox/blob/98bddef2a90f3746379bea72892214ac35b825bd/toolkit/content/gmp-sources/widevinecdm.json#L27)

### What the pinned Chromium does on arm64

It treats arm64 Linux differently, but not in a way that stops Qt. At the pin,
`library_widevine_cdm_available` lists Linux on `x64` only.
[widevine.gni](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/third_party/widevine/cdm/widevine.gni?id=bcc09a68b50a40c60d8dc83f0e6b6dc872614812#n27)
That value feeds only `enable_library_widevine_cdm`, which feeds bundling and the component.
Bundling needs a branded build. The component flag, `ENABLE_WIDEVINE_CDM_COMPONENT`, is one Qt
reads, but `!is_qtwebengine` already forces it false, so the arm64 exclusion changes nothing there.
`ENABLE_WIDEVINE` does not depend on it.
[BUILD.gn](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/third_party/widevine/cdm/BUILD.gn?id=bcc09a68b50a40c60d8dc83f0e6b6dc872614812#n23)
So on aarch64 `ENABLE_WIDEVINE` is true, the component is false, and Qt's lookup compiles as it does
on x86_64. Nothing in the Qt build conditions these two arguments on the CPU.

The lookup itself is wrong for arm64. Qt picks the directory by word size, not by architecture, so a
64-bit arm64 build searches `_platform_specific/linux_x64/`, and every fixed `WidevineCdm` path says
`linux_x64` too.
[content_client_qt.cpp](https://code.qt.io/cgit/qt/qtwebengine.git/tree/src/core/content_client_qt.cpp?h=v6.11.3#n315),
[content_client_qt.cpp](https://code.qt.io/cgit/qt/qtwebengine.git/tree/src/core/content_client_qt.cpp?h=v6.11.3#n331)
A CDM in Chrome's layout at `_platform_specific/linux_arm64/` is not found. Only `--widevine-path`,
or a bare `libwidevinecdm.so` in a searched directory, reaches one. Qt's
[`dev` branch](https://code.qt.io/cgit/qt/qtwebengine.git/tree/src/core/content_client_qt.cpp?h=dev#n315)
has the same lines. Chromium's own path variable would give `linux_arm64`.
[cdm_paths.gni](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/media/cdm/library_cdm/cdm_paths.gni?id=bcc09a68b50a40c60d8dc83f0e6b6dc872614812#n29)

Automatic discovery on aarch64 would need a one-line engine patch. Loading through the switch needs
none. Asahi's README warns that Arch Linux ARM's glibc is too old for its rewritten CDM, which would
concern Omaweb's [aarch64 package](../adr/0044-build-the-aarch64-package-on-arch-linux-arm.md), but
the warning looks stale: the [Arch Linux ARM mirror](http://mirror.archlinuxarm.org/aarch64/core/)
carries glibc 2.43 against the 2.36 it asks for.

## Licence by delivery route

No public text gives Omaweb permission on any route. The texts that exist are general, and they
point at a separate agreement with Google.

### The clauses

The licence at the pin, which Chrome's build copies beside the CDM as `WidevineCdm/LICENSE`, reads:
[LICENSE](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/third_party/widevine/LICENSE?id=bcc09a68b50a40c60d8dc83f0e6b6dc872614812#n1),
[BUILD.gn](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/third_party/widevine/cdm/BUILD.gn?id=bcc09a68b50a40c60d8dc83f0e6b6dc872614812#n39)

> Google LLC and its affiliates ("Google") own all legal right, title and interest in and to the
> content decryption module software ("Software") and related documentation, including any
> intellectual property rights in the Software. You may not use, modify, sell, or otherwise
> distribute the Software without a separate license agreement with Google. The Software is not open
> source software.
>
> If you are interested in licensing the Software, please contact www.widevine.com

The
[README](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/third_party/widevine/README.chromium?id=bcc09a68b50a40c60d8dc83f0e6b6dc872614812)
gives `License: Commercial` and says "No third-party files are checked in here. If you are
interested in integrating Widevine CDM into your product, please visit the URL above."
`widevine.gni` says Widevine "Can be optionally enabled in Chromium on non-Android platforms" and
sends the reader to that LICENSE.

[Widevine's developer overview](https://developers.google.com/widevine/drm/overview) (last updated
2024-10-09) says: "A license agreement is required for the use of Widevine products or services."
and "Widevine does not assess any fee for use of its products and services." It lists Chromium
Browser and CEF / Electron as supported platforms, which is a statement of technology and not a
grant. The [Widevine DRM page](https://www.widevine.com/solutions/widevine-drm) describes
integration for device makers, starting with "Completion of legal agreements", and nothing for
browser vendors. No page found publishes the agreement.

Chrome's terms cover Chrome. The [Additional Terms](https://www.google.com/chrome/terms/) (last
modified 30 September 2025) do not mention Widevine. They send the reader to the
[Google Terms of Service](https://policies.google.com/terms) (effective 30 July 2026), which say:

> Some of our services include downloadable or preloaded software. We give you permission to use
> that software as part of the services.

The licence is "personal" and "non-assignable", and the section ends:

> You may not copy, modify, distribute, sell, or lease any part of our services or software.

### Shipped in Omaweb's pacman repository

Refused without an agreement. Publishing the file is distribution, which LICENSE names. Technically
it is the easiest route: a package drops the library where `webenginePluginsPath()` looks, or passes
`--widevine-path`.

Raspberry Pi does ship one. Its package index lists it, and no public statement of the terms was
found, so this shows it happens and not that anyone may.

### Downloaded by Omaweb

No public clause permits it, and the general clause above applies. Technically Omaweb would have to
write this itself, because the engine has no component updater.

What downstreams say, in their own words:

- Brave shows: "It's a Google extension loaded from Google servers, which Brave cannot inspect. By
  installing, you'll agree to Google's terms of use."
  [brave_generated_resources.grd](https://github.com/brave/brave-core/blob/c586364fb3f8ea7af9a97bf70e34eb77e4dcde5c/app/brave_generated_resources.grd#L383)
  It names no agreement of its own and points the reader at Google's.
- Firefox labels it "Widevine Content Decryption Module provided by Google Inc." and fetches it from
  Google's component URLs, with a licence link that goes to Google's privacy policy.
  [aboutAddons.ftl](https://github.com/mozilla-firefox/firefox/blob/98bddef2a90f3746379bea72892214ac35b825bd/toolkit/locales/en-US/toolkit/about/aboutAddons.ftl#L600),
  [widevinecdm.json](https://github.com/mozilla-firefox/firefox/blob/98bddef2a90f3746379bea72892214ac35b825bd/toolkit/content/gmp-sources/widevinecdm.json#L27),
  [GMPProvider.sys.mjs](https://github.com/mozilla-firefox/firefox/blob/98bddef2a90f3746379bea72892214ac35b825bd/toolkit/mozapps/extensions/internal/GMPProvider.sys.mjs#L72)
  The nearest thing to a statement of permission is a Mozilla engineer on arm64 in August 2026:
  "We'll have to first discuss with Google about shipping officially". So Mozilla asks.
  [Bug 1612995, comment 19](https://bugzilla.mozilla.org/show_bug.cgi?id=1612995#c19)
- Vivaldi's [help page](https://help.vivaldi.com/desktop/troubleshoot/widevine/) only tells the
  reader to use "Check for updates" at `vivaldi://components`. It says nothing on licence.
- Electron says it "does not download, update, license, or sign Widevine CDM files for applications"
  and that developers must obtain "any required CDM files, licenses, and VMP signing support from
  Widevine or an authorized integration provider".
  [testing-widevine-cdm.md](https://github.com/electron/electron/blob/6752821287dd7eae86525b5a63d53a2040cb80a3/docs/tutorial/testing-widevine-cdm.md?plain=1#L36)
- castLabs' Electron fork installs it through the Component Updater and states a policy that is "no
  longer official information": a CDM stays available for about a year of Chromium releases, and
  beyond that "the ability to do so can be revoked at any time". It puts Chromium 140, this engine's
  version, outside that window.
  [ECS wiki](https://github.com/castlabs/electron-releases/wiki#supported-versions)

None of them quotes a clause that lets them do it. Each either has an agreement it does not publish
or does not say. Two non-Google accounts describe what that agreement is like, and both are hearsay
about Google's position:

- Marshall Greenblatt, writing about CEF, said in 2015 that after talking with Widevine he
  understood the CDM to be free provided it is downloaded from Google through Chromium's component
  updater, under an MLA with no cost.
  [chromium-dev](https://groups.google.com/a/chromium.org/g/chromium-dev/c/16hDHEpgUb8)
- Samuel Maddock, building an MIT-licensed Electron browser, wrote in 2019 that after four months
  Widevine answered that it was "not supporting an open source solution like this".
  [Blog](https://blog.samuelmaddock.com/posts/google-widevine-blocked-my-browser/) Omaweb is MPL
  2.0.

Greenblatt's account is the only one that explains the shape of the practice: download from Google,
never redistribute. It is eleven years old and not Google's text, so it settles nothing.

### Taken from a Chrome the reader installed

No public clause permits it. On x86_64 this route needs no code, because the engine already looks
there. Qt's documentation says "You can obtain it from a third party, or from a Google Chrome
installation", and Qt's lookup also names Firefox's and Chromium's directories. That is Qt
describing its software. Qt cannot grant a licence to Google's file.

The reader, not Omaweb, is the one using the file, and the ToS permission reaches it only "as part
of the services". Whether a Chrome install's permission extends to a different host is not answered
by any text found.

### A local experiment

**Copying a CDM out of a Chrome install to load it elsewhere is not clearly permitted, and on the
plain wording it is refused.** Two clauses reach it. The ToS says "You may not copy" any part of the
software, and it binds anyone using Chrome. LICENSE says "You may not use" the Software without a
separate agreement, and the only permission on offer is use "as part of the services". No clause
carves out private testing.

What stays unsettled is narrower:

- Whether the same rule reaches a CDM loaded in place, with no copy made, as the engine's own lookup
  does. Only "use" applies there, and whether the ToS is the "separate license agreement" is not
  stated anywhere.
- How either clause meets mandatory law, such as consumer or interoperability rights. The ToS itself
  preserves "all legal rights granted to consumers". That was not researched.
- Whether Google enforces any of this. No statement found applies these clauses to a private test.

A probe of the engine's lookup that uses no real CDM is unaffected: an empty file at a searched path
shows the search without any Google software. The probes below do that and go no further, so no real
CDM was loaded for this note.

An experiment can also happen by accident. If Chrome is installed on the machine where the engine
runs, the engine picks up its CDM, as described under run time.

## Verified media path and services

### Security levels

Google's material on levels is thin in public. The architecture PDF returned 403. Chrome's developer
blog calls Level 1 the highest, "where all content processing, cryptography, and control is
performed within the Trusted Execution Environment", and labels `HW_SECURE_ALL` as L1 and
`SW_SECURE_DECODE` as L3.
[Media updates in Chrome 62](https://developer.chrome.com/blog/media-updates-in-chrome-62) As shown
above, `SW_SECURE_DECODE` is the most Linux offers at the pin, so Omaweb's ceiling is L3. Firefox's
source agrees for its own browser: "The Widevine L1 plugin is currently only available for Windows
x64."
[GMPUtils.sys.mjs](https://github.com/mozilla-firefox/firefox/blob/98bddef2a90f3746379bea72892214ac35b825bd/toolkit/modules/GMPUtils.sys.mjs#L54)

### What VMP is

VMP verifies the browser's binaries. Widevine's original announcement no longer renders on
[its news page](https://www.widevine.com/news), but Electron quotes it: "VMP provides a method to
verify the authenticity of a device platform. For browser deployments, this will provide an
additional signal to determine if a browser-based implementation is reliable and secure."
[testing-widevine-cdm.md](https://github.com/electron/electron/blob/6752821287dd7eae86525b5a63d53a2040cb80a3/docs/tutorial/testing-widevine-cdm.md?plain=1#L23)

**VMP does not exist on Linux, so the issue's question falls away for Omaweb.** At the pin,
`enable_cdm_host_verification` needs Mac or Windows and a branded build, and an assert says "CDM
host verification is only supported on Mac and Windows."
[media_options.gni](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/media/media_options.gni?id=bcc09a68b50a40c60d8dc83f0e6b6dc872614812#n270),
[media_options.gni](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/media/media_options.gni?id=bcc09a68b50a40c60d8dc83f0e6b6dc872614812#n286)
Binary signing for the CDM is gated on that same flag.
[widevine.gni](https://code.qt.io/cgit/qt/qtwebengine-chromium.git/tree/chromium/third_party/widevine/cdm/widevine.gni?id=bcc09a68b50a40c60d8dc83f0e6b6dc872614812#n67)
castLabs, a Widevine partner, states the same: "Since the `Linux` Widevine CDM does not support VMP,
and thus does not require a VMP signature, EVS does not provide signing of `Linux` binaries."
[EVS wiki](https://github.com/castlabs/electron-releases/wiki/EVS) Its status table lists
`PLATFORM_UNVERIFIED` as "Expected on Linux. On macOS/Windows this means production streaming
services will typically not work."
[VMP wiki](https://github.com/castlabs/electron-releases/wiki/VMP)

So on Linux, Chrome and stock Chromium are alike to the licence server on this signal. A service
that turns away a Chromium derivative on Linux is doing so for another reason. That sentence is an
inference from the sources above, and no service states it.

### Who can sign

This matters only if Omaweb ever ships on Windows or macOS, and
[macOS distribution is a non-goal](../product/requirements.md).

- Google, directly. castLabs says that "Previously a license agreement with Google Widevine was
  required to get your own VMP signing certificate". The Google pages read for this note do not
  describe the process. The [Widevine DRM page](https://www.widevine.com/solutions/widevine-drm) is
  the nearest.
  [ECS README](https://github.com/castlabs/electron-releases/blob/059d79860d288044df48e8839d9c621dee4a17e1/README.md?plain=1#L17)
- castLabs EVS, a free service, signs only macOS and Windows packages "derived from official
  releases of Electron for Content Security". Omaweb's engine is not one.
  [EVS wiki](https://github.com/castlabs/electron-releases/wiki/EVS)
- castLabs' commercial route for "custom Chromium/Electron adaptations", through the
  [Widevine 3PL](https://www.widevine.com/solutions/widevine-3pl) programme. It involves "an audit
  of the application and the relevant code". Google's 3PL page names castLabs among its partners.
  castLabs' own wiki records that in May 2022 it paused EVS for ECS "At the request of Widevine".
  [ECS wiki](https://github.com/castlabs/electron-releases/wiki)

### What services document

Each row is the service's own help page. None mentions VMP. None mentions stock Chromium, except
Netflix.

| Service                                                                | Linux in a browser                                                                                                                     | Resolution                                                                                                                                | Chromium derivatives                                                                                                                                                                                 |
| ---------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| [Netflix](https://help.netflix.com/en/node/23742)                      | Chrome 117+, Firefox 129+, Edge 118+, Opera 92+. "we can't guarantee Netflix will work", and no support                                | Up to Full HD (1080p) for all four                                                                                                        | [Not supported](https://help.netflix.com/en/node/27451): "If you get this error but don't use Google Chrome, or you're using a Chromium-based web browser, your browser isn't supported by Netflix." |
| [Prime Video](https://www.primevideo.com/help?nodeId=GUX9FYHU5D8LC9EJ) | "available via a web browser on a computer running Windows, macOS, Chrome OS, or Linux", latest Chrome, Firefox, Edge, Safari or Opera | "If you're running an operating system other than Windows or macOS, playback is restricted to standard definition."                       | Not stated                                                                                                                                                                                           |
| [Spotify](https://support.spotify.com/us/article/web-player-help/)     | Lists Chrome, Firefox, Edge, Opera and Safari for desktop. No mention of Linux                                                         | Audio                                                                                                                                     | Not stated. Its "Playback of protected content is not enabled" entry says "you need to enable the Widevine plugin"                                                                                   |
| [YouTube](https://support.google.com/youtube/answer/78358?hl=en)       | Paid movies and shows need the "Newest version of Google Chrome, Firefox, or Safari" on "Windows 7+, Mac OS X 10.7+, or Ubuntu 10+"    | A note under the bitrate table says: "Playback in HD is unavailable for streaming on a browser, except for Safari for HD streaming only." | Not stated                                                                                                                                                                                           |

That Netflix line is on a page for one error code. It does not say how Netflix decides, or whether
it blocks such a browser or only declines to support it. Its
[Firefox error page](https://help.netflix.com/en/node/32661) also blames "a custom version of
Firefox". Omaweb sends the engine's user agent minus the `QtWebEngine` token, so it reads as a
recent Chrome ([requirements](../product/requirements.md)). Whether that gets past Netflix is
unknown, and Omaweb [offers no user-agent tricks](../product/requirements.md) to help.

Disney+, Apple TV, Hulu and Max gave no usable first-party text. The Disney+ help pages render empty
outside a browser. Search snippets of
[its browser article](https://help.disneyplus.com/en-GB/article/disneyplus-computer-browser-requirements)
say it supports the latest Chrome, Firefox, Edge and Opera on Windows 10 and newer, and name neither
Linux nor a resolution. That is a snippet of a first-party page and was not read in full. Snippets
of [Apple's guide](https://support.apple.com/en-sg/guide/tvplus/apdc0cb7ad64/web) say "any web
browser, such as Chrome or Firefox"; the page returned 404 when fetched.

### What users report

None of this is first-party. All of it is old, and none tests stock Chromium on x86_64 today.

- David Buchanan, writing on Asahi Linux in 2023 with a ChromeOS CDM, reports that "Netflix will
  refuse to work at all unless you have a ChromeOS useragent", that "Spotify's web player Just
  Works", and that Netflix defaulted to 720p with 1080p only if the client asks for it.
  [Blog](https://www.da.vidbuchanan.co.uk/blog/netflix-on-asahi.html)
- A Hacker News commenter in 2022 put Chrome's Netflix limit at 720p for L3 on macOS and Windows,
  which Netflix's table now contradicts.
  [Hacker News](https://news.ycombinator.com/item?id=32166359)
- A tester reports protected playback in a self-built Firefox on Linux arm64 with a native CDM in
  September 2026, without naming a service.
  [Bug 1612995, comment 25](https://bugzilla.mozilla.org/show_bug.cgi?id=1612995#c25)

The answer to the issue's question: on the first-party record, no service says it refuses a browser
for lacking VMP. Only Netflix says it does not support Chromium-based browsers, and it says that by
browser family. A reader who got past that would see at most 1080p on Netflix and SD on Prime Video,
the same as Chrome on Linux. None of these pages offers 4K on Linux.

## What this engine does here

The probes ran on 2026-11-04 with no Widevine CDM on the machine and no real CDM at any point. They
show what the engine does up to loading a library, and nothing after that.

A small program in a scratch directory, linked against the installed `omaweb-qtwebengine` 6.11.3-1
on x86_64, opened an off-the-record profile at `https://localhost/`, where `isSecureContext` was
true. It called `navigator.requestMediaKeySystemAccess` for each key system with a `cenc` H.264
configuration, then `createMediaKeys`. It ran offscreen under `dbus-run-session` with a throwaway
`HOME`, so the lookup saw only what the probe put there. Clear Key is the control: it needs no
library, so it shows that EME itself works in every run.

| Setup                                                            | `org.w3.clearkey` | `com.widevine.alpha` access | `createMediaKeys`                                |
| ---------------------------------------------------------------- | ----------------- | --------------------------- | ------------------------------------------------ |
| Nothing                                                          | Granted, created  | `NotSupportedError`         | Not reached                                      |
| `--widevine-path` at a file that does not exist                  | Granted, created  | `NotSupportedError`         | Not reached                                      |
| `--widevine-path` at an empty `libwidevinecdm.so`                | Granted, created  | Granted                     | `NotSupportedError: CreateCdmFunc not available` |
| Empty file under `~/.config/google-chrome/WidevineCdm/1.0.0/...` | Granted, created  | Granted                     | `NotSupportedError: CreateCdmFunc not available` |

This confirms four things on the shipped x86_64 engine:

- It takes the switch, and it finds a file by its own lookup with no switch.
- It offers Widevine to a page on the strength of the file existing. Nothing inspects the file, so a
  page is told yes before anything has loaded.
- It tries to load the file at startup. The log says `Unable to load CDM ... file too short` from
  `content_main_runner_impl.cc:401`, which is `PreloadLibraryCdms`, before any page asks.
- With the preload failed, the CDM process could not open the file either:
  `cannot open shared object file: Operation not permitted`. A CDM that does not preload will not
  load later.

On aarch64 the probes did not run, because this machine is x86_64. The 6.11.3-1 aarch64 package's
`libQt6WebEngineCore` carries `widevine-path`, `com.widevine.alpha`, the `WidevineCdm` search
directories and the string `/_platform_specific/linux_x64/libwidevinecdm.so`, and carries no
`linux_arm64` path. So the shipped binary agrees with the source: the loader is there, and its
lookup names the wrong directory.

What stays unproven:

- That a real CDM loads and decrypts on this engine. The source points that way and Qt documents it.
  The licence section is why nobody tried.
- That Omaweb itself behaves as the probe program did. Omaweb's source has no EME or key system code
  and its launch flags do not touch CDMs, so nothing suggests otherwise, but Omaweb was not run.
- Any service. Testing one needs a signed-in account and a real CDM.

## Recommendation

**Keep the non-goal, and file no implementation work.**

The blocker is the licence, not the engine. On x86_64 the engine needs nothing built, and on aarch64
only discovery needs a one-line fix. VMP does not apply on Linux. But every route that Omaweb would
own needs permission that no public text gives. Shipping is refused without an agreement, and a
download has no clause behind it. The route that needs no code, a reader's own Chrome, leaves Omaweb
telling readers to rely on a permission nobody has stated. Omaweb would also be working for a return
capped at 1080p on Netflix, which by its own page does not support Chromium-based browsers, and SD
on Prime Video.

What would reopen this is a signed agreement, or a written answer from Widevine. Google says it
charges no fee and tells readers of LICENSE to contact it, so one enquiry is cheap. Maddock's
account suggests it may go unanswered, or be refused, for an open source browser. That is a
maintainer's call and not engineering work.

One finding does not wait for that. The non-goal is not what the engine does. By the source and the
probes, the shipped engine looks for a CDM at startup, so on a machine with Chrome or a Firefox CDM
it would load Google's closed library into the zygote before the sandbox and offer Widevine to
pages, with no action by Omaweb or the reader. So keeping the non-goal leaves a choice for the
maintainer: remove the lookup in the patch series, or have the requirements say that it happens.
Neither is feature work, and this note makes neither change.
