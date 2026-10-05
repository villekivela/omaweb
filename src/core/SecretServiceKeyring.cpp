#include "PaymentCardKeyring.h"

#include <QtGlobal>

#pragma push_macro("signals")
#undef signals
#include <libsecret/secret.h>
#pragma pop_macro("signals")

namespace omaweb {

namespace {

    // The only things a keyring stores where they can be read without
    // unlocking it are the attributes and the label. The attributes are the
    // schema name, which libsecret adds, and a random identifier; the label is
    // the same for every card.
    const SecretSchema paymentCardSchema = {.name = "dev.omaweb.browser.PaymentCard",
        .flags = SECRET_SCHEMA_NONE,
        .attributes = {{"id", SECRET_SCHEMA_ATTRIBUTE_STRING}},
        .reserved = 0,
        .reserved1 = nullptr,
        .reserved2 = nullptr,
        .reserved3 = nullptr,
        .reserved4 = nullptr,
        .reserved5 = nullptr,
        .reserved6 = nullptr,
        .reserved7 = nullptr};

    constexpr auto label = "Omaweb payment card";
    constexpr auto serviceName = "org.freedesktop.secrets";

    // Logs the keyring's own message, which names a D-Bus error and may quote
    // an item's path or attributes. Those are the schema name and a card's
    // random identifier, never anything about the card: libsecret is never
    // given a secret it could quote back.
    void report(GError *error)
    {
        qWarning("The keyring answered: %s", error->message);
        g_error_free(error);
    }

    bool succeeded(GError *error)
    {
        if (!error) {
            return true;
        }
        report(error);
        return false;
    }

    // A Secret Service the bus offered that nothing answered for: it could not
    // be started, took no name, or did not reply in time.
    bool unanswered(const GError *error)
    {
        if (error->domain == G_IO_ERROR) {
            return error->code == G_IO_ERROR_TIMED_OUT || error->code == G_IO_ERROR_CLOSED
                || error->code == G_IO_ERROR_NOT_CONNECTED;
        }
        if (error->domain != G_DBUS_ERROR) {
            return false;
        }
        switch (error->code) {
        case G_DBUS_ERROR_SERVICE_UNKNOWN:
        case G_DBUS_ERROR_NAME_HAS_NO_OWNER:
        case G_DBUS_ERROR_NO_REPLY:
        case G_DBUS_ERROR_TIMEOUT:
        case G_DBUS_ERROR_TIMED_OUT:
        case G_DBUS_ERROR_DISCONNECTED:
        case G_DBUS_ERROR_NO_SERVER:
        case G_DBUS_ERROR_SPAWN_EXEC_FAILED:
        case G_DBUS_ERROR_SPAWN_FORK_FAILED:
        case G_DBUS_ERROR_SPAWN_CHILD_EXITED:
        case G_DBUS_ERROR_SPAWN_CHILD_SIGNALED:
        case G_DBUS_ERROR_SPAWN_FAILED:
        case G_DBUS_ERROR_SPAWN_SETUP_FAILED:
        case G_DBUS_ERROR_SPAWN_CONFIG_INVALID:
        case G_DBUS_ERROR_SPAWN_SERVICE_INVALID:
        case G_DBUS_ERROR_SPAWN_SERVICE_NOT_FOUND:
        case G_DBUS_ERROR_SPAWN_PERMISSIONS_INVALID:
        case G_DBUS_ERROR_SPAWN_FILE_INVALID:
        case G_DBUS_ERROR_SPAWN_NO_MEMORY:
            return true;
        default:
            return false;
        }
    }

