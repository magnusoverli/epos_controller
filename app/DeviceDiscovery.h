#pragma once
#include "DeviceSnapshot.h"
#include "SelectedDevice.h"
#include <QObject>
#include <QThread>
#include <QTimer>
#include <chrono>
#include <functional>

class DeviceDiscovery final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged)
    Q_PROPERTY(SelectedDevice *selectedDevice READ selectedDevice CONSTANT)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(QString message READ message NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(bool stale READ stale NOTIFY stateChanged)
  public:
    using Scan = std::move_only_function<ScanResult(bool)>;
    static constexpr auto PollInterval = std::chrono::milliseconds{1500};
    explicit DeviceDiscovery(Scan scan, QObject *parent = nullptr,
                             std::chrono::milliseconds pollInterval = PollInterval);
    ~DeviceDiscovery() override;
    [[nodiscard]] QVariantList devices() const;
    [[nodiscard]] const ScanResult &snapshot() const { return m_snapshot; }
    SelectedDevice *selectedDevice() { return &m_selected; }
    QString status() const { return statusName(m_snapshot.status); }
    QString message() const;
    bool busy() const { return m_busy; }
    bool stale() const { return m_snapshot.stale; }
    Q_INVOKABLE void refresh(bool force = false);
    Q_INVOKABLE void selectDevice(const QString &connectionId);
  signals:
    void devicesChanged();
    void stateChanged();
    void scanFinished();

  private:
    void startScan(bool force);
    void publish(ScanResult result);
    void updateSelection();
    Scan m_scan;
    ScanResult m_snapshot;
    SelectedDevice m_selected;
    QString m_selection;
    QString m_selectedKey;
    QString m_selectedUsbId;
    bool m_everSelected = false;
    bool m_busy = false;
    bool m_pending = false;
    bool m_forcePending = false;
    quint64 m_epoch = 0;
    QThread m_thread;
    QObject *m_worker;
    QTimer m_timer;
};
