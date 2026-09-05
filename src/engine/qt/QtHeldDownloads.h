#pragma once

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QUrl>
#include <QVariantMap>

namespace omaweb {

class QtHeldDownloads final : public QObject {
    Q_OBJECT

public:
    explicit QtHeldDownloads(QObject *parent = nullptr);

    // Qt requires a synchronous download decision. Holding cancels the request
    // before it writes data and retains enough state to request the file again.
    Q_INVOKABLE QString hold(QObject *download);

    Q_INVOKABLE QVariantMap held(const QString &token) const;

    Q_INVOKABLE bool discard(const QString &token);

    Q_INVOKABLE int heldCount() const;

private:
    struct HeldDownload {
        QPointer<QObject> view;
        QUrl sourceUrl;
        QString fileName;
    };

    QHash<QString, HeldDownload> m_held;
};

} // namespace omaweb
