#include "GitHubForge.h"

#include <QEventLoop>
#include <QHttpHeaders>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

#include <utility>

namespace omaweb {

namespace {

    void setError(QString *destination, const QString &message)
    {
        if (destination) {
            *destination = message;
        }
    }

    QJsonObject objectFrom(const QByteArray &body)
    {
        return QJsonDocument::fromJson(body).object();
    }

    QString responseFailure(
        const QByteArray &body, const QString &networkError, const QString &fallback)
    {
        const auto apiMessage = objectFrom(body).value(QStringLiteral("message")).toString();
        const auto detail = apiMessage.isEmpty() ? networkError : apiMessage;
        return detail.isEmpty() ? fallback : QStringLiteral("%1: %2").arg(fallback, detail);
    }

} // namespace

GitHubForge::GitHubForge(QString clientId, QString appSlug, QUrl webRoot, QUrl apiRoot)
    : m_clientId(std::move(clientId))
    , m_appSlug(std::move(appSlug))
    , m_webRoot(std::move(webRoot))
    , m_apiRoot(std::move(apiRoot))
{
}

QUrl GitHubForge::webUrl(const QString &path) const { return m_webRoot.resolved(QUrl(path)); }

QUrl GitHubForge::apiUrl(const QString &path) const { return m_apiRoot.resolved(QUrl(path)); }

GitHubForge::Response GitHubForge::request(const QByteArray &method, const QUrl &url,
    const QByteArray &body, const QByteArray &accessToken, const QByteArray &contentType,
    qsizetype maximumResponseBytes)
{
    QNetworkRequest request(url);
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    request.setMaximumRedirectsAllowed(0);
    if (!accessToken.isEmpty()) {
        request.setRawHeader("Authorization", "Bearer " + accessToken);
    }
    if (!contentType.isEmpty()) {
        request.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
    }
    auto *reply = m_network.sendCustomRequest(request, method, body);
    QEventLoop wait;
    QByteArray responseBody;
    bool responseTooLarge = false;
    const auto consume = [&] {
        responseBody.append(reply->readAll());
        if (maximumResponseBytes >= 0 && responseBody.size() > maximumResponseBytes) {
            responseTooLarge = true;
            reply->abort();
        }
    };
    QObject::connect(reply, &QNetworkReply::readyRead, &wait, consume);
    QObject::connect(reply, &QNetworkReply::finished, &wait, &QEventLoop::quit);
    wait.exec();
    consume();
    Response response {.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(),
        .body = responseTooLarge ? QByteArray {} : std::move(responseBody),
        .error = responseTooLarge ? QStringLiteral("The response is larger than the safe limit")
            : reply->error() == QNetworkReply::NoError ? QString {}
                                                       : reply->errorString()};
    reply->deleteLater();
    return response;
}

const ForgeVocabulary &GitHubForge::vocabulary() const
{
    static const ForgeVocabulary words {
        .name = QStringLiteral("GitHub"),
        .identifier = QStringLiteral("github"),
        .repositoryName = QStringLiteral("omaweb-sync"),
        .commitEmailDomain = QStringLiteral("users.noreply.github.com"),
        .contactingStatus = QStringLiteral("Contacting GitHub"),
        .authorizationStatus = QStringLiteral("Waiting for GitHub authorization"),
        .repositoryStatus = QStringLiteral("Create the private Sync repository on GitHub"),
        .installationStatus = QStringLiteral("Waiting for GitHub App installation"),
        .failureStatus = QStringLiteral("GitHub connection failed"),
        .connectionNotStarted = QStringLiteral("GitHub connection has not been started"),
        .authorizationRefused = QStringLiteral("GitHub did not authorize Sync"),
        .authorizationExpired = QStringLiteral("GitHub authorization expired; connect again"),
        .connectAction = QStringLiteral("Connect GitHub"),
        .authorizationAction = QStringLiteral("Open GitHub authorization"),
        .failureTitle = QStringLiteral("GitHub Sync failed"),
        .codeCopiedNotice = QStringLiteral("GitHub code copied"),
        .codePrompt = QStringLiteral("Enter %1 on GitHub"),
        .authorizationNote = QStringLiteral(
            "The GitHub login becomes your Sync identity; Omaweb does not create an account."),
        .repositoryTitle = QStringLiteral("Create the private omaweb-sync repository on GitHub"),
        .repositoryNote = QStringLiteral("Keep the prefilled name and Private visibility. After "
                                         "GitHub creates it, return here to continue."),
        .installationTitle = QStringLiteral("Install Omaweb Sync for your personal GitHub account"),
        .installationNote
        = QStringLiteral("Choose Only select repositories, select omaweb-sync, and install. Omaweb "
                         "detects approval automatically."),
        .observationNote
        = QStringLiteral("GitHub can still observe repository size and update timing."),
    };
    return words;
}

DeviceAuthorization GitHubForge::beginAuthorization(QString *errorMessage)
{
    if (m_clientId.isEmpty()) {
        setError(errorMessage, QStringLiteral("This build has no GitHub App client ID"));
        return {};
    }
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("client_id"), m_clientId);
    const auto response = request("POST", webUrl(QStringLiteral("/login/device/code")),
        form.toString(QUrl::FullyEncoded).toUtf8(), {}, "application/x-www-form-urlencoded");
    const auto object = objectFrom(response.body);
    if (response.status != 200) {
        setError(errorMessage,
            response.error.isEmpty() ? object.value(QStringLiteral("error_description")).toString()
                                     : response.error);
        return {};
    }
    return {.deviceCode = object.value(QStringLiteral("device_code")).toString(),
        .userCode = object.value(QStringLiteral("user_code")).toString(),
        .verificationUrl = QUrl(object.value(QStringLiteral("verification_uri")).toString()),
        .expiresInSeconds = object.value(QStringLiteral("expires_in")).toInt(),
        .pollIntervalSeconds = object.value(QStringLiteral("interval")).toInt()};
}

