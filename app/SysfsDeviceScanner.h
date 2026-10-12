#pragma once

#include <QString>
#include <QVariantList>

// All filesystem roots are configurable, including HID and permission lookup.
// Tests must never accidentally inspect the host's devices.
struct SysfsPaths {
    QString usb = "/sys/bus/usb/devices";
    QString hid = "/sys/class/hidraw";
    QString devices = "/dev";
};

class SysfsDeviceScanner {
public:
    explicit SysfsDeviceScanner(SysfsPaths paths = {});
    [[nodiscard]] QVariantList operator()() const;

private:
    SysfsPaths m_paths;
};
