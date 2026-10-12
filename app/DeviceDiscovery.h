#pragma once
#include <QObject>
#include <QVariantList>
#include <functional>
#include <chrono>

// Observable discovery state; the injected scanner owns platform access.
class DeviceDiscovery final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged)
public:
    using Scan = std::move_only_function<QVariantList()>;
    static constexpr auto PollInterval = std::chrono::milliseconds{1500};
    explicit DeviceDiscovery(Scan scan, QObject *parent = nullptr,
                              std::chrono::milliseconds pollInterval = PollInterval);
    [[nodiscard]] QVariantList devices() const { return m_devices; }
    Q_INVOKABLE void refresh();
signals:
    void devicesChanged();
private:
    Scan m_scan;
    QVariantList m_devices;
};
