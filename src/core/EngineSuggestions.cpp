#include "EngineSuggestions.h"

#include "PrivacyFile.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

#include <utility>

namespace omaweb {
namespace {

    constexpr QLatin1StringView enabledKey("engine-suggestions");
    // A proposal that arrives after this is one the reader has typed past.
    constexpr int answerTimeoutMilliseconds = 1000;

} // namespace

EngineSuggestions::EngineSuggestions(QString configRoot, QObject *parent)
    : QObject(parent)
    , m_configRoot(std::move(configRoot))
{
    // Only an explicit `true` turns it on: a file that cannot be read the way
    // it is written sends nothing anywhere.
    const auto value = PrivacyFile::read(m_configRoot, enabledKey);
    m_enabled = value.isBool() && value.toBool();
}

EngineSuggestions::~EngineSuggestions() = default;

bool EngineSuggestions::enabled() const { return m_enabled; }

void EngineSuggestions::setEnabled(bool enabled)
{
    if (enabled == m_enabled) {
        return;
    }
    m_enabled = enabled;
    PrivacyFile::write(m_configRoot, enabledKey, m_enabled);
    emit enabledChanged();
}

QNetworkReply *EngineSuggestions::ask(const QUrl &address)
{
    if (!m_network) {
        m_network = std::make_unique<QNetworkAccessManager>();
    }
    QNetworkRequest request(address);
    // The engine learns the terms and nothing about who typed them: no
    // cookie goes out, none that comes back is kept, and the user agent
    // names the browser and not its version or platform.
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Omaweb"));
    request.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
    auto *reply = m_network->get(request);
    // The whole answer, not the silence between its bytes: an engine that
    // trickles is as late as one that says nothing.
    QTimer::singleShot(answerTimeoutMilliseconds, reply, &QNetworkReply::abort);
    return reply;
}

QStringList EngineSuggestions::parse(const QByteArray &body)
{
    const auto document = QJsonDocument::fromJson(body);
    const auto answer = document.array();
    if (!document.isArray() || answer.size() < 2 || !answer.at(0).isString()
        || !answer.at(1).isArray()) {
        return {};
    }
    QStringList proposals;
    for (const auto value : answer.at(1).toArray()) {
        if (!value.isString()) {
            return {};
        }
        const auto proposal = value.toString().trimmed();
        if (!proposal.isEmpty()) {
            proposals.append(proposal);
        }
    }
    return proposals;
}

} // namespace omaweb
