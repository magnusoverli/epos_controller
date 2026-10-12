#pragma once
#include <QString>
#include <QVariantList>

namespace HidInspection {
// Read public sysfs metadata and check node permissions without opening nodes.
[[nodiscard]] QVariantList scan(const QString &usbPath,
                  const QString &hidRoot = "/sys/class/hidraw",
                  const QString &devRoot = "/dev");
}