ForgeAuthorization GitHubForge::pollAuthorization(const QString &deviceCode, QString *errorMessage)
{
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("client_id"), m_clientId);
    form.addQueryItem(QStringLiteral("device_code"), deviceCode);
    form.addQueryItem(QStringLiteral("grant_type"),
        QStringLiteral("urn:ietf:params:oauth:grant-type:device_code"));
    const auto response = request("POST", webUrl(QStringLiteral("/login/oauth/access_token")),
        form.toString(QUrl::FullyEncoded).toUtf8(), {}, "application/x-www-form-urlencoded");
    const auto token = objectFrom(response.body);
    const auto code = token.value(QStringLiteral("error")).toString();
    if (code == QLatin1String("authorization_pending") || code == QLatin1String("slow_down")) {
        return {.state = AuthorizationState::Pending,
            .pollIntervalAdjustmentSeconds = code == QLatin1String("slow_down") ? 5 : 0};
    }
    if (response.status != 200 || !code.isEmpty()) {
        setError(errorMessage, token.value(QStringLiteral("error_description")).toString());
        return {.state = code == QLatin1String("expired_token") ? AuthorizationState::Expired
                                                                : AuthorizationState::Failed};
    }
    auto accessToken = token.value(QStringLiteral("access_token")).toString().toUtf8();
    const auto profile = request("GET", apiUrl(QStringLiteral("/user")), {}, accessToken);
    const auto user = objectFrom(profile.body);
    if (profile.status != 200 || user.value(QStringLiteral("login")).toString().isEmpty()) {
        setError(errorMessage,
            profile.error.isEmpty() ? QStringLiteral("GitHub did not return an identity")
                                    : profile.error);
        return {.state = AuthorizationState::Failed};
    }
    return {.state = AuthorizationState::Complete,
        .accessToken = std::move(accessToken),
        .refreshToken = token.value(QStringLiteral("refresh_token")).toString().toUtf8(),
        .login = user.value(QStringLiteral("login")).toString(),
        .accountId = user.value(QStringLiteral("id")).toInteger(),
        .avatarUrl = QUrl(user.value(QStringLiteral("avatar_url")).toString()),
        .expiresInSeconds = token.value(QStringLiteral("expires_in")).toInt()};
}

