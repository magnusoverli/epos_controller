#pragma once
#include "DeviceDiscovery.h"
#include <QObject>

class DiagnosticsController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString snapshot READ snapshot NOTIFY snapshotChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
  public:
    explicit DiagnosticsController(DeviceDiscovery &discovery, QObject *parent = nullptr);
    QString snapshot() const { return m_snapshot; }
    bool busy() const { return m_pending; }
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void copy(const QString &snapshot);
  signals:
    void snapshotChanged();
    void busyChanged();

  private:
    DeviceDiscovery &m_discovery;
    QString m_snapshot;
    bool m_pending = false;
};
