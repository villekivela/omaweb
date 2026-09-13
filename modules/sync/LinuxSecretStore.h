#pragma once

#include "SecretStore.h"

namespace omaweb {

class LinuxSecretStore final : public SecretStore {
public:
    bool store(
        const QString &name, const QByteArray &secret, QString *errorMessage = nullptr) override;
    QByteArray retrieve(const QString &name, QString *errorMessage = nullptr) override;
    bool remove(const QString &name, QString *errorMessage = nullptr) override;
};

} // namespace omaweb
