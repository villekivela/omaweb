#include "QtHeldDownloads.h"

#include <QUuid>
#include <QtWebEngineCore/QWebEngineDownloadRequest>

namespace omaweb {

QtHeldDownloads::QtHeldDownloads(QObject *parent)
    : QObject(parent)
{
}

QString QtHeldDownloads::hold(QObject *download)
{
    auto *request = qobject_cast<QWebEngineDownloadRequest *>(download);
    if (!request) {
        return {};
    }
    HeldDownload held { request->property("view").value<QObject *>(), request->url(),
        request->downloadFileName().isEmpty() ? request->suggestedFileName()
                                              : request->downloadFileName() };
    request->cancel();
    if (!held.view || !held.sourceUrl.isValid()) {
        return {};
    }
    const auto token = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_held.insert(token, std::move(held));
    return token;
}

QVariantMap QtHeldDownloads::held(const QString &token) const
{
    const auto it = m_held.constFind(token);
    if (it == m_held.cend() || !it->view) {
        return {};
    }
    return { { QStringLiteral("sourceUrl"), it->sourceUrl },
        { QStringLiteral("fileName"), it->fileName },
        { QStringLiteral("view"), QVariant::fromValue(it->view.data()) } };
}

bool QtHeldDownloads::discard(const QString &token) { return m_held.remove(token) > 0; }

int QtHeldDownloads::heldCount() const { return static_cast<int>(m_held.size()); }

} // namespace omaweb
