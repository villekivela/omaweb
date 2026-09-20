#pragma once

#include "KnownExtensions.h"

#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>

namespace omaweb {

// Fetches a Known extension's package from the store and puts it on disk.
//
// The request is here; every decision about what arrives is in ExtensionPackage
// and needs no network to make. That split is the point: what makes a download
// safe is a key that shipped with the build, so the code that trusts the key is
// testable without ever reaching Google.
//
// Omaweb asks the store only when the reader turns an extension on, and asks
// what version is offered at most once a day after that. It is the only thing
// in the browser that talks to Google, and it says so in Settings before it
// does.
class ExtensionInstaller final : public QObject {
    Q_OBJECT

public:
    // The lab and the tests stand this up without a network. `Never` refuses
    // every request rather than making one that cannot be answered.
    enum class Ask { Store, Never };

    explicit ExtensionInstaller(Ask ask = Ask::Store, QObject *parent = nullptr);

    // Fetch this extension and unpack it at `destination`, replacing whatever
    // is there. Does nothing when the same extension is already on its way.
    void fetch(const KnownExtension &extension, const QString &destination);

    // Fetch only if the store offers a version other than the one unpacked at
    // `destination`. A package that is absent is always fetched; one that is
    // current costs a few hundred bytes to confirm.
    void refresh(const KnownExtension &extension, const QString &destination);

    // Whether this extension is being fetched right now, for a surface that
    // wants to say so.
    bool fetching(const QString &key) const;

signals:
    // The package is on disk and can be loaded.
    void installed(const QString &key);
    // Nothing was written. `reason` is a sentence for the reader: a download
    // that fails silently leaves a switch that is on and an extension that is
    // not there.
    void failed(const QString &key, const QString &reason);
    // What is on disk is what the store offers, so nothing was fetched.
    void current(const QString &key);
    void fetchingChanged(const QString &key, bool fetching);

private:
    void download(const KnownExtension &extension, const QString &destination);
    void give(const QString &key, const QString &reason);
    void release(const QString &key);

    Ask m_ask;
    QNetworkAccessManager m_network;
    // Which extensions are in flight, so a reader pressing a switch twice does
    // not start two downloads over one folder.
    QHash<QString, bool> m_inFlight;
};

} // namespace omaweb
