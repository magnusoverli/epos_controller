#pragma once
#include "ApplicationUpdate.h"
#include "DeviceSnapshot.h"
#include "ReleaseCatalog.h"
#include "settings/ReportTransport.h"

namespace Firmware {
// All entry points are worker-only. No live firmware commands during download or
// confirmation. The journal is keyed by the unit's serial hash, never its hidraw name.
using InstallFunction =
    std::function<UpdateOutcome(const DeviceSnapshot &, const QByteArray &, const Release &, Version,
                                const UpdateProgress &, std::stop_token)>;
UpdateOutcome installLinuxUpdate(const DeviceSnapshot &device, const QByteArray &archive,
                                 const Release &release, Version installed, const UpdateProgress &progress,
                                 std::stop_token stop);
// Query the selected target version. Accept the vendor's unknown/unsupported-
// command fallback and explicit compatibility, but reject incompatibility/errors.
Result<void> verifyUpdatePreflight(Settings::ReportTransport &transport, Version target,
                                   std::stop_token stop);
} // namespace Firmware
