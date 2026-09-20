#include "ExtensionInstaller.h"

#include "ExtensionPackage.h"

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QtConcurrent/QtConcurrent>

namespace omaweb {

namespace {

    // The store answers a package request with a redirect to a storage host, so
    // the reply that matters is the second one.
    QNetworkRequest requestFor(const QUrl &address)
    {
        QNetworkRequest request(address);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
            QVariant::fromValue(QNetworkRequest::NoLessSafeRedirectPolicy));
        // Nothing about the reader goes with this. The endpoint needs a product
        // and a version, which are already in the address, and a browser asking
        // for a package has no reason to say who is running it.
        request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Omaweb"));
        return request;
    }

} // namespace

ExtensionInstaller::ExtensionInstaller(Ask ask, QObject *parent)
    : QObject(parent)
    , m_ask(ask)
{
}

bool ExtensionInstaller::fetching(const QString &key) const { return m_inFlight.value(key, false); }

void ExtensionInstaller::release(const QString &key)
{
    m_inFlight.remove(key);
    emit fetchingChanged(key, false);
}

void ExtensionInstaller::give(const QString &key, const QString &reason)
{
    release(key);
    emit failed(key, reason);
}

void ExtensionInstaller::fetch(const KnownExtension &extension, const QString &destination)
{
    if (extension.key.isEmpty() || fetching(extension.key)) {
        return;
    }
    if (m_ask == Ask::Never) {
        emit failed(extension.key, QStringLiteral("This Omaweb does not download extensions."));
        return;
    }
    m_inFlight.insert(extension.key, true);
    emit fetchingChanged(extension.key, true);
    download(extension, destination);
}

void ExtensionInstaller::refresh(const KnownExtension &extension, const QString &destination)
{
    if (extension.key.isEmpty() || fetching(extension.key)) {
        return;
    }
    const QString installed = ExtensionPackage::versionInstalled(destination);
    if (installed.isEmpty()) {
        fetch(extension, destination);
        return;
    }
    if (m_ask == Ask::Never) {
        return;
    }
    m_inFlight.insert(extension.key, true);
    emit fetchingChanged(extension.key, true);

    QNetworkReply *reply
        = m_network.get(requestFor(ExtensionPackage::updateAddress(extension.storeId)));
    connect(
        reply, &QNetworkReply::finished, this, [this, reply, extension, destination, installed] {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) {
                // Being offline is not a fault to report: the package on disk
                // goes on working, and the next day asks again.
                release(extension.key);
                return;
            }
            const QString offered = ExtensionPackage::versionOffered(reply->readAll());
            if (offered.isEmpty() || offered == installed) {
                release(extension.key);
                emit current(extension.key);
                return;
            }
            download(extension, destination);
        });
}

void ExtensionInstaller::download(const KnownExtension &extension, const QString &destination)
{
    QNetworkReply *reply
        = m_network.get(requestFor(ExtensionPackage::storeAddress(extension.storeId)));
    connect(reply, &QNetworkReply::finished, this, [this, reply, extension, destination] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            give(extension.key,
                QStringLiteral("The download did not finish. Check the network and try again."));
            return;
        }
        const QByteArray crx = reply->readAll();
        // Verifying and unpacking tens of megabytes would hold the window
        // still, and this runs while the reader is looking at the switch they
        // just pressed.
        auto *watcher = new QFutureWatcher<ExtensionPackage::Result>(this);
        connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, extension] {
            const ExtensionPackage::Result result = watcher->result();
            watcher->deleteLater();
            if (!result.ok) {
                give(extension.key, result.error);
                return;
            }
            release(extension.key);
            emit installed(extension.key);
        });
        watcher->setFuture(QtConcurrent::run([crx, extension, destination] {
            return ExtensionPackage::install(crx, extension, destination);
        }));
    });
}

} // namespace omaweb
