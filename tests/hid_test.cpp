#include "HidInspection.h"
#include "HidDescriptor.h"
#include "HidDescriptorPresentation.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <cstdio>

namespace {
QVariantMap inspect(const QByteArray &bytes) {
    const auto view = std::as_bytes(std::span(bytes.constData(), static_cast<std::size_t>(bytes.size())));
    return HidDescriptorPresentation::toVariant(HidDescriptor::parse(view), view.size());
}
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    // Captured vendor-page report descriptor from the project's 860T ANC.
    const auto descriptor = QByteArray::fromHex("0600ff0901a101150026ff007508090296fe0085019102090296fe008505910209029541850281020902950c85068102090295038503b102090295038504b102c0");
    auto parsed = inspect(descriptor);
    if (!parsed["valid"].toBool() || parsed["reports"].toList().size() != 6) return 1;
    bool found = false;
    for (const auto &item : parsed["reports"].toList()) {
        const auto r = item.toMap();
        if (r["kind"] == "output" && r["id"].toInt() == 1) {
            found = r["payloadBytes"].toInt() == 254 && r["wireBytes"].toInt() == 255;
        }
    }
    if (!found) return 2;
    // Partial-byte fields sharing a report must be aggregated before rounding.
    parsed = inspect(QByteArray::fromHex("75019501810295078102"));
    if (!parsed["valid"].toBool() || parsed["reports"].toList().first().toMap()["wireBytes"].toInt() != 1) return 3;
    // Restore global state after push/pop, preserving report ID and bit count.
    parsed = inspect(QByteArray::fromHex("850175089501a4750195018102b48102"));
    if (!parsed["valid"].toBool() || parsed["reports"].toList().first().toMap()["wireBytes"].toInt() != 3) return 4;
    for (const auto &bad : {QByteArray(), QByteArray::fromHex("26ff"), QByteArray::fromHex("b4"),
                            QByteArray::fromHex("8500"), QByteArray::fromHex("c0"), QByteArray::fromHex("a101")}) {
        if (inspect(bad)["valid"].toBool()) return 5;
    }
    // Associate by canonical USB ancestry, not globally by hidraw number.
    QTemporaryDir root;
    if (!root.isValid()) return 6;
    auto usb = root.path() + "/usb/1-1";
    auto hid = root.path() + "/hid";
    QDir().mkpath(usb + "/1-1:1.4/hid");
    QDir().mkpath(hid + "/hidraw0");
    QDir().mkpath(hid + "/hidraw1");
    QFile::link(usb + "/1-1:1.4/hid", hid + "/hidraw0/device");
    QFile::link(root.path(), hid + "/hidraw1/device");
    QFile file(usb + "/1-1:1.4/hid/report_descriptor");
    if (!file.open(QIODevice::WriteOnly) || file.write(descriptor) != descriptor.size()) return 7;
    file.close();
    const auto interfaces = HidInspection::scan(usb, hid, root.path() + "/dev");
    if (interfaces.size() != 1 || interfaces[0].toMap()["canRead"].toBool()) return 8;
    // Oversized sysfs data must be rejected after a bounded read, and that
    // truncation must remain visible at the JSON/UI boundary.
    const QByteArray oversized(static_cast<qsizetype>(HidDescriptor::MaxDescriptorBytes * 2), '\0');
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write(oversized) != oversized.size()) return 9;
    file.close();
    const auto limited = HidInspection::scan(usb, hid, root.path() + "/dev").first().toMap();
    if (limited["valid"].toBool() || !limited["descriptorTruncated"].toBool() ||
        limited["descriptorBytes"].toULongLong() != HidDescriptor::MaxDescriptorBytes + 1 ||
        !limited["reports"].toList().isEmpty()) return 10;
    std::puts("HID report sizes, global state, malformed input and USB ancestry checks passed.");
    return 0;
}
