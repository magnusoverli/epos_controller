#include "DeviceSnapshot.h"
#include "HidDescriptorPresentation.h"

QString statusName(ScanStatus status) {
    switch (status) {
    case ScanStatus::Ready:
        return QStringLiteral("ready");
    case ScanStatus::Partial:
        return QStringLiteral("partial");
    case ScanStatus::Unavailable:
        return QStringLiteral("unavailable");
    }
    return QStringLiteral("unavailable");
}

QString errorName(ReadError code) {
    switch (code) {
    case ReadError::Missing:
        return QStringLiteral("missing");
    case ReadError::Empty:
        return QStringLiteral("empty");
    case ReadError::PermissionDenied:
        return QStringLiteral("permissionDenied");
    case ReadError::ReadFailed:
        return QStringLiteral("readFailed");
    case ReadError::TooLarge:
        return QStringLiteral("tooLarge");
    case ReadError::EnumerationFailed:
        return QStringLiteral("enumerationFailed");
    }
    return QStringLiteral("readFailed");
}

QVariantList issueVariants(const QVector<ScanIssue> &issues) {
    QVariantList values;
    values.reserve(issues.size());
    for (const auto &issue : issues)
        values.append(
            QVariantMap{{"code", errorName(issue.code)}, {"path", issue.path}, {"message", issue.message}});
    return values;
}

QVariantList deviceVariants(const QVector<DeviceSnapshot> &devices) {
    QVariantList values;
    values.reserve(devices.size());
    for (const auto &device : devices) {
        QVariantList interfaces;
        interfaces.reserve(device.hidInterfaces.size());
        for (const auto &hid : device.hidInterfaces) {
            auto info = HidDescriptorPresentation::toVariant(hid.layout,
                                                             static_cast<std::size_t>(hid.descriptorBytes));
            info.insert("node", hid.node);
            info.insert("interfaceNumber", hid.interfaceNumber);
            info.insert("descriptorTruncated", hid.descriptorTruncated);
            info.insert("canRead", hid.canRead);
            info.insert("canWrite", hid.canWrite);
            info.insert("issues", issueVariants(hid.issues));
            interfaces.append(info);
        }
        values.append(QVariantMap{{"key", device.key},
                                  {"connectionId", device.connectionId},
                                  {"name", device.name},
                                  {"usbId", device.usbId},
                                  {"usbRevision", device.usbRevision},
                                  {"transport", "USB audio + HID"},
                                  {"hidInterfaces", interfaces},
                                  {"issues", issueVariants(device.issues)}});
    }
    return values;
}
