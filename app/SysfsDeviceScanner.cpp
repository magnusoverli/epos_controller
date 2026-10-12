#include "SysfsDeviceScanner.h"
#include "HidInspection.h"

#include <QDir>
#include <QFile>
#include <utility>

namespace {
QString readAttribute(const QString &directory, const QString &name) {
    QFile file(directory + '/' + name);
    if (!file.open(QIODevice::ReadOnly)) return {};
    // USB identity attributes fit in a sysfs page. Reject oversized input
    // instead of allocating without a bound or interpreting a partial value.
    constexpr qint64 AttributeLimit = 4096;
    const auto bytes = file.read(AttributeLimit + 1);
    return bytes.size() <= AttributeLimit ? QString::fromUtf8(bytes).trimmed() : QString();
}
}

SysfsDeviceScanner::SysfsDeviceScanner(SysfsPaths paths) : m_paths(std::move(paths)) {}

QVariantList SysfsDeviceScanner::operator()() const {
    QVariantList found;
    const QDir directory(m_paths.usb);
    for (const auto &entry : directory.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        const auto path = entry.absoluteFilePath();
        const auto vendor = readAttribute(path, "idVendor").toLower();
        const auto product = readAttribute(path, "idProduct").toLower();
        // Only hardware validated by this project is supported.
        if (vendor != "1395" || product != "018e") continue;
        auto name = readAttribute(path, "product");
        if (name.isEmpty()) name = "EPOS IMPACT 860T ANC";
        found.append(QVariantMap{{"key", entry.fileName()}, {"name", name},
                                {"usbId", vendor + ':' + product},
                                {"usbRevision", readAttribute(path, "bcdDevice")},
                                {"transport", "USB audio + HID"},
                                {"hidInterfaces", HidInspection::scan(path, m_paths.hid, m_paths.devices)}});
    }
    return found;
}
