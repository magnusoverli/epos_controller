#pragma once

#include "DeviceSnapshot.h"
#include "HidInspection.h"
#include <QHash>

struct SysfsPaths {
    QString usb = "/sys/bus/usb/devices";
    QString hid = "/sys/class/hidraw";
    QString devices = "/dev";
};

// Owned by one worker. Callers must not invoke this scanner concurrently.
class SysfsDeviceScanner {
  public:
    explicit SysfsDeviceScanner(SysfsPaths paths = {});
    [[nodiscard]] ScanResult operator()(bool force = false);

  private:
    struct Connection {
        QString identity;
        QString id;
    };
    SysfsPaths m_paths;
    QHash<QString, Connection> m_connections;
    quint64 m_generation = 0;
    HidInspection m_hid;
};
