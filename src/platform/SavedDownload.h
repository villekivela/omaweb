#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

namespace omaweb {

class SavedDownload final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool quarantineAvailable READ quarantineAvailable CONSTANT)

public:
    explicit SavedDownload(QObject *parent = nullptr);

    bool quarantineAvailable() const;

    // Record the source in platform metadata and remove execute permissions.
    Q_INVOKABLE bool quarantine(
        const QString &path, const QUrl &sourceUrl, const QUrl &pageUrl) const;

    Q_INVOKABLE bool reveal(const QString &path) const;
};

void registerSavedDownload();

} // namespace omaweb
