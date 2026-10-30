#include "BarFeed.h"

#include "BarSocket.h"
#include "StoredFavicons.h"

#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QSaveFile>
#include <QUrl>

#include <sys/stat.h>

namespace omaweb {
namespace {

    const QStringList tabGroups {QStringLiteral("pins"), QStringLiteral("tabs")};

    // Long enough to tell two pictures apart for as long as a run lasts.
    constexpr int nameLength = 32;

    // A Space that stored no icon for a page answers a PNG of one pixel, which
    // the sidebar reads as no artwork. Read from the header, which holds the
    // width and the height after the signature and the chunk's length and type.
    bool isNoArtwork(const QByteArray &image)
    {
        constexpr qsizetype sizeAt = 16;
        if (image.size() < sizeAt + 8 || !image.startsWith("\x89PNG\r\n\x1a\n")) {
            return false;
        }
        const auto dimension = [&image](qsizetype at) {
            return (quint32(quint8(image.at(at))) << 24) | (quint32(quint8(image.at(at + 1))) << 16)
                | (quint32(quint8(image.at(at + 2))) << 8) | quint32(quint8(image.at(at + 3)));
        };
        return dimension(sizeAt) <= 1 && dimension(sizeAt + 4) <= 1;
    }

} // namespace

BarFeed::BarFeed(BarSocket *socket, QString directory, QObject *parent)
    : QObject(parent)
    , m_socket(socket)
    , m_directory(std::move(directory))
{
    m_pause.setSingleShot(true);
    connect(&m_pause, &QTimer::timeout, this, &BarFeed::send);
}

BarFeed::~BarFeed() = default;

void BarFeed::publish(const QJsonObject &state)
{
    m_state = state;
    if (m_pause.isActive()) {
        return;
    }
    const auto waited = m_sinceSent.isValid() ? m_sinceSent.elapsed() : m_interval;
    if (waited >= m_interval) {
        send();
    } else {
        m_pause.start(static_cast<int>(m_interval - waited));
    }
}

void BarFeed::setInterval(int milliseconds) { m_interval = milliseconds; }

void BarFeed::clear()
{
    m_pause.stop();
    m_state = {};
    forgetAllBut({});
}

void BarFeed::send()
{
    QSet<QString> addresses;
    const auto state = withFiles(m_state, addresses);
    m_socket->publish(state);
    m_sinceSent.start();
    forgetAllBut(addresses);
    for (const auto &address : std::as_const(addresses)) {
        if (!m_files.contains(address) && !m_reading.contains(address)) {
            read(address);
        }
    }
}

// The icon is read once per address. A page that draws a new picture reports
// a new address, which is read in its turn.
void BarFeed::read(const QString &address)
{
    m_reading.insert(address);
    storedfavicons::read(QUrl(address), this, [this, address](const QByteArray &image) {
        if (!m_reading.remove(address)) {
            return;
        }
        if (image.isEmpty() || isNoArtwork(image)) {
            m_files.insert(address, QString());
            return;
        }
        const auto name = QString::fromLatin1(
            QCryptographicHash::hash(image, QCryptographicHash::Sha256).toHex().left(nameLength));
        const auto file = m_directory.filePath(name + QStringLiteral(".png"));
        if (!QFile::exists(file)) {
            if (!m_directory.mkpath(QStringLiteral("."))) {
                qWarning("The bar's icon directory %s could not be made",
                    qPrintable(m_directory.path()));
                return;
            }
            ::chmod(QFile::encodeName(m_directory.path()).constData(), S_IRWXU);
            QSaveFile save(file);
            if (!save.open(QIODevice::WriteOnly) || save.write(image) != image.size()
                || !save.commit()) {
                qWarning("The bar's icon %s could not be written", qPrintable(file));
                return;
            }
        }
        m_files.insert(address, file);
        publish(m_state);
    });
}

QJsonObject BarFeed::withFiles(const QJsonObject &state, QSet<QString> &addresses) const
{
    auto spaces = state.value(QStringLiteral("spaces")).toArray();
    for (auto space = spaces.begin(); space != spaces.end(); ++space) {
        auto object = space->toObject();
        for (const auto &group : tabGroups) {
            auto tabs = object.value(group).toArray();
            for (auto tab = tabs.begin(); tab != tabs.end(); ++tab) {
                auto fields = tab->toObject();
                const auto address = fields.value(QStringLiteral("icon")).toString();
                if (!address.isEmpty()) {
                    addresses.insert(address);
                }
                fields.insert(QStringLiteral("icon"), m_files.value(address));
                *tab = fields;
            }
            object.insert(group, tabs);
        }
        *space = object;
    }
    auto withFiles = state;
    withFiles.insert(QStringLiteral("spaces"), spaces);
    return withFiles;
}

// A file is deleted once no address the bar shows names it. Two addresses can
// name one picture, and so one file.
void BarFeed::forgetAllBut(const QSet<QString> &addresses)
{
    QSet<QString> kept;
    for (auto it = m_files.begin(); it != m_files.end();) {
        if (addresses.contains(it.key())) {
            kept.insert(it.value());
            ++it;
        } else {
            it = m_files.erase(it);
        }
    }
    for (auto it = m_reading.begin(); it != m_reading.end();) {
        it = addresses.contains(*it) ? std::next(it) : m_reading.erase(it);
    }
    const auto files = m_directory.entryList({QStringLiteral("*.png")}, QDir::Files);
    for (const auto &name : files) {
        const auto file = m_directory.filePath(name);
        if (!kept.contains(file)) {
            QFile::remove(file);
        }
    }
}

} // namespace omaweb
