#pragma once
#include "DeviceSnapshot.h"
#include <QObject>
#include <optional>

class SelectedDevice final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY changed)
    Q_PROPERTY(QString name READ name NOTIFY changed)
    Q_PROPERTY(QString usbId READ usbId NOTIFY changed)
    Q_PROPERTY(QString connectionId READ connectionId NOTIFY changed)
    Q_PROPERTY(int interfaceCount READ interfaceCount NOTIFY changed)
  public:
    explicit SelectedDevice(QObject *parent) : QObject(parent) {}
    bool connected() const { return m_device.has_value() && !m_stale; }
    QString name() const { return m_device ? m_device->name : QString(); }
    QString usbId() const { return m_device ? m_device->usbId : QString(); }
    QString connectionId() const { return m_device ? m_device->connectionId : QString(); }
    int interfaceCount() const { return m_device ? static_cast<int>(m_device->hidInterfaces.size()) : 0; }
    void update(std::optional<DeviceSnapshot> device, bool stale);
  signals:
    void changed();

  private:
    std::optional<DeviceSnapshot> m_device;
    bool m_stale = false;
};
