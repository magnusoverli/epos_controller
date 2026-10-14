#include "FirmwareController.h"

FirmwareController::FirmwareController(SettingsController &settings, bool enabled,
                                       QNetworkAccessManager *network, QObject *parent,
                                       Firmware::InstallFunction installer)
    : QObject(parent), m_settings(settings), m_enabled(enabled), m_download(network ? *network : m_network),
      m_package(network ? *network : m_network), m_installer(std::move(installer)),
      m_installWorker(new QObject) {
    m_installWorker->moveToThread(&m_installThread);
    connect(&m_installThread, &QThread::finished, m_installWorker, &QObject::deleteLater);
    m_installThread.start();
    connect(&m_package, &Firmware::PackageDownload::progress, this, [this](qint64 done, qint64 total) {
        if (m_installing && m_downloading) {
            m_installProgress = total > 0 ? 0.05 * double(done) / double(total) : 0;
            emit changed();
        }
    });
    connect(
        &m_package, &Firmware::PackageDownload::completed, this,
        [this](const QByteArray &archive, const QString &error) {
            if (!m_installing || !m_downloading)
                return;
            m_downloading = false;
            if (!error.isEmpty()) {
                finishInstall({false, false, error});
                return;
            }
            if (!m_installDevice || m_settings.connectionId() != m_installDevice->connectionId) {
                finishInstall({false, false,
                               tr("The headset connection changed during download. Confirm again after "
                                  "reconnecting.")});
                return;
            }
            m_installationMessage = tr("Verifying the official package and headset identity…");
            emit changed();
            const auto device = *m_installDevice;
            const auto release = *m_installRelease;
            const auto source = m_installSource;
            const auto stop = m_installStop.get_token();
            QMetaObject::invokeMethod(
                m_installWorker,
                [this, device, release, source, archive, stop] {
                    int lastPercent = -1;
                    std::optional<Firmware::UpdateStage> lastStage;
                    const Firmware::UpdateProgress progress = [&](Firmware::UpdateStage stage, qsizetype done,
                                                                  qsizetype total) -> Firmware::Result<void> {
                        const int percent = total > 0 ? int(done * 100 / total) : 0;
                        if (lastStage != stage || percent != lastPercent) {
                            lastStage = stage;
                            lastPercent = percent;
                            QMetaObject::invokeMethod(
                                this,
                                [this, stage, percent] {
                                    using Firmware::UpdateStage;
                                    switch (stage) {
                                    case UpdateStage::Connecting:
                                        m_installationMessage = tr("Connecting to the firmware interface…");
                                        break;
                                    case UpdateStage::Transferring:
                                        m_installationMessage =
                                            tr("Transferring firmware: %1%. Keep the headset connected.")
                                                .arg(percent);
                                        break;
                                    case UpdateStage::Validating:
                                        m_installationMessage =
                                            tr("Waiting for the headset to validate the firmware…");
                                        break;
                                    case UpdateStage::Reconnecting:
                                        m_installationMessage =
                                            tr("Waiting for the same headset to return after reboot…");
                                        break;
                                    case UpdateStage::Committing:
                                        m_installationMessage = tr("Committing the validated firmware…");
                                        break;
                                    case UpdateStage::Verifying:
                                        m_installationMessage = tr("Reading the installed firmware version…");
                                        break;
                                    }
                                    m_installProgress = 0.05 + 0.85 * double(percent) / 100;
                                    emit changed();
                                },
                                Qt::QueuedConnection);
                        }
                        return {};
                    };
                    const auto outcome = m_installer(device, archive, release, source, progress, stop);
                    QMetaObject::invokeMethod(
                        this, [this, outcome] { finishInstall(outcome); }, Qt::QueuedConnection);
                },
                Qt::QueuedConnection);
        });
    connect(&settings, &SettingsController::changed, this, &FirmwareController::identityChanged);
    m_refresh.setInterval(std::chrono::hours{6});
    connect(&m_refresh, &QTimer::timeout, this, &FirmwareController::checkForUpdates);
    connect(&m_download, &Firmware::PackageDownload::completed, this,
            [this](const QByteArray &bytes, const QString &error) {
                if (m_requestEpoch != m_epoch || m_identity.isEmpty())
                    return;
                m_busy = false;
                const auto result = error.isEmpty()
                                        ? Firmware::latestRelease(bytes)
                                        : Firmware::Result<Firmware::Release>(std::unexpected(error));
                if (!result) {
                    m_status = QStringLiteral("error");
                    m_message = tr("Could not check for updates: %1").arg(result.error());
                } else {
                    m_release = *result;
                    m_checkedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
                    const auto installed = Firmware::parseVersion(m_settings.firmwareVersion().toStdString());
                    if (!installed) {
                        identityChanged();
                        return;
                    }
                    if (m_release->version > *installed) {
                        m_status = QStringLiteral("available");
                        m_message = tr("Firmware %1 is available for your headset.").arg(latestVersion());
                    } else if (m_release->version == *installed) {
                        m_status = QStringLiteral("current");
                        m_message = tr("Your headset is up to date.");
                    } else {
                        m_status = QStringLiteral("ahead");
                        m_message =
                            tr("Your installed firmware is newer than the latest public production release.");
                    }
                }
                emit changed();
            });
    identityChanged();
}
QString FirmwareController::latestVersion() const {
    return m_release ? QString::fromStdString(m_release->version.text()) : QString();
}
void FirmwareController::identityChanged() {
    if (m_installing) {
        if (m_downloading && m_installDevice && m_settings.connectionId() != m_installDevice->connectionId) {
            m_downloading = false;
            m_package.cancel();
            finishInstall({false, false,
                           tr("Headset disconnected during download. No firmware transfer was started.")});
        }
        return; // The install worker owns re-enumeration and postboot identity checks.
    }
    const auto version = Firmware::parseVersion(m_settings.firmwareVersion().toStdString());
    const auto identity = m_settings.available() && version
                              ? m_settings.connectionId() + ':' + m_settings.firmwareVersion()
                              : QString();
    if (identity == m_identity && !m_message.isEmpty()) {
        emit changed(); // Settings-idle transitions also change installation availability.
        return;
    }
    m_identity = identity;
    ++m_epoch;
    m_download.cancel(); // Completion from the old connection cannot publish a result.
    m_refresh.stop();
    m_busy = false;
    m_release.reset();
    m_checkedAt.clear();
    m_status = QStringLiteral("idle");
    m_message = m_settings.available() ? tr("Waiting for the headset's firmware version…")
                                       : tr("Connect a supported headset to check for updates.");
    if (!m_enabled)
        m_message = tr("Online checks are disabled in this preview/test session.");
    emit changed();
    if (m_enabled && !m_identity.isEmpty()) {
        m_refresh.start();
        checkForUpdates();
    }
}
FirmwareController::~FirmwareController() {
    m_installStop.request_stop();
    disconnect(&m_package, nullptr, this, nullptr);
    m_package.cancel();
    m_installThread.quit();
    m_installThread.wait();
    if (m_installing)
        m_settings.releaseFromUpdate();
}
bool FirmwareController::canInstall() const {
    const auto installed = Firmware::parseVersion(m_settings.firmwareVersion().toStdString());
    return m_enabled && !m_busy && !m_installing && !m_retryBlocked && !m_settings.busy() &&
           !m_settings.needsAccess() && updateAvailable() && installed && m_release &&
           Firmware::supportedUpgrade(*installed, *m_release);
}
void FirmwareController::installUpdate(const QString &confirmedConnection, const QString &confirmedVersion) {
    if (!canInstall() || confirmedConnection != m_settings.connectionId() ||
        confirmedVersion != latestVersion())
        return;
    m_installRelease = m_release;
    m_installSource = *Firmware::parseVersion(m_settings.firmwareVersion().toStdString());
    m_installing = true;
    m_installDevice = m_settings.reserveForUpdate(confirmedConnection);
    if (!m_installDevice) {
        m_installing = false;
        emit changed();
        return;
    }
    m_refresh.stop();
    m_installStop = std::stop_source{};
    m_installProgress = 0;
    m_downloading = true;
    m_installationMessage = tr("Downloading firmware %1 from EPOS…").arg(confirmedVersion);
    emit changed();
    if (m_installing && m_downloading && m_installRelease)
        m_package.start(m_installRelease->url);
}
void FirmwareController::finishInstall(Firmware::UpdateOutcome outcome) {
    m_downloading = false;
    m_retryBlocked = !outcome.verified && outcome.deviceMayHaveChanged;
    m_installProgress = outcome.verified ? 1 : m_installProgress;
    m_installationMessage = outcome.verified
                                ? tr("Firmware %1 installed and verified on the headset.")
                                      .arg(QString::fromStdString(m_installRelease->version.text()))
                                : tr("Installation stopped: %1").arg(outcome.error);
    m_settings.releaseFromUpdate(); // Keep identity callbacks suppressed until the reservation is released.
    m_installing = false;
    m_installDevice.reset();
    m_installRelease.reset();
    identityChanged();
    emit changed();
}
void FirmwareController::checkForUpdates() {
    if (!canCheck())
        return;
    m_requestEpoch = ++m_epoch;
    m_busy = true;
    m_release.reset();
    m_checkedAt.clear();
    m_status = QStringLiteral("checking");
    m_message = tr("Checking EPOS for firmware updates…");
    const auto epoch = m_epoch;
    emit changed();
    if (epoch == m_epoch)
        m_download.startCatalog();
}
