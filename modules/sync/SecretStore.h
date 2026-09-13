#pragma once

#include <QByteArray>
#include <QString>

namespace omaweb {

class SecretStore {
public:
    virtual ~SecretStore() = default;

    SecretStore(const SecretStore &) = delete;
    SecretStore &operator=(const SecretStore &) = delete;

    virtual bool store(
        const QString &name, const QByteArray &secret, QString *errorMessage = nullptr) = 0;
    virtual QByteArray retrieve(const QString &name, QString *errorMessage = nullptr) = 0;
    virtual bool remove(const QString &name, QString *errorMessage = nullptr) = 0;

protected:
    SecretStore() = default;
};

} // namespace omaweb
