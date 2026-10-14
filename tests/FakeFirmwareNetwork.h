#pragma once
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <cstring>

class FakeReply final : public QNetworkReply {
  public:
    FakeReply(const QNetworkRequest &request, QObject *parent) : QNetworkReply(parent) {
        setRequest(request);
        setUrl(request.url());
        setOperation(QNetworkAccessManager::GetOperation);
        open(QIODevice::ReadOnly | QIODevice::Unbuffered);
    }
    void headers(int status, qint64 length = -1, bool redirect = false) {
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, status);
        if (length != -1)
            setHeader(QNetworkRequest::ContentLengthHeader, length);
        if (redirect)
            setAttribute(QNetworkRequest::RedirectionTargetAttribute, QUrl("https://other.invalid/file.zip"));
        emit metaDataChanged();
    }
    void publish(QByteArray bytes) {
        m_data = std::move(bytes);
        emit readyRead();
    }
    void complete() {
        setFinished(true);
        emit finished();
    }
    void fail(NetworkError error) {
        setError(error, QStringLiteral("Synthetic network error"));
        emit errorOccurred(error);
        complete();
    }
    void abort() override { fail(OperationCanceledError); }
    qint64 bytesAvailable() const override { return m_data.size() + QNetworkReply::bytesAvailable(); }
    bool isSequential() const override { return true; }

  protected:
    qint64 readData(char *data, qint64 max) override {
        const auto count = qMin(max, qint64(m_data.size()));
        if (count == 0)
            return isFinished() ? -1 : 0;
        std::memcpy(data, m_data.constData(), static_cast<std::size_t>(count));
        m_data.remove(0, static_cast<qsizetype>(count));
        return count;
    }

  private:
    QByteArray m_data;
};
class FakeNetwork final : public QNetworkAccessManager {
  public:
    QPointer<FakeReply> reply;
    int requests = 0;
    static QJsonObject release(const QString &version = QStringLiteral("1.5.619")) {
        return {
            {"Version", version},
            {"BuildType", "Production"},
            {"FileUrl", "https://update.eposstorage.com/deviceupdates/IMPACT_860T_ANC_" + version + ".zip"},
            {"HashOfZipFile", QString(64, 'a')}};
    }
    void catalog(const QJsonArray &rows) {
        const auto bytes = QJsonDocument(rows).toJson();
        reply->headers(200, bytes.size());
        reply->publish(bytes);
        reply->complete();
    }

  protected:
    QNetworkReply *createRequest(Operation operation, const QNetworkRequest &request,
                                 QIODevice *data) override {
        Q_ASSERT(operation == GetOperation && data == nullptr);
        ++requests;
        reply = new FakeReply(request, this);
        return reply;
    }
};
