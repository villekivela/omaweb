// A Secret Service of the tests' own, which keeps its items in memory: the part
// of the freedesktop Secret Service API libsecret uses to store, search, read
// and delete passwords, in the "plain" session algorithm only, with one
// collection. `dev.omaweb.FakeSecretService.Dump` answers every item's label,
// attributes and secret as JSON, which is what the tests read the stored form
// from.
//
// `dev.omaweb.FakeSecretService.Behave` sets how it answers, as a desktop's
// keyring can: "refusing" answers a search with an error, "locked" keeps the
// collection locked and dismisses every prompt to unlock it, "unlocking" is
// locked until a prompt the reader answers, and "no-default" has no
// collection until one is created. An empty behaviour is the default.
//
// It runs as its own process because libsecret's calls wait for the answer:
// served from the caller's own thread, they would wait forever.

#include <QCoreApplication>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusVariant>
#include <QDBusVirtualObject>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QMap>

#include <cstdio>

namespace {

using Attributes = QMap<QString, QString>;

struct Secret {
    QDBusObjectPath session;
    QByteArray parameters;
    QByteArray value;
    QString contentType;
};

QDBusArgument &operator<<(QDBusArgument &argument, const Secret &secret)
{
    argument.beginStructure();
    argument << secret.session << secret.parameters << secret.value << secret.contentType;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, Secret &secret)
{
    argument.beginStructure();
    argument >> secret.session >> secret.parameters >> secret.value >> secret.contentType;
    argument.endStructure();
    return argument;
}

using Secrets = QMap<QDBusObjectPath, Secret>;

} // namespace

Q_DECLARE_METATYPE(Secret)
Q_DECLARE_METATYPE(Secrets)

namespace {

const QString root = QStringLiteral("/org/freedesktop/secrets");
const QString collection = root + QStringLiteral("/collection/login");
const QString defaultAlias = root + QStringLiteral("/aliases/default");
const QString session = root + QStringLiteral("/session/1");
const QString prompt = root + QStringLiteral("/prompt/1");
const QString promptInterface = QStringLiteral("org.freedesktop.Secret.Prompt");
const QString serviceInterface = QStringLiteral("org.freedesktop.Secret.Service");
const QString collectionInterface = QStringLiteral("org.freedesktop.Secret.Collection");
const QString itemInterface = QStringLiteral("org.freedesktop.Secret.Item");
const QString propertiesInterface = QStringLiteral("org.freedesktop.DBus.Properties");

struct Item {
    QString path;
    QString label;
    Attributes attributes;
    QByteArray secret;
    QString contentType;
    quint64 created = 0;
};

class FakeSecretService final : public QDBusVirtualObject {
public:
    QString introspect(const QString &) const override { return {}; }

    bool handleMessage(const QDBusMessage &message, const QDBusConnection &connection) override
    {
        const auto reply = answer(message);
        connection.send(reply);
        if (message.path() == prompt && message.member() == QLatin1String("Prompt")) {
            if (m_behaviour == QLatin1String("locked")) {
                complete(connection, true, QVariant::fromValue(QList<QDBusObjectPath> {}));
            } else if (m_prompted == QLatin1String("unlock")) {
                m_locked = false;
                complete(connection, false,
                    QVariant::fromValue(QList<QDBusObjectPath> {QDBusObjectPath(collection)}));
            } else if (m_prompted == QLatin1String("create")) {
                m_collectionExists = true;
                complete(connection, false, QVariant::fromValue(QDBusObjectPath(collection)));
            }
            m_prompted.clear();
        }
        return true;
    }

private:
    QList<Item> m_items;
    int m_next = 0;
    QString m_behaviour;
    bool m_locked = false;
    bool m_collectionExists = true;
    // What the open prompt unlocks or creates when the reader answers it.
    QString m_prompted;

    bool hasCollection(const QString &path) const
    {
        return m_collectionExists && collectionPath(path) == collection;
    }

    QList<QDBusObjectPath> itemPaths() const
    {
        QList<QDBusObjectPath> paths;
        for (const auto &item : m_items) {
            paths.append(QDBusObjectPath(item.path));
        }
        return paths;
    }

    // Answers the prompt as the reader would, after the call that showed it.
    void complete(const QDBusConnection &connection, bool dismissed, const QVariant &result)
    {
        auto completed
            = QDBusMessage::createSignal(prompt, promptInterface, QStringLiteral("Completed"));
        completed << dismissed << QVariant::fromValue(QDBusVariant(result));
        connection.send(completed);
    }

    static QDBusMessage fail(const QDBusMessage &message, const QString &name)
    {
        return message.createErrorReply(name, name);
    }

    QString collectionPath(const QString &path) const
    {
        return path == defaultAlias ? collection : path;
    }

    Item *item(const QString &path)
    {
        for (auto &item : m_items) {
            if (item.path == path) {
                return &item;
            }
        }
        return nullptr;
    }

