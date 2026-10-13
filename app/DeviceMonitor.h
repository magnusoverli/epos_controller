#pragma once
#include <QObject>
#include <QTimer>

struct udev;
struct udev_monitor;

// udev supplies events after device-node/rule processing. Polling remains a fallback.
class DeviceMonitor final : public QObject {
    Q_OBJECT
  public:
    explicit DeviceMonitor(QObject *parent = nullptr);
    ~DeviceMonitor() override;
    bool available() const { return m_available; }
  signals:
    void topologyChanged();

  private:
    udev *m_udev = nullptr;
    udev_monitor *m_monitor = nullptr;
    QTimer m_debounce;
    bool m_available = false;
};
