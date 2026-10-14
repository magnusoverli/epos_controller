#pragma once
#include "PackageInspection.h"

namespace Firmware {
struct Release {
    Version version;
    QUrl url;
    QByteArray archiveHash;
};
// Public production catalog used by EPOS Connect Web, scoped to the supported model.
QUrl catalogUrl();
Result<Release> latestRelease(const QByteArray &json);
} // namespace Firmware
