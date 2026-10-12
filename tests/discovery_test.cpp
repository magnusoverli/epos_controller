#include "DeviceDiscovery.h"
#include "SysfsDeviceScanner.h"
#include "Diagnostics.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <cstdio>
#include <memory>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir root;
    if (!root.isValid()) return 1;
    auto write = [&](const QString &device, const QString &attribute, const QByteArray &value) {
        QDir().mkpath(root.path() + '/' + device);
        QFile file(root.path() + '/' + device + '/' + attribute);
        return file.open(QIODevice::WriteOnly) && file.write(value) == value.size();
    };
    if (!write("1-1", "idVendor", "1395\n") || !write("1-1", "idProduct", "018e\n") ||
        !write("1-1", "product", "EPOS IMPACT 860T ANC\n") ||
        !write("1-1:1.3", "product", "HID interface\n") ||
        !write("2-1", "idVendor", "1395") || !write("2-1", "idProduct", "ffff") ||
        !write("3-1", "idVendor", "1234") || !write("3-1", "idProduct", "018e")) return 2;
    const auto scan = [&] { return SysfsDeviceScanner({root.path(), root.path() + "/hid", root.path() + "/dev"})(); };
    auto devices = scan();
    if (devices.size() != 1 || devices.first().toMap().value("name") != "EPOS IMPACT 860T ANC") return 3;
    QDir(root.path() + "/1-1").removeRecursively();
    if (!scan().isEmpty()) return 4;
    if (!SysfsDeviceScanner({root.path() + "/missing", "", ""})().isEmpty()) return 5;
    // Observable state uses a fake provider, never the host. Unchanged polls
    // must not notify; unplug/replug must each produce exactly one update.
    DeviceDiscovery discovery([&devices] { return devices; }, nullptr, std::chrono::milliseconds::zero());
    int notifications = 0;
    QObject::connect(&discovery, &DeviceDiscovery::devicesChanged, [&] { ++notifications; });
    discovery.refresh();
    if (notifications != 0) return 6;
    const auto connected = devices;
    devices.clear();
    discovery.refresh();
    if (notifications != 1 || !discovery.devices().isEmpty()) return 7;
    devices = connected;
    discovery.refresh();
    if (notifications != 2 || discovery.devices() != connected) return 8;
    const auto report = QJsonDocument::fromJson(Diagnostics::toJson(devices).toUtf8()).object();
    if (report["schemaVersion"].toInt() != 1 || report["devices"].toArray().size() != 1) return 9;
    DeviceDiscovery owningProvider(
        [snapshot = std::make_unique<QVariantList>(connected)] { return *snapshot; },
        nullptr, std::chrono::milliseconds::zero());
    if (owningProvider.devices() != connected) return 10;
    std::puts("Discovery filtering, interface deduplication, disconnect and missing-root checks passed.");
    return 0;
}
