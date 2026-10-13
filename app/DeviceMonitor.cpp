#include "DeviceMonitor.h"
#include <QSocketNotifier>
#include <libudev.h>

DeviceMonitor::DeviceMonitor(QObject *parent) : QObject(parent), m_udev(udev_new()) {
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(150);
    connect(&m_debounce, &QTimer::timeout, this, &DeviceMonitor::topologyChanged);
    if (!m_udev)
        return;
    m_monitor = udev_monitor_new_from_netlink(m_udev, "udev");
    if (!m_monitor || udev_monitor_filter_add_match_subsystem_devtype(m_monitor, "usb", nullptr) < 0 ||
        udev_monitor_filter_add_match_subsystem_devtype(m_monitor, "hidraw", nullptr) < 0 ||
        udev_monitor_enable_receiving(m_monitor) < 0)
        return;
    const auto fd = udev_monitor_get_fd(m_monitor);
    if (fd < 0)
        return;
    auto *notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    connect(notifier, &QSocketNotifier::activated, this, [this] {
        for (int count = 0; count < 64; ++count) {
            auto *device = udev_monitor_receive_device(m_monitor);
            if (!device)
                break;
            udev_device_unref(device);
            m_debounce.start();
        }
    });
    m_available = true;
}

DeviceMonitor::~DeviceMonitor() {
    for (auto *notifier : findChildren<QSocketNotifier *>())
        delete notifier;
    if (m_monitor)
        udev_monitor_unref(m_monitor);
    if (m_udev)
        udev_unref(m_udev);
}
