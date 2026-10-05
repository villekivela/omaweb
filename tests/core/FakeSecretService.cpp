// A Secret Service of the tests' own, which keeps its items in memory: the part
// of the freedesktop Secret Service API libsecret uses to store, search, read
// and delete passwords, in the "plain" session algorithm only, with one
// collection that is always unlocked. `dev.omaweb.FakeSecretService.Dump`
// answers every item's label, attributes and secret as JSON, which is what the
// tests read the stored form from.
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
        return true;
    }

private:
    QList<Item> m_items;
    int m_next = 0;

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
                QVariant::fromValue(QList<QDBusObjectPath> {QDBusObjectPath(collection)})}};
        }
        if (collectionPath(path) == collection && interface == collectionInterface) {
            QList<QDBusObjectPath> items;
            for (const auto &item : m_items) {
                items.append(QDBusObjectPath(item.path));
            }
            return {{QStringLiteral("Items"), QVariant::fromValue(items)},
                {QStringLiteral("Label"), QStringLiteral("Login")},
                {QStringLiteral("Locked"), false},
                {QStringLiteral("Created"), QVariant::fromValue(quint64 {0})},
                {QStringLiteral("Modified"), QVariant::fromValue(quint64 {0})}};
        }
        if (const auto *found = item(path); found && interface == itemInterface) {
            return {{QStringLiteral("Attributes"), QVariant::fromValue(found->attributes)},
                {QStringLiteral("Label"), found->label}, {QStringLiteral("Locked"), false},
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
                const auto wanted = qdbus_cast<Attributes>(arguments.value(0));
                return message.createReply({QVariant::fromValue(search(wanted)),
                    QVariant::fromValue(QList<QDBusObjectPath> {})});
            }
            if (member == QLatin1String("Unlock")) {
                return message.createReply({arguments.value(0),
                    QVariant::fromValue(QDBusObjectPath(QStringLiteral("/")))});
            }
            if (member == QLatin1String("ReadAlias")) {
                return message.createReply(QVariant::fromValue(QDBusObjectPath(collection)));
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

        if (collectionPath(path) == collection && interface == collectionInterface) {
            if (member == QLatin1String("CreateItem")) {
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
