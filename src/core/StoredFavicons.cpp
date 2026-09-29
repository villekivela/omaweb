#include "StoredFavicons.h"

#include "HistoryQuery.h"

#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QStringList>
#include <QUuid>

#include <memory>
#include <utility>

namespace omaweb::storedfavicons {
namespace {

    // Every window that can answer, by source. Image providers ask from the
    // interface's image-loading thread while windows come and go on the
    // interface thread, so the table is locked. A lookup only posts its
    // question, so holding the lock through it costs nothing, and a window
    // removing itself waits until no lookup is still reaching for it.
    struct Registry {
        QMutex mutex;
        QHash<QString, Lookup> sources;
        Reader reader;
    };

    Registry &registry()
    {
        static Registry instance;
        return instance;
    }

    // The page and Space travel as base64: an address is full of the slashes
    // and percent signs that an image URL's path would otherwise rewrite.
    constexpr auto encoding = QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals;

    // Answers once, whatever happens to the question. A window that closes
    // with a lookup still queued drops the call that carried it, and the image
    // waiting on it would never finish; letting go of the last copy of an
    // unanswered question answers it with nothing instead.
    class AnswerOnce {
    public:
        explicit AnswerOnce(Answer answer)
            : m_answer(std::move(answer))
        {
        }

        ~AnswerOnce()
        {
            if (m_answer) {
                m_answer({});
            }
        }

        AnswerOnce(const AnswerOnce &) = delete;
        AnswerOnce &operator=(const AnswerOnce &) = delete;

        void operator()(const QByteArray &image)
        {
            if (auto answer = std::exchange(m_answer, {})) {
                answer(image);
            }
        }

    private:
        Answer m_answer;
    };

} // namespace

QString addSource(Lookup lookup)
{
    const auto source = QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto &table = registry();
    const QMutexLocker locker(&table.mutex);
    table.sources.insert(source, std::move(lookup));
    return source;
}

void removeSource(const QString &source)
{
    auto &table = registry();
    const QMutexLocker locker(&table.mutex);
    table.sources.remove(source);
}

QUrl address(const QString &source, const QString &spaceId, const QUrl &pageUrl)
{
    if (source.isEmpty() || history::origin(pageUrl).isEmpty()) {
        return {};
    }
    return QUrl(QStringLiteral("image://%1/%2/%3/%4")
            .arg(QLatin1String(providerId), source,
                QString::fromLatin1(spaceId.toUtf8().toBase64(encoding)),
                QString::fromLatin1(pageUrl.toString().toUtf8().toBase64(encoding))));
}

void find(const QString &identifier, Answer answer)
{
    const auto parts = identifier.split(QLatin1Char('/'));
    if (parts.size() != 3) {
        answer({});
        return;
    }
    const auto spaceId
        = QString::fromUtf8(QByteArray::fromBase64(parts.at(1).toLatin1(), encoding));
    const QUrl pageUrl(QString::fromUtf8(QByteArray::fromBase64(parts.at(2).toLatin1(), encoding)));
    auto &table = registry();
    QMutexLocker locker(&table.mutex);
    const auto lookup = table.sources.constFind(parts.at(0));
    if (lookup == table.sources.cend()) {
        locker.unlock();
        answer({});
        return;
    }
    const auto once = std::make_shared<AnswerOnce>(std::move(answer));
    (*lookup)(spaceId, pageUrl, [once](const QByteArray &image) { (*once)(image); });
}

void setReader(Reader reader)
{
    auto &table = registry();
    const QMutexLocker locker(&table.mutex);
    table.reader = std::move(reader);
}

void read(const QUrl &iconUrl, QObject *context, Answer answer)
{
    Reader reader;
    {
        auto &table = registry();
        const QMutexLocker locker(&table.mutex);
        reader = table.reader;
    }
    if (reader) {
        reader(iconUrl, context, std::move(answer));
    }
}

} // namespace omaweb::storedfavicons
