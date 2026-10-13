#pragma once
#include "CapturedDescriptor.h"
#include "DeviceSnapshot.h"
#include "SysfsDeviceScanner.h"
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <cstdio>
#include <cstdlib>
#include <source_location>

inline void check(bool condition, const char *label,
                  std::source_location location = std::source_location::current()) {
    if (condition)
        return;
    std::fprintf(stderr, "%s:%u: %s\n", location.file_name(), location.line(), label);
    std::exit(EXIT_FAILURE);
}

inline void writeFixture(const QString &path, const QByteArray &value) {
    check(QDir().mkpath(QFileInfo(path).path()), "create fixture parent");
    QFile file(path);
    check(file.open(QIODevice::WriteOnly | QIODevice::Truncate), "open fixture file");
    check(file.write(value) == value.size(), "write complete fixture file");
}

struct Fixture {
    QTemporaryDir root;
    SysfsPaths paths;
    Fixture() : paths{root.path() + "/usb", root.path() + "/hid", root.path() + "/dev"} {
        check(root.isValid(), "temporary fixture directory");
        for (const auto &path : {paths.usb, paths.hid, paths.devices})
            check(QDir().mkpath(path), "create fixture root");
    }
    QString addDevice(const QString &key = "1-1", const QByteArray &number = "1") {
        const auto path = paths.usb + '/' + key;
        writeFixture(path + "/idVendor", "1395\n");
        writeFixture(path + "/idProduct", "018e\n");
        writeFixture(path + "/product", "EPOS IMPACT 860T ANC\n");
        writeFixture(path + "/bcdDevice", "0100\n");
        writeFixture(path + "/devnum", number);
        return path;
    }
    QString addHid(const QString &usb, const QString &node = "hidraw0") {
        const auto path = usb + "/interface/hid";
        writeFixture(usb + "/interface/bInterfaceNumber", "04\n");
        writeFixture(path + "/report_descriptor", QByteArray::fromHex(CapturedDescriptorHex));
        check(QDir().mkpath(paths.hid + '/' + node), "create HID entry");
        check(QFile::link(path, paths.hid + '/' + node + "/device"), "link canonical HID ancestry");
        return path + "/report_descriptor";
    }
};

inline DeviceSnapshot sampleDevice(QString key = "1-1", QString connection = "1-1#1") {
    DeviceSnapshot device;
    device.key = std::move(key);
    device.connectionId = std::move(connection);
    device.name = QStringLiteral("EPOS IMPACT 860T ANC");
    device.usbId = QStringLiteral("1395:018e");
    return device;
}
