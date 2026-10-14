#pragma once

#include "DeviceDiscovery.h"
#include "settings/EditQueue.h"
#include "settings/LinuxHidTransport.h"
#include <QThread>
#include <QVariantMap>
#include <stop_token>

// QObject presentation adapter; transport and protocol state live on one owned worker.
class SettingsController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(bool hasSnapshot READ hasSnapshot NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(QVariantMap controls READ controls NOTIFY changed)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(bool needsAccess READ needsAccess NOTIFY changed)
    Q_PROPERTY(QString firmwareVersion READ firmwareVersion NOTIFY changed)
    Q_PROPERTY(QString firmwareMessage READ firmwareMessage NOTIFY changed)
  public:
    explicit SettingsController(DeviceDiscovery &discovery,
                                Settings::TransportFactory factory = Settings::openHidTransport,
                                QObject *parent = nullptr);
    ~SettingsController() override;
    bool available() const { return m_device.has_value(); }
    bool busy() const { return m_busy || m_updateReserved || !m_edits.empty(); }
    bool ready() const { return m_values.has_value() && m_valuesCurrent; }
    bool hasSnapshot() const { return m_values.has_value(); }
    bool needsAccess() const { return available() && (m_accessDenied || m_updateAccessDenied); }
    QString status() const { return m_status; }
    QString message() const { return m_message; }
    QVariantMap controls() const;
    bool active() const { return m_active; }
    void setActive(bool active);
    void setIdentityReadsEnabled(bool enabled);
    QString firmwareVersion() const { return m_firmware; }
    QString connectionId() const { return m_device ? m_device->connectionId : QString(); }
    QString firmwareMessage() const { return m_firmwareMessage; }
    Q_INVOKABLE void refreshFirmware();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void apply(const QString &key, int value);
    Q_INVOKABLE void applySidetonePercent(int percent);
    Q_INVOKABLE void factoryReset(const QString &confirmedConnectionId);
    std::optional<DeviceSnapshot> reserveForUpdate(const QString &connectionId);
    void releaseFromUpdate();
  signals:
    void changed();
    void activeChanged();

  private:
    void selectionChanged();
    void readIfPending();
    bool dispatchEdit();
    QString editBlockReason(Settings::Feature feature) const;
    void startIdentityRead();
    void start(std::optional<Settings::Change> change, bool reset = false);
    DeviceDiscovery &m_discovery;
    Settings::TransportFactory m_factory;
    std::optional<DeviceSnapshot> m_device;
    std::optional<Settings::Values> m_values;
    Settings::EditQueue m_edits;
    QMap<Settings::Feature, QString> m_editErrors;
    bool m_valuesCurrent = false;
    quint64 m_editRevision = 0; // Explicit refresh/connection boundary, not unrelated readback.
    QString m_status;
    QString m_message;
    QString m_firmware;
    QString m_firmwareMessage;
    bool m_identityEnabled = false;
    bool m_identityPending = false;
    int m_identityAttempts = 0;
    bool m_busy = false;
    bool m_updateReserved = false;
    bool m_active = false;
    bool m_readPending = false;
    bool m_accessDenied = false;
    bool m_updateAccessDenied = false;
    quint64 m_epoch = 0;
    std::stop_source m_cancel;
    QThread m_thread;
    QObject *m_worker;
};
