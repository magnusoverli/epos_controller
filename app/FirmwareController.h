#pragma once
#include "SettingsController.h"
#include "firmware/LinuxUpdate.h"
#include "firmware/PackageDownload.h"
#include "firmware/ReleaseCatalog.h"
#include <QDateTime>

// Live release discovery is independent of device settings and package installation.
class FirmwareController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool canCheck READ canCheck NOTIFY changed)
    Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY changed)
    Q_PROPERTY(QString checkedAt READ checkedAt NOTIFY changed)
    Q_PROPERTY(bool canInstall READ canInstall NOTIFY changed)
    Q_PROPERTY(bool installing READ installing NOTIFY changed)
    Q_PROPERTY(double installProgress READ installProgress NOTIFY changed)
    Q_PROPERTY(QString installationMessage READ installationMessage NOTIFY changed)
  public:
    explicit FirmwareController(SettingsController &settings, bool enabled = true,
                                QNetworkAccessManager *network = nullptr, QObject *parent = nullptr,
                                Firmware::InstallFunction installer = Firmware::installLinuxUpdate);
    ~FirmwareController() override;
    bool busy() const { return m_busy; }
    bool canCheck() const { return m_enabled && !m_busy && !m_installing && !m_identity.isEmpty(); }
    bool canInstall() const;
    bool installing() const { return m_installing; }
    double installProgress() const { return m_installProgress; }
    QString installationMessage() const { return m_installationMessage; }
    bool updateAvailable() const { return m_status == QStringLiteral("available"); }
    QString status() const { return m_status; }
    QString message() const { return m_message; }
    QString latestVersion() const;
    QString checkedAt() const { return m_checkedAt; }
    Q_INVOKABLE void checkForUpdates();
    Q_INVOKABLE void installUpdate(const QString &confirmedConnection, const QString &confirmedVersion);
  signals:
    void changed();

  private:
    void identityChanged();
    void finishInstall(Firmware::UpdateOutcome outcome);
    SettingsController &m_settings;
    bool m_enabled;
    QNetworkAccessManager m_network;
    Firmware::PackageDownload m_download;
    Firmware::PackageDownload m_package;
    Firmware::InstallFunction m_installer;
    QThread m_installThread;
    QObject *m_installWorker;
    std::stop_source m_installStop;
    bool m_installing = false;
    bool m_downloading = false;
    bool m_retryBlocked = false;
    double m_installProgress = 0;
    QString m_installationMessage;
    std::optional<DeviceSnapshot> m_installDevice;
    std::optional<Firmware::Release> m_installRelease;
    Firmware::Version m_installSource{};
    QTimer m_refresh;
    std::optional<Firmware::Release> m_release;
    QString m_identity;
    QString m_status = QStringLiteral("idle");
    QString m_message;
    QString m_checkedAt;
    bool m_busy = false;
    quint64 m_epoch = 0;
    quint64 m_requestEpoch = 0;
};