ForgeAuthorization GitHubForge::refreshAuthorization(
    const QByteArray &refreshToken, QString *errorMessage)
{
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("client_id"), m_clientId);
    form.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("refresh_token"));
    form.addQueryItem(QStringLiteral("refresh_token"), QString::fromUtf8(refreshToken));
    const auto response = request("POST", webUrl(QStringLiteral("/login/oauth/access_token")),
        form.toString(QUrl::FullyEncoded).toUtf8(), {}, "application/x-www-form-urlencoded");
    const auto token = objectFrom(response.body);
    if (response.status != 200
        || token.value(QStringLiteral("access_token")).toString().isEmpty()) {
        setError(errorMessage, token.value(QStringLiteral("error_description")).toString());
        return {};
    }
    return {.state = AuthorizationState::Complete,
        .accessToken = token.value(QStringLiteral("access_token")).toString().toUtf8(),
        .refreshToken = token.value(QStringLiteral("refresh_token")).toString().toUtf8(),
        .expiresInSeconds = token.value(QStringLiteral("expires_in")).toInt()};
}

InstallationState GitHubForge::installationState(
    const QByteArray &accessToken, const QString &login, qint64 repositoryId, QString *errorMessage)
{
    if (m_appSlug.isEmpty()) {
        setError(errorMessage, QStringLiteral("This build has no GitHub App slug"));
        return InstallationState::Failed;
    }
    const auto response = request(
        "GET", apiUrl(QStringLiteral("/user/installations?per_page=100")), {}, accessToken);
    if (response.status != 200) {
        setError(errorMessage,
            response.error.isEmpty() ? QStringLiteral("GitHub could not check the App installation")
                                     : response.error);
        return InstallationState::Failed;
    }
    const auto installations
        = objectFrom(response.body).value(QStringLiteral("installations")).toArray();
    for (const auto &value : installations) {
        const auto installation = value.toObject();
        const auto account = installation.value(QStringLiteral("account")).toObject();
        if (installation.value(QStringLiteral("app_slug")).toString() == m_appSlug
            && account.value(QStringLiteral("login")).toString() == login) {
            if (repositoryId <= 0
                || installation.value(QStringLiteral("repository_selection")).toString()
                    == QLatin1String("all")) {
                return InstallationState::Complete;
            }
            const auto installationId = installation.value(QStringLiteral("id")).toInteger();
            const auto repositories = request("GET",
                apiUrl(QStringLiteral("/user/installations/%1/repositories?per_page=100")
                        .arg(installationId)),
                {}, accessToken);
            if (repositories.status != 200) {
                setError(errorMessage,
                    repositories.error.isEmpty()
                        ? QStringLiteral("GitHub could not check repository access")
                        : repositories.error);
                return InstallationState::Failed;
            }
            for (const auto &repository :
                objectFrom(repositories.body).value(QStringLiteral("repositories")).toArray()) {
                if (repository.toObject().value(QStringLiteral("id")).toInteger() == repositoryId) {
                    return InstallationState::Complete;
                }
            }
            return InstallationState::Pending;
        }
    }
    return InstallationState::Pending;
}

QUrl GitHubForge::installationUrl(qint64 accountId, qint64 repositoryId) const
{
    if (repositoryId <= 0) {
        return webUrl(QStringLiteral("/apps/%1/installations/new").arg(m_appSlug));
    }
    auto url = webUrl(QStringLiteral("/apps/%1/installations/new/permissions").arg(m_appSlug));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("suggested_target_id"), QString::number(accountId));
    query.addQueryItem(QStringLiteral("repository_ids[]"), QString::number(repositoryId));
    url.setQuery(query);
    return url;
}

