#include "DownloadPolicy.h"

#include <QHash>

#include <algorithm>

namespace omaweb::DownloadPolicy {

namespace {

// The extension the desktop will read once the file is on disk. Filesystems
// that accept a trailing dot or space drop it, and the ones that do not never
// see the name at all, so the extension that matters is the one left after they
// are taken off — otherwise "setup.exe. " reads as an unknown kind and lands as
// a program.
QString settledExtension(const QString &fileName)
{
    auto name = fileName.trimmed();
    while (!name.isEmpty() && (name.endsWith(QLatin1Char('.')) || name.endsWith(QLatin1Char(' ')))) {
        name.chop(1);
    }
    const auto separator = std::max(name.lastIndexOf(QLatin1Char('/')),
        name.lastIndexOf(QLatin1Char('\\')));
    if (separator >= 0) {
        name = name.mid(separator + 1);
    }
    const auto dot = name.lastIndexOf(QLatin1Char('.'));
    if (dot < 0) {
        return {};
    }
    return name.mid(dot + 1).toLower();
}

const QHash<QString, QString> &extensionKinds()
{
    static const QHash<QString, QString> kinds{
        // Run by the operating system as it stands.
        {QStringLiteral("exe"), QStringLiteral("executable")},
        {QStringLiteral("com"), QStringLiteral("executable")},
        {QStringLiteral("scr"), QStringLiteral("executable")},
        {QStringLiteral("pif"), QStringLiteral("executable")},
        {QStringLiteral("cpl"), QStringLiteral("executable")},
        {QStringLiteral("bin"), QStringLiteral("executable")},
        {QStringLiteral("run"), QStringLiteral("executable")},
        {QStringLiteral("elf"), QStringLiteral("executable")},
        {QStringLiteral("out"), QStringLiteral("executable")},
        {QStringLiteral("jar"), QStringLiteral("executable")},
        {QStringLiteral("dll"), QStringLiteral("executable")},
        {QStringLiteral("so"), QStringLiteral("executable")},
        {QStringLiteral("dylib"), QStringLiteral("executable")},
        {QStringLiteral("wasm"), QStringLiteral("executable")},
        // Run by an interpreter the desktop already associates with them.
        {QStringLiteral("sh"), QStringLiteral("script")},
        {QStringLiteral("bash"), QStringLiteral("script")},
        {QStringLiteral("zsh"), QStringLiteral("script")},
        {QStringLiteral("fish"), QStringLiteral("script")},
        {QStringLiteral("ksh"), QStringLiteral("script")},
        {QStringLiteral("csh"), QStringLiteral("script")},
        {QStringLiteral("bat"), QStringLiteral("script")},
        {QStringLiteral("cmd"), QStringLiteral("script")},
        {QStringLiteral("ps1"), QStringLiteral("script")},
        {QStringLiteral("psm1"), QStringLiteral("script")},
        {QStringLiteral("vbs"), QStringLiteral("script")},
        {QStringLiteral("vbe"), QStringLiteral("script")},
        {QStringLiteral("wsf"), QStringLiteral("script")},
        {QStringLiteral("wsh"), QStringLiteral("script")},
        {QStringLiteral("hta"), QStringLiteral("script")},
        {QStringLiteral("js"), QStringLiteral("script")},
        {QStringLiteral("mjs"), QStringLiteral("script")},
        {QStringLiteral("jse"), QStringLiteral("script")},
        {QStringLiteral("py"), QStringLiteral("script")},
        {QStringLiteral("pl"), QStringLiteral("script")},
        {QStringLiteral("rb"), QStringLiteral("script")},
        {QStringLiteral("php"), QStringLiteral("script")},
        {QStringLiteral("lua"), QStringLiteral("script")},
        {QStringLiteral("scpt"), QStringLiteral("script")},
        {QStringLiteral("applescript"), QStringLiteral("script")},
        // A desktop entry names a command and an icon, and the desktop runs the
        // command when the reader opens what looks like a document.
        {QStringLiteral("desktop"), QStringLiteral("script")},
        // Hands something to a package manager or an application store.
        {QStringLiteral("msi"), QStringLiteral("installer")},
        {QStringLiteral("msix"), QStringLiteral("installer")},
        {QStringLiteral("msp"), QStringLiteral("installer")},
        {QStringLiteral("appx"), QStringLiteral("installer")},
        {QStringLiteral("pkg"), QStringLiteral("installer")},
        {QStringLiteral("mpkg"), QStringLiteral("installer")},
        {QStringLiteral("deb"), QStringLiteral("installer")},
        {QStringLiteral("rpm"), QStringLiteral("installer")},
        {QStringLiteral("apk"), QStringLiteral("installer")},
        {QStringLiteral("appimage"), QStringLiteral("installer")},
        {QStringLiteral("snap"), QStringLiteral("installer")},
        {QStringLiteral("flatpak"), QStringLiteral("installer")},
        {QStringLiteral("flatpakref"), QStringLiteral("installer")},
        {QStringLiteral("crx"), QStringLiteral("installer")},
        {QStringLiteral("xpi"), QStringLiteral("installer")},
        // Mounted, and then whatever is inside it is on the reader's machine.
        {QStringLiteral("dmg"), QStringLiteral("disk image")},
        {QStringLiteral("sparseimage"), QStringLiteral("disk image")},
        {QStringLiteral("sparsebundle"), QStringLiteral("disk image")},
        {QStringLiteral("iso"), QStringLiteral("disk image")},
        {QStringLiteral("img"), QStringLiteral("disk image")},
        {QStringLiteral("vhd"), QStringLiteral("disk image")},
        {QStringLiteral("vhdx"), QStringLiteral("disk image")},
        {QStringLiteral("vmdk"), QStringLiteral("disk image")},
        {QStringLiteral("qcow2"), QStringLiteral("disk image")},
        // Carries anything at all, including every kind above, past a check
        // that reads only the outermost name.
        {QStringLiteral("zip"), QStringLiteral("archive")},
        {QStringLiteral("tar"), QStringLiteral("archive")},
        {QStringLiteral("gz"), QStringLiteral("archive")},
        {QStringLiteral("tgz"), QStringLiteral("archive")},
        {QStringLiteral("bz2"), QStringLiteral("archive")},
        {QStringLiteral("tbz2"), QStringLiteral("archive")},
        {QStringLiteral("xz"), QStringLiteral("archive")},
        {QStringLiteral("txz"), QStringLiteral("archive")},
        {QStringLiteral("zst"), QStringLiteral("archive")},
        {QStringLiteral("lz"), QStringLiteral("archive")},
        {QStringLiteral("lzma"), QStringLiteral("archive")},
        {QStringLiteral("lha"), QStringLiteral("archive")},
        {QStringLiteral("cab"), QStringLiteral("archive")},
        {QStringLiteral("arj"), QStringLiteral("archive")},
        {QStringLiteral("7z"), QStringLiteral("archive")},
        {QStringLiteral("rar"), QStringLiteral("archive")},
        {QStringLiteral("war"), QStringLiteral("archive")},
        {QStringLiteral("ear"), QStringLiteral("archive")},
    };
    return kinds;
}

const QHash<QString, QString> &mimeKinds()
{
    static const QHash<QString, QString> kinds{
        {QStringLiteral("application/x-executable"), QStringLiteral("executable")},
        {QStringLiteral("application/x-msdownload"), QStringLiteral("executable")},
        {QStringLiteral("application/x-dosexec"), QStringLiteral("executable")},
        {QStringLiteral("application/x-mach-binary"), QStringLiteral("executable")},
        {QStringLiteral("application/vnd.microsoft.portable-executable"),
            QStringLiteral("executable")},
        {QStringLiteral("application/x-sharedlib"), QStringLiteral("executable")},
        {QStringLiteral("application/java-archive"), QStringLiteral("executable")},
        {QStringLiteral("application/x-sh"), QStringLiteral("script")},
        {QStringLiteral("application/x-shellscript"), QStringLiteral("script")},
        {QStringLiteral("text/x-shellscript"), QStringLiteral("script")},
        {QStringLiteral("application/x-csh"), QStringLiteral("script")},
        {QStringLiteral("application/x-bat"), QStringLiteral("script")},
        {QStringLiteral("application/x-powershell"), QStringLiteral("script")},
        {QStringLiteral("application/x-desktop"), QStringLiteral("script")},
        {QStringLiteral("application/x-msi"), QStringLiteral("installer")},
        {QStringLiteral("application/x-ms-installer"), QStringLiteral("installer")},
        {QStringLiteral("application/x-debian-package"), QStringLiteral("installer")},
        {QStringLiteral("application/vnd.debian.binary-package"), QStringLiteral("installer")},
        {QStringLiteral("application/x-rpm"), QStringLiteral("installer")},
        {QStringLiteral("application/x-redhat-package-manager"), QStringLiteral("installer")},
        {QStringLiteral("application/vnd.android.package-archive"),
            QStringLiteral("installer")},
        {QStringLiteral("application/x-xpinstall"), QStringLiteral("installer")},
        {QStringLiteral("application/x-apple-diskimage"), QStringLiteral("disk image")},
        {QStringLiteral("application/x-iso9660-image"), QStringLiteral("disk image")},
        {QStringLiteral("application/x-raw-disk-image"), QStringLiteral("disk image")},
        {QStringLiteral("application/zip"), QStringLiteral("archive")},
        {QStringLiteral("application/x-zip-compressed"), QStringLiteral("archive")},
        {QStringLiteral("application/x-tar"), QStringLiteral("archive")},
        {QStringLiteral("application/x-gtar"), QStringLiteral("archive")},
        {QStringLiteral("application/gzip"), QStringLiteral("archive")},
        {QStringLiteral("application/x-gzip"), QStringLiteral("archive")},
        {QStringLiteral("application/x-bzip2"), QStringLiteral("archive")},
        {QStringLiteral("application/x-xz"), QStringLiteral("archive")},
        {QStringLiteral("application/zstd"), QStringLiteral("archive")},
        {QStringLiteral("application/x-7z-compressed"), QStringLiteral("archive")},
        {QStringLiteral("application/vnd.rar"), QStringLiteral("archive")},
        {QStringLiteral("application/x-rar-compressed"), QStringLiteral("archive")},
    };
    return kinds;
}

} // namespace

QString riskKind(const QString &fileName, const QString &mimeType)
{
    const auto extension = settledExtension(fileName);
    if (!extension.isEmpty()) {
        const auto kind = extensionKinds().value(extension);
        if (!kind.isEmpty()) {
            return kind;
        }
    }
    // A server states the type in one field with optional parameters after a
    // semicolon, and the case of neither half is its own to choose.
    const auto declared = mimeType.section(QLatin1Char(';'), 0, 0).trimmed().toLower();
    if (declared.isEmpty()) {
        return {};
    }
    return mimeKinds().value(declared);
}

bool highRisk(const QString &fileName, const QString &mimeType)
{
    return !riskKind(fileName, mimeType).isEmpty();
}

} // namespace omaweb::DownloadPolicy
