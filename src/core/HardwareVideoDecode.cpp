#include "HardwareVideoDecode.h"

#include <QString>

namespace omaweb {
namespace {

    // The feature that turns on VA-API decoding for a Chromium drawing through
    // GL, which is what QtWebEngine does: it renders offscreen into a texture
    // Qt Quick composites, so the Ozone and Vulkan spellings of this feature
    // are not the ones read.
    const auto decodeFeature = QStringLiteral("VaapiVideoDecodeLinuxGL");
    const auto enableFeatures = QStringLiteral("--enable-features");
    const auto disableFeatures = QStringLiteral("--disable-features");

    // A switch counts whether or not it carries a value, so `--disable-gpu` and
    // `--disable-gpu=1` are the same answer. `--disable-gpu-sandbox` is not one
    // of these, and is refused elsewhere.
    bool names(const QString &argument, const QStringList &flags)
    {
        for (const auto &flag : flags) {
            if (argument == flag || argument.startsWith(flag + QStringLiteral("="))) {
                return true;
            }
        }
        return false;
    }

    bool leavesNoGpuToDecodeOn(const QString &argument)
    {
        static const QStringList flags = {
            QStringLiteral("--disable-gpu"),
            QStringLiteral("--disable-accelerated-video-decode"),
        };
        return names(argument, flags);
    }

    QStringList features(const QString &argument)
    {
        const auto separator = argument.indexOf(u'=');
        if (separator == -1) {
            return {};
        }
        return argument.sliced(separator + 1).split(u',', Qt::SkipEmptyParts, Qt::CaseSensitive);
    }

} // namespace

QStringList hardwareVideoDecodeFlags(const QStringList &engineFlags)
{
    // Chromium reads one `--enable-features` list and one `--disable-features`
    // list, and the last of each on the command line is the one it reads.
    QStringList enabled;
    for (const auto &flag : engineFlags) {
        if (leavesNoGpuToDecodeOn(flag)) {
            return {};
        }
        if (names(flag, {disableFeatures}) && features(flag).contains(decodeFeature)) {
            return {};
        }
        if (names(flag, {enableFeatures})) {
            enabled = features(flag);
        }
    }
    if (enabled.contains(decodeFeature)) {
        return {};
    }
    // Omaweb's list goes last, so it carries the reader's own features with it:
    // a host that had to name a companion feature to get its driver working
    // keeps hardware decode rather than losing it to the naming.
    enabled.append(decodeFeature);
    return {enableFeatures + u'=' + enabled.join(u',')};
}

} // namespace omaweb
