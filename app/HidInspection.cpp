#include "HidInspection.h"
#include "HidDescriptor.h"
#include "HidDescriptorPresentation.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <unistd.h>

namespace {
QString interfaceNumber(QString path, const QString &usb) {
    while (path.startsWith(usb + '/')) {
        QFile number(path + "/bInterfaceNumber");
        if (number.open(QIODevice::ReadOnly)) {
            const auto bytes = number.read(17);
            return bytes.size() <= 16 ? QString::fromLatin1(bytes).trimmed() : QString();
        }
        path = QFileInfo(path).dir().absolutePath();
    }
    return {};
}
}

QVariantList HidInspection::scan(const QString &usbPath, const QString &hidRoot, const QString &devRoot) {
    QVariantList interfaces;
    const auto usb = QFileInfo(usbPath).canonicalFilePath();
    if (usb.isEmpty()) return interfaces;
    for (const auto &entry : QDir(hidRoot).entryInfoList({"hidraw*"}, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        const auto path = QFileInfo(entry.absoluteFilePath() + "/device").canonicalFilePath();
        if (!path.startsWith(usb + '/')) continue;
        QFile descriptor(path + "/report_descriptor");
        // One extra byte detects oversized inputs without reading them all.
        const auto bytes = descriptor.open(QIODevice::ReadOnly)
            ? descriptor.read(static_cast<qint64>(HidDescriptor::MaxDescriptorBytes + 1)) : QByteArray();
        const auto view = std::as_bytes(std::span(bytes.constData(), static_cast<std::size_t>(bytes.size())));
        auto info = HidDescriptorPresentation::toVariant(HidDescriptor::parse(view), view.size());
        info.insert("descriptorTruncated", view.size() > HidDescriptor::MaxDescriptorBytes);
        const auto node = QDir(devRoot).filePath(entry.fileName());
        const auto native = QFile::encodeName(node);
        // Permission probes only; never open the HID device node.
        info.insert("node", node);
        info.insert("interfaceNumber", interfaceNumber(path, usb));
        info.insert("canRead", ::access(native.constData(), R_OK) == 0);
        info.insert("canWrite", ::access(native.constData(), W_OK) == 0);
        interfaces.append(info);
    }
    return interfaces;
}
