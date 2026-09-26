#pragma once

#include <QObject>
#include <QVariantList>

class QWebEngineCertificateError;
class QWebEngineLoadingInfo;

namespace omaweb {

// The certificates QtWebEngine hands the adapter, described for the shell.
// Qt reports them as a list of `QSslCertificate`, which QML cannot read, on
// two occasions: a certificate failure carries the chain that failed, and,
// with the engine patch that reports it, a finished load carries the chain the
// page arrived over. A stock QtWebEngine has no way to say the second, so an
// Omaweb built against one reads it from nowhere and says so.
class QtCertificates final : public QObject {
    Q_OBJECT
    // Whether this build's engine reports the chain a page arrived over. Read
    // from the engine's own headers when Omaweb is built.
    Q_PROPERTY(bool arrivedChainReported READ arrivedChainReported CONSTANT)

public:
    explicit QtCertificates(QObject *parent = nullptr);

    bool arrivedChainReported() const;

    // The chain a certificate failure was raised for, as the engine built it.
    Q_INVOKABLE QVariantList refusedChain(const QWebEngineCertificateError &error) const;
    // The chain a finished load arrived over, from the server's certificate to
    // the trust anchor. Empty for a load that made no TLS connection, and on
    // an engine that does not report it.
    Q_INVOKABLE QVariantList arrivedChain(const QWebEngineLoadingInfo &info) const;
};

// Makes `QtCertificates` available to QML as `import Omaweb.Engine`. Call
// once per process, before loading QML that uses it.
void registerQtCertificates();

} // namespace omaweb
