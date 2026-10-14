#include "PackageDownload.h"
#include "PackageInspection.h"
#include "ReleaseCatalog.h"
#include <QNetworkRequest>

namespace Firmware {
PackageDownload::PackageDownload(QNetworkAccessManager &network, QObject *parent,
                                 std::chrono::milliseconds deadline)
    : QObject(parent), m_network(network) {
    m_deadline.setSingleShot(true);
    m_deadline.setInterval(deadline);
    connect(&m_deadline, &QTimer::timeout, this, [this] { finish(tr("Update server request timed out.")); });
}
PackageDownload::~PackageDownload() {
    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->abort();
        m_reply->deleteLater();
    }
}
void PackageDownload::start(const QUrl &url) {
    if (active())
        return;
    if (!isPackageUrl(url)) {
        emit completed({}, tr("Only official HTTPS firmware package URLs are supported."));
        return;
    }
    startRequest(url, PackageSizeLimit);
}
void PackageDownload::startCatalog() {
    if (!active())
        startRequest(catalogUrl(), 1024 * 1024);
}
void PackageDownload::startRequest(const QUrl &url, qsizetype limit) {
    m_limit = limit;
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::AuthenticationReuseAttribute, QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);
    request.setTransferTimeout(15000);
    request.setRawHeader("Accept-Encoding", "identity");
    m_bytes.clear();
    m_reply = m_network.get(request);
    m_reply->setReadBufferSize(64 * 1024);
    connect(m_reply, &QNetworkReply::metaDataChanged, this, [this] { checkHeaders(); });
    connect(m_reply, &QIODevice::readyRead, this, &PackageDownload::drain);
    connect(m_reply, &QNetworkReply::finished, this, [this, reply = m_reply.data()] {
        if (m_reply != reply)
            return;
        if (m_reply->error() != QNetworkReply::NoError) {
            finish(tr("HTTPS request failed: %1").arg(m_reply->errorString()));
            return;
        }
        drain();
        if (m_reply != reply)
            return;
        if (m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
            finish(tr("The update server did not return HTTP 200."));
            return;
        }
        bool hasLength = false;
        const auto length = m_reply->header(QNetworkRequest::ContentLengthHeader).toLongLong(&hasLength);
        if (m_bytes.isEmpty() || (hasLength && length != m_bytes.size())) {
            finish(tr("Update server response is empty or incomplete."));
            return;
        }
        finish({});
    });
    m_deadline.start();
}
bool PackageDownload::checkHeaders() {
    if (!m_reply)
        return false;
    const auto status = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (m_reply->attribute(QNetworkRequest::RedirectionTargetAttribute).isValid() ||
        (status != 0 && status != 200)) {
        finish(tr("Update server redirects and non-200 responses are not accepted."));
        return false;
    }
    bool hasLength = false;
    const auto length = m_reply->header(QNetworkRequest::ContentLengthHeader).toLongLong(&hasLength);
    if (hasLength && (length < 0 || length > m_limit)) {
        finish(tr("Update server response exceeds the size limit."));
        return false;
    }
    return true;
}
void PackageDownload::drain() {
    if (!checkHeaders())
        return;
    while (m_reply->bytesAvailable() > 0) {
        const auto bytes = m_reply->read(qMin(qsizetype(64 * 1024), m_limit - m_bytes.size() + 1));
        if (bytes.isEmpty())
            break;
        m_bytes.append(bytes);
        if (m_bytes.size() > m_limit) {
            finish(tr("Update server response exceeds the size limit."));
            return;
        }
    }
    bool hasLength = false;
    const auto length = m_reply->header(QNetworkRequest::ContentLengthHeader).toLongLong(&hasLength);
    emit progress(m_bytes.size(), hasLength && length > 0 ? length : -1);
}
void PackageDownload::finish(const QString &error) {
    if (!m_reply)
        return;
    auto *reply = m_reply.data();
    m_reply.clear();
    m_deadline.stop();
    reply->disconnect(this);
    if (!reply->isFinished())
        reply->abort();
    reply->deleteLater();
    auto bytes = error.isEmpty() ? std::move(m_bytes) : QByteArray{};
    m_bytes.clear();
    emit completed(std::move(bytes), error);
}
void PackageDownload::cancel() {
    finish(tr("Package download cancelled."));
}
} // namespace Firmware