    // Whether the session bus has a Secret Service on it or can start one. A
    // missing service is answered here rather than by a call that would fail
    // only after the bus had tried to start it.
    bool busOffersSecretService()
    {
        GError *error = nullptr;
        auto *bus = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
        if (!succeeded(error) || !bus) {
            return false;
        }
        const auto ask = [bus](const char *method, GVariant *parameters) {
            GError *failure = nullptr;
            auto *reply = g_dbus_connection_call_sync(bus, "org.freedesktop.DBus",
                "/org/freedesktop/DBus", "org.freedesktop.DBus", method, parameters, nullptr,
                G_DBUS_CALL_FLAGS_NONE, -1, nullptr, &failure);
            return succeeded(failure) ? reply : nullptr;
        };
        bool offered = false;
        if (auto *owner = ask("NameHasOwner", g_variant_new("(s)", serviceName))) {
            gboolean owned = FALSE;
            g_variant_get(owner, "(b)", &owned);
            offered = owned;
            g_variant_unref(owner);
        }
        if (!offered) {
            if (auto *activatable = ask("ListActivatableNames", nullptr)) {
                GVariantIter *names = nullptr;
                g_variant_get(activatable, "(as)", &names);
                const gchar *name = nullptr;
                while (!offered && g_variant_iter_next(names, "&s", &name)) {
                    offered = g_strcmp0(name, serviceName) == 0;
                }
                g_variant_iter_free(names);
                g_variant_unref(activatable);
            }
        }
        g_object_unref(bus);
        return offered;
    }

    class SecretServiceKeyring final : public PaymentCardKeyring {
    public:
        bool available() override { return busOffersSecretService(); }

        // Every item of the schema, unlocking the keyring if it is locked:
        // the desktop asks the reader through its own prompt.
        std::expected<QList<KeyringItem>, KeyringFailure> items() override
        {
            if (!available()) {
                return std::unexpected(KeyringFailure::Unreachable);
            }
            GError *error = nullptr;
            auto *attributes = g_hash_table_new(g_str_hash, g_str_equal);
            auto *found = secret_password_searchv_sync(&paymentCardSchema, attributes,
                static_cast<SecretSearchFlags>(
                    SECRET_SEARCH_ALL | SECRET_SEARCH_UNLOCK | SECRET_SEARCH_LOAD_SECRETS),
                nullptr, &error);
            g_hash_table_unref(attributes);
            if (error) {
                const auto failure
                    = unanswered(error) ? KeyringFailure::Unreachable : KeyringFailure::Failed;
                report(error);
                return std::unexpected(failure);
            }
            // An item the reader left locked, by dismissing the desktop's
            // prompt, comes back without its secret: the keyring stayed locked,
            // and is not an empty one.
            bool locked = false;
            for (auto *entry = found; entry && !locked; entry = entry->next) {
                locked = SECRET_IS_ITEM(entry->data)
                    && secret_item_get_locked(SECRET_ITEM(entry->data));
            }
            if (locked) {
                g_list_free_full(found, g_object_unref);
                return std::unexpected(KeyringFailure::Locked);
            }
            QList<KeyringItem> items;
            for (auto *entry = found; entry; entry = entry->next) {
                auto *retrievable = static_cast<SecretRetrievable *>(entry->data);
                auto *held = secret_retrievable_get_attributes(retrievable);
                const auto *id = static_cast<const char *>(g_hash_table_lookup(held, "id"));
                GError *failure = nullptr;
                auto *value
                    = secret_retrievable_retrieve_secret_sync(retrievable, nullptr, &failure);
                if (succeeded(failure) && value && id) {
                    gsize length = 0;
                    const auto *bytes = secret_value_get(value, &length);
                    items.append({.id = QString::fromUtf8(id),
                        .secret = QByteArray(bytes, static_cast<qsizetype>(length))});
                }
                if (value) {
                    secret_value_unref(value);
                }
                g_hash_table_unref(held);
            }
            g_list_free_full(found, g_object_unref);
            return items;
        }

        bool store(const QString &id, const QByteArray &secret) override
        {
            if (!available()) {
                return false;
            }
            GError *error = nullptr;
            const auto stored
                = secret_password_store_sync(&paymentCardSchema, SECRET_COLLECTION_DEFAULT, label,
                    secret.constData(), nullptr, &error, "id", id.toUtf8().constData(), nullptr);
            return succeeded(error) && stored;
        }

        bool remove(const QString &id) override
        {
            if (!available()) {
                return false;
            }
            GError *error = nullptr;
            const auto removed = secret_password_clear_sync(
                &paymentCardSchema, nullptr, &error, "id", id.toUtf8().constData(), nullptr);
            return succeeded(error) && removed;
        }
    };

} // namespace

std::unique_ptr<PaymentCardKeyring> makeDesktopKeyring()
{
    return std::make_unique<SecretServiceKeyring>();
}

} // namespace omaweb
