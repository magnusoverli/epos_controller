#pragma once
#include <QByteArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QTimer>
#include <chrono>

namespace Firmware {
// Credential-free, bounded HTTPS download. Integrity inspection is a separate step.
class PackageDownload final : public QObject {
    Q_OBJECT
  public:
    explicit PackageDownload(QNetworkAccessManager &network, QObject *parent = nullptr,
                             std::chrono::milliseconds deadline = std::chrono::seconds{60});
    ~PackageDownload() override;
    bool active() const { return !m_reply.isNull(); }
    void start(const QUrl &url);
    void startCatalog();
    void cancel();
  signals:
    void progress(qint64 received, qint64 total);
    void completed(QByteArray bytes, QString error);

  private:
    void startRequest(const QUrl &url, qsizetype limit);
    bool checkHeaders();
    void drain();
    void finish(const QString &error);
    QNetworkAccessManager &m_network;
    QPointer<QNetworkReply> m_reply;
    QTimer m_deadline;
    QByteArray m_bytes;
    qsizetype m_limit = 0;
};
} // namespace Firmware
