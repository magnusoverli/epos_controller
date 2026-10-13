#include "DeviceDiscovery.h"
#include <algorithm>
#include <stdexcept>
#include <utility>

DeviceDiscovery::DeviceDiscovery(Scan scan, QObject *parent, std::chrono::milliseconds interval)
    : QObject(parent), m_scan(std::move(scan)), m_selected(this), m_worker(new QObject) {
    if (!m_scan) {
        delete m_worker;
        throw std::invalid_argument("Device discovery requires a scanner");
    }
    m_worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    m_thread.start();
    connect(&m_timer, &QTimer::timeout, this, [this] {
        // A slow scan already satisfies a poll. Do not keep the queue perpetually busy.
        if (!m_busy)
            refresh();
    });
    if (interval > std::chrono::milliseconds::zero())
        m_timer.start(interval);
    refresh();
}

DeviceDiscovery::~DeviceDiscovery() {
    m_timer.stop();
    m_thread.quit();
    // The scanner is owned here and only accessed by this worker. Join before its destruction.
    m_thread.wait();
}

QVariantList DeviceDiscovery::devices() const {
    QVariantList choices;
    choices.reserve(m_snapshot.devices.size());
    for (const auto &device : m_snapshot.devices)
        choices.append(QVariantMap{{"name", device.name}, {"connectionId", device.connectionId}});
    return choices;
}

QString DeviceDiscovery::message() const {
    if (m_snapshot.status == ScanStatus::Unavailable)
        return tr("Discovery unavailable — previous results are stale. Refresh to retry.");
    if (m_snapshot.status == ScanStatus::Partial)
        return tr("Some device metadata could not be inspected. See diagnostics.");
    if (m_busy)
        return tr("Inspecting devices…");
    return {};
}

void DeviceDiscovery::refresh(bool force) {
    if (force)
        ++m_epoch; // Topology/explicit refresh invalidates an older in-flight result.
    if (m_busy) {
        m_pending = true;
        m_forcePending = m_forcePending || force;
        return;
    }
    startScan(force);
}

void DeviceDiscovery::startScan(bool force) {
    m_busy = true;
    emit stateChanged();
    const auto epoch = m_epoch;
    QMetaObject::invokeMethod(
        m_worker,
        [this, force, epoch] {
            auto result = m_scan(force);
            QMetaObject::invokeMethod(
                this,
                [this, result = std::move(result), epoch]() mutable {
                    const bool current = epoch == m_epoch;
                    if (current)
                        publish(std::move(result));
                    if (m_pending) {
                        const bool nextForce = m_forcePending;
                        m_pending = m_forcePending = false;
                        startScan(nextForce);
                    } else {
                        m_busy = false;
                        emit stateChanged();
                        if (current)
                            emit scanFinished();
                    }
                },
                Qt::QueuedConnection);
        },
        Qt::QueuedConnection);
}

void DeviceDiscovery::publish(ScanResult result) {
    if (result.status == ScanStatus::Unavailable) {
        result.devices = m_snapshot.devices;
        result.stale = true;
    }
    const bool changed = result.devices != m_snapshot.devices;
    m_snapshot = std::move(result);
    if (!m_everSelected && !m_snapshot.stale && !m_snapshot.devices.isEmpty()) {
        m_selection = m_snapshot.devices.first().connectionId;
        m_everSelected = true;
    }
    updateSelection();
    if (changed)
        emit devicesChanged();
    emit stateChanged();
}

void DeviceDiscovery::selectDevice(const QString &connectionId) {
    if (m_snapshot.stale)
        return;
    const auto found = std::ranges::find(m_snapshot.devices, connectionId, &DeviceSnapshot::connectionId);
    if (found == m_snapshot.devices.end())
        return;
    m_selection = connectionId;
    m_everSelected = true;
    updateSelection();
}

void DeviceDiscovery::updateSelection() {
    const auto found = std::ranges::find(m_snapshot.devices, m_selection, &DeviceSnapshot::connectionId);
    m_selected.update(found == m_snapshot.devices.end() ? std::nullopt : std::optional(*found),
                      m_snapshot.stale);
}