    QList<QDBusObjectPath> search(const Attributes &wanted) const
    {
        QList<QDBusObjectPath> found;
        for (const auto &item : m_items) {
            bool matches = true;
            for (auto it = wanted.cbegin(); it != wanted.cend(); ++it) {
                matches = matches && item.attributes.value(it.key()) == it.value();
            }
            if (matches) {
                found.append(QDBusObjectPath(item.path));
            }
        }
        return found;
    }

    QVariantMap properties(const QString &path, const QString &interface)
    {
        if (path == root && interface == serviceInterface) {
            return {{QStringLiteral("Collections"),
                QVariant::fromValue(m_collectionExists
                        ? QList<QDBusObjectPath> {QDBusObjectPath(collection)}
                        : QList<QDBusObjectPath> {})}};
        }
        if (hasCollection(path) && interface == collectionInterface) {
            return {{QStringLiteral("Items"), QVariant::fromValue(itemPaths())},
                {QStringLiteral("Label"), QStringLiteral("Login")},
                {QStringLiteral("Locked"), m_locked},
                {QStringLiteral("Created"), QVariant::fromValue(quint64 {0})},
                {QStringLiteral("Modified"), QVariant::fromValue(quint64 {0})}};
        }
        if (const auto *found = item(path); found && interface == itemInterface) {
            return {{QStringLiteral("Attributes"), QVariant::fromValue(found->attributes)},
                {QStringLiteral("Label"), found->label}, {QStringLiteral("Locked"), m_locked},
                {QStringLiteral("Created"), QVariant::fromValue(found->created)},
                {QStringLiteral("Modified"), QVariant::fromValue(found->created)}};
        }
        return {};
    }

