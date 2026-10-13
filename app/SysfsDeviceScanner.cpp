#include "SysfsDeviceScanner.h"
#include "SysfsReader.h"
#include <QDir>
#include <algorithm>
#include <optional>
#include <utility>

SysfsDeviceScanner::SysfsDeviceScanner(SysfsPaths paths) : m_paths(std::move(paths)) {}

ScanResult SysfsDeviceScanner::operator()(bool force) {
    ScanResult result;
    auto entries = SysfsReader::directories(m_paths.usb);
    if (!entries) {
        result.status = ScanStatus::Unavailable;
        result.issues.append(entries.error());
        return result;
    }
    auto hid = std::optional<std::expected<QVector<HidInspection::Entry>, ScanIssue>>{};
    QHash<QString, Connection> connections;
    QStringList active;
    for (const auto &entry : *entries) {
        if (entry.contains(':'))
            continue; // USB interface, not a usb_device.
        const auto path = QDir(m_paths.usb).filePath(entry);
        auto vendor = SysfsReader::text(path + "/idVendor");
        if (!vendor) {
            result.issues.append(vendor.error());
            continue;
        }
        if (vendor->toLower() != "1395")
            continue;
        auto product = SysfsReader::text(path + "/idProduct");
        if (!product) {
            result.issues.append(product.error());
            continue;
        }
        if (product->toLower() != "018e")
            continue;
        DeviceSnapshot device;
        device.key = entry;
        device.usbId = QStringLiteral("1395:018e");
        auto attribute = [&](const QString &name) {
            auto value = SysfsReader::text(path + '/' + name);
            if (!value) {
                device.issues.append(value.error());
                return QString();
            }
            return *value;
        };
        device.name = attribute("product");
        if (device.name.isEmpty())
            device.name = QStringLiteral("EPOS IMPACT 860T ANC");
        device.usbRevision = attribute("bcdDevice");
        const auto identity = SysfsReader::identity(path, false) + ':' + attribute("devnum");
        // Disappearance after reading identity is reported, not published as a live device.
        if (SysfsReader::identity(path).isEmpty()) {
            result.issues.append({ReadError::Missing, path, QStringLiteral("Device removed during scan")});
            continue;
        }
        auto connection = m_connections.value(entry);
        if (connection.identity != identity || connection.id.isEmpty())
            connection = {identity, entry + '#' + QString::number(++m_generation)};
        connections.insert(entry, connection);
        device.connectionId = connection.id;
        active.append(connection.id);
        if (!hid)
            hid = HidInspection::enumerate(m_paths.hid);
        if (*hid)
            device.hidInterfaces = m_hid.scan(path, connection.id, **hid, m_paths.devices, force);
        else
            device.issues.append(hid->error());
        if (!device.issues.isEmpty() ||
            std::any_of(device.hidInterfaces.begin(), device.hidInterfaces.end(),
                        [](const HidInterface &info) { return !info.issues.isEmpty() || !info.layout; }))
            result.status = ScanStatus::Partial;
        result.devices.append(std::move(device));
    }
    m_connections = std::move(connections);
    m_hid.prune(active);
    if (!result.issues.isEmpty())
        result.status = ScanStatus::Partial;
    return result;
}
