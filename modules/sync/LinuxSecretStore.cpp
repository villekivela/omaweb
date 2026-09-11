#include "LinuxSecretStore.h"

#pragma push_macro("signals")
#undef signals
#include <libsecret/secret.h>
#pragma pop_macro("signals")

namespace omaweb {

namespace {

    const SecretSchema omawebSyncSchema = {.name = "dev.omaweb.browser.Sync",
        .flags = SECRET_SCHEMA_NONE,
        .attributes = {{"name", SECRET_SCHEMA_ATTRIBUTE_STRING}},
        .reserved = 0,
        .reserved1 = nullptr,
        .reserved2 = nullptr,
        .reserved3 = nullptr,
        .reserved4 = nullptr,
        .reserved5 = nullptr,
        .reserved6 = nullptr,
        .reserved7 = nullptr};

    void takeError(GError *error, QString *destination)
    {
        if (!error) {
            return;
        }
        if (destination) {
            *destination = QString::fromUtf8(error->message);
        }
        g_error_free(error);
    }

} // namespace

bool LinuxSecretStore::store(const QString &name, const QByteArray &secret, QString *errorMessage)
{
    GError *error = nullptr;
    const auto encoded
        = secret.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    const auto stored
        = secret_password_store_sync(&omawebSyncSchema, SECRET_COLLECTION_DEFAULT, "Omaweb Sync",
            encoded.constData(), nullptr, &error, "name", name.toUtf8().constData(), nullptr);
    takeError(error, errorMessage);
    return stored;
}

QByteArray LinuxSecretStore::retrieve(const QString &name, QString *errorMessage)
{
    GError *error = nullptr;
    auto *secret = secret_password_lookup_sync(
        &omawebSyncSchema, nullptr, &error, "name", name.toUtf8().constData(), nullptr);
    takeError(error, errorMessage);
    if (!secret) {
        return {};
    }
    const auto result = QByteArray::fromBase64(QByteArray(secret),
        QByteArray::Base64UrlEncoding | QByteArray::AbortOnBase64DecodingErrors);
    secret_password_free(secret);
    return result;
}

bool LinuxSecretStore::remove(const QString &name, QString *errorMessage)
{
    GError *error = nullptr;
    const auto removed = secret_password_clear_sync(
        &omawebSyncSchema, nullptr, &error, "name", name.toUtf8().constData(), nullptr);
    takeError(error, errorMessage);
    return removed;
}

} // namespace omaweb
