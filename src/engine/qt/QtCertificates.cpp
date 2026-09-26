#include "QtCertificates.h"

#include "CertificateChain.h"

#include <QQmlEngine>
#include <QtWebEngineCore/QWebEngineCertificateError>
#include <QtWebEngineCore/QWebEngineLoadingInfo>

namespace omaweb {

QtCertificates::QtCertificates(QObject *parent)
    : QObject(parent)
{
}

bool QtCertificates::arrivedChainReported() const
{
#if OMAWEB_PAGE_CERTIFICATES
    return true;
#else
    return false;
#endif
}

QVariantList QtCertificates::refusedChain(const QWebEngineCertificateError &error) const
{
    return describeCertificateChain(error.certificateChain());
}

QVariantList QtCertificates::arrivedChain(const QWebEngineLoadingInfo &info) const
{
#if OMAWEB_PAGE_CERTIFICATES
    return describeCertificateChain(info.certificateChain());
#else
    Q_UNUSED(info);
    return {};
#endif
}

void registerQtCertificates()
{
    qmlRegisterSingletonType<QtCertificates>("Omaweb.Engine", 1, 0, "QtCertificates",
        [](QQmlEngine *, QJSEngine *) -> QObject * { return new QtCertificates; });
}

} // namespace omaweb
