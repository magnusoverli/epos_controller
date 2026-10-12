#include "DeviceDiscovery.h"

#include <QTimer>
#include <stdexcept>
#include <utility>

DeviceDiscovery::DeviceDiscovery(Scan scan, QObject *parent, std::chrono::milliseconds pollInterval)
    : QObject(parent), m_scan(std::move(scan)) {
    if (!m_scan) throw std::invalid_argument("Device discovery requires a scanner");
    refresh();
    if (pollInterval > std::chrono::milliseconds::zero()) {
        auto *timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, &DeviceDiscovery::refresh);
        timer->start(pollInterval);
    }
}

void DeviceDiscovery::refresh() {
    const auto latest = m_scan();
    if (latest == m_devices) return;
    m_devices = latest;
    emit devicesChanged();
}