QUrl GitHubForge::repositoryCreationUrl(const QString &owner, const QString &repositoryName) const
{
    auto url = webUrl(QStringLiteral("/new"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("owner"), owner);
    query.addQueryItem(QStringLiteral("name"), repositoryName);
    query.addQueryItem(
        QStringLiteral("description"), QStringLiteral("Encrypted Omaweb browser synchronization"));
    query.addQueryItem(QStringLiteral("visibility"), QStringLiteral("private"));
    url.setQuery(query);
    return url;
}

ForgeRepository GitHubForge::provisionPrivateRepository(const QByteArray &accessToken,
    const QString &owner, const QString &preferredName, QString *errorMessage)
{
    if (owner.isEmpty()) {
        setError(errorMessage, QStringLiteral("GitHub identity is not available"));
        return {};
    }
    for (int suffix = 1; suffix <= 100; ++suffix) {
        const auto name = suffix == 1 ? preferredName
                                      : preferredName + QLatin1Char('-') + QString::number(suffix);
        const auto existing = request(
            "GET", apiUrl(QStringLiteral("/repos/%1/%2").arg(owner, name)), {}, accessToken);
        if (existing.status == 200) {
            const auto repository = objectFrom(existing.body);
            const auto marker = request("GET",
                apiUrl(QStringLiteral("/repos/%1/%2/contents/meta.json").arg(owner, name)), {},
                accessToken);
            const auto markerObject = objectFrom(marker.body);
            const auto markerContents = QByteArray::fromBase64(
                markerObject.value(QStringLiteral("content")).toString().toLatin1());
            const auto metadata = objectFrom(markerContents);
            const auto isPrivate = repository.value(QStringLiteral("private")).toBool();
            const auto isExistingSync = marker.status == 200
                && metadata.value(QStringLiteral("format")).toString()
                    == vocabulary().repositoryName;
            const auto isNewEmptyRepository
                = marker.status == 404 && repository.value(QStringLiteral("size")).toInteger() == 0;
            if (isPrivate && (isExistingSync || isNewEmptyRepository)) {
                return {.id = repository.value(QStringLiteral("id")).toInteger(),
                    .name = name,
                    .cloneUrl = QUrl(repository.value(QStringLiteral("clone_url")).toString()),
                    .isPrivate = true,
                    .created = isNewEmptyRepository};
            }
            continue;
        }
        if (existing.status != 404) {
            setError(errorMessage,
                responseFailure(existing.body, existing.error,
                    QStringLiteral("GitHub could not check the repository")));
            return {};
        }
        const auto response = request("POST", apiUrl(QStringLiteral("/user/repos")),
            QJsonDocument(QJsonObject {{QStringLiteral("name"), name},
                              {QStringLiteral("description"),
                                  QStringLiteral("Encrypted Omaweb browser synchronization")},
                              {QStringLiteral("private"), true}})
                .toJson(QJsonDocument::Compact),
            accessToken, "application/json");
        const auto repository = objectFrom(response.body);
        if (response.status == 201) {
            return {.id = repository.value(QStringLiteral("id")).toInteger(),
                .name = name,
                .cloneUrl = QUrl(repository.value(QStringLiteral("clone_url")).toString()),
                .isPrivate = repository.value(QStringLiteral("private")).toBool(),
                .created = true};
        }
        if (response.status != 422) {
            setError(errorMessage,
                responseFailure(response.body, response.error,
                    QStringLiteral("GitHub could not create the repository")));
            return {};
        }
    }
    setError(errorMessage, QStringLiteral("GitHub has no available Omaweb Sync repository name"));
    return {};
}

QByteArray GitHubForge::fetchAvatar(const QUrl &avatarUrl, QString *errorMessage)
{
    if (!avatarUrl.isValid() || avatarUrl.scheme() != QLatin1String("https")) {
        setError(errorMessage, QStringLiteral("GitHub returned an invalid avatar address"));
        return {};
    }
    constexpr qsizetype maximumAvatarBytes = 2 * 1024 * 1024;
    const auto response = request("GET", avatarUrl, {}, {}, {}, maximumAvatarBytes);
    if (response.status != 200) {
        setError(errorMessage,
            response.error.isEmpty() ? QStringLiteral("GitHub avatar download failed")
                                     : response.error);
        return {};
    }
    return response.body;
}

} // namespace omaweb