    QDBusMessage answer(const QDBusMessage &message)
    {
        const auto path = message.path();
        const auto interface = message.interface();
        const auto member = message.member();
        const auto arguments = message.arguments();

        if (interface == propertiesInterface && member == QLatin1String("GetAll")) {
            return message.createReply(QVariant::fromValue(
                properties(collectionPath(path), arguments.value(0).toString())));
        }
        if (interface == propertiesInterface && member == QLatin1String("Get")) {
            const auto all = properties(collectionPath(path), arguments.value(0).toString());
            const auto name = arguments.value(1).toString();
            if (!all.contains(name)) {
                return fail(message, QStringLiteral("org.freedesktop.DBus.Error.InvalidArgs"));
            }
            return message.createReply(QVariant::fromValue(QDBusVariant(all.value(name))));
        }
        if (interface == QLatin1String("dev.omaweb.FakeSecretService")
            && member == QLatin1String("Behave")) {
            m_behaviour = arguments.value(0).toString();
            m_locked = m_behaviour == QLatin1String("locked")
                || m_behaviour == QLatin1String("unlocking");
            m_collectionExists = m_behaviour != QLatin1String("no-default");
            return message.createReply();
        }
        if (interface == QLatin1String("dev.omaweb.FakeSecretService")
            && member == QLatin1String("Dump")) {
            QJsonArray items;
            for (const auto &item : m_items) {
                QJsonObject attributes;
                for (auto it = item.attributes.cbegin(); it != item.attributes.cend(); ++it) {
                    attributes.insert(it.key(), it.value());
                }
                items.append(QJsonObject {{QStringLiteral("label"), item.label},
                    {QStringLiteral("attributes"), attributes},
                    {QStringLiteral("secret"), QString::fromUtf8(item.secret)}});
            }
            return message.createReply(
                QString::fromUtf8(QJsonDocument(items).toJson(QJsonDocument::Compact)));
        }

        if (path == root && interface == serviceInterface) {
            if (member == QLatin1String("OpenSession")) {
                if (arguments.value(0).toString() != QLatin1String("plain")) {
                    return fail(message, QStringLiteral("org.freedesktop.DBus.Error.NotSupported"));
                }
                return message.createReply({QVariant::fromValue(QDBusVariant(QString())),
                    QVariant::fromValue(QDBusObjectPath(session))});
            }
            if (member == QLatin1String("SearchItems")) {
                if (m_behaviour == QLatin1String("refusing")) {
                    return fail(message, QStringLiteral("org.freedesktop.DBus.Error.AccessDenied"));
                }
                const auto found = search(qdbus_cast<Attributes>(arguments.value(0)));
                const auto none = QList<QDBusObjectPath> {};
                return message.createReply({QVariant::fromValue(m_locked ? none : found),
                    QVariant::fromValue(m_locked ? found : none)});
            }
            if (member == QLatin1String("Unlock")) {
                if (m_locked) {
                    m_prompted = QStringLiteral("unlock");
                    return message.createReply({QVariant::fromValue(QList<QDBusObjectPath> {}),
                        QVariant::fromValue(QDBusObjectPath(prompt))});
                }
                return message.createReply({arguments.value(0),
                    QVariant::fromValue(QDBusObjectPath(QStringLiteral("/")))});
            }
            if (member == QLatin1String("ReadAlias")) {
                return message.createReply(QVariant::fromValue(
                    QDBusObjectPath(m_collectionExists ? collection : QStringLiteral("/"))));
            }
            if (member == QLatin1String("CreateCollection")) {
                if (m_collectionExists) {
                    return message.createReply({QVariant::fromValue(QDBusObjectPath(collection)),
                        QVariant::fromValue(QDBusObjectPath(QStringLiteral("/")))});
                }
                m_prompted = QStringLiteral("create");
                return message.createReply(
                    {QVariant::fromValue(QDBusObjectPath(QStringLiteral("/"))),
                        QVariant::fromValue(QDBusObjectPath(prompt))});
            }
            if (member == QLatin1String("GetSecrets") && m_locked) {
                return fail(message, QStringLiteral("org.freedesktop.Secret.Error.IsLocked"));
            }
            if (member == QLatin1String("GetSecrets")) {
                Secrets secrets;
                for (const auto &wanted : qdbus_cast<QList<QDBusObjectPath>>(arguments.value(0))) {
                    if (const auto *found = item(wanted.path())) {
                        secrets.insert(wanted,
                            Secret {
                                QDBusObjectPath(session), {}, found->secret, found->contentType});
                    }
                }
                return message.createReply(QVariant::fromValue(secrets));
            }
        }

        if (path == prompt && interface == promptInterface) {
            if (member == QLatin1String("Prompt")) {
                return message.createReply();
            }
            if (member == QLatin1String("Dismiss")) {
                m_prompted.clear();
                return message.createReply();
            }
        }

        if (hasCollection(path) && interface == collectionInterface) {
            if (member == QLatin1String("CreateItem")) {
                if (m_locked) {
                    return fail(message, QStringLiteral("org.freedesktop.Secret.Error.IsLocked"));
                }
                const auto given = qdbus_cast<QVariantMap>(arguments.value(0));
                const auto secret = qdbus_cast<Secret>(arguments.value(1));
                const bool replace = arguments.value(2).toBool();
                const auto attributes = qdbus_cast<Attributes>(
                    given.value(QStringLiteral("org.freedesktop.Secret.Item.Attributes")));
                Item *kept = nullptr;
                if (replace) {
                    for (auto &existing : m_items) {
                        if (existing.attributes == attributes) {
                            kept = &existing;
                        }
                    }
                }
                if (!kept) {
                    Item added;
                    added.path = collection + QStringLiteral("/%1").arg(++m_next);
                    added.created = quint64(QDateTime::currentSecsSinceEpoch());
                    m_items.append(added);
                    kept = &m_items.last();
                }
                kept->label
                    = given.value(QStringLiteral("org.freedesktop.Secret.Item.Label")).toString();
                kept->attributes = attributes;
                kept->secret = secret.value;
                kept->contentType = secret.contentType;
                return message.createReply({QVariant::fromValue(QDBusObjectPath(kept->path)),
                    QVariant::fromValue(QDBusObjectPath(QStringLiteral("/")))});
            }
            if (member == QLatin1String("SearchItems")) {
                return message.createReply(
                    QVariant::fromValue(search(qdbus_cast<Attributes>(arguments.value(0)))));
            }
        }

        if (auto *found = item(path); found && interface == itemInterface) {
            if (m_locked) {
                return fail(message, QStringLiteral("org.freedesktop.Secret.Error.IsLocked"));
            }
            if (member == QLatin1String("GetSecret")) {
                return message.createReply(QVariant::fromValue(
                    Secret {QDBusObjectPath(session), {}, found->secret, found->contentType}));
            }
            if (member == QLatin1String("Delete")) {
                m_items.removeIf([&path](const Item &item) { return item.path == path; });
                return message.createReply(
                    QVariant::fromValue(QDBusObjectPath(QStringLiteral("/"))));
            }
        }
        std::fprintf(stderr, "fake secret service: unanswered %s %s.%s\n", qPrintable(path),
            qPrintable(interface), qPrintable(member));
        return fail(message, QStringLiteral("org.freedesktop.DBus.Error.UnknownMethod"));
    }
};

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    qDBusRegisterMetaType<Attributes>();
    qDBusRegisterMetaType<Secret>();
    qDBusRegisterMetaType<Secrets>();
    qDBusRegisterMetaType<QList<QDBusObjectPath>>();

    auto bus = QDBusConnection::sessionBus();
    FakeSecretService service;
    if (!bus.registerVirtualObject(root, &service, QDBusConnection::SubPath)
        || !bus.registerService(QStringLiteral("org.freedesktop.secrets"))) {
        std::fprintf(stderr, "fake secret service: %s\n", qPrintable(bus.lastError().message()));
        return 1;
    }
    return QCoreApplication::exec();
}
