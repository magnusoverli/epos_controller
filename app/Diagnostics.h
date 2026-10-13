#pragma once
#include "DeviceSnapshot.h"
#include <QByteArray>

namespace Diagnostics {
[[nodiscard]] QByteArray toJson(const ScanResult &snapshot);
}
