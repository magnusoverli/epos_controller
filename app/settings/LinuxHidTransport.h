#pragma once

#include "DeviceSnapshot.h"
#include "ReportTransport.h"
#include <functional>
#include <memory>

namespace Settings {
using TransportFactory = std::move_only_function<Result<std::unique_ptr<ReportTransport>>(
    const DeviceSnapshot &, std::stop_token)>;

[[nodiscard]] bool supportsSettings(const DeviceSnapshot &device);
// Caller owns the returned O_RDONLY/nonblocking fd. Independent hidraw input queue;
// no settings lock, output reports, feature transfers, or event subscriptions.
[[nodiscard]] Result<int> openHidEventInput(const DeviceSnapshot &device);
[[nodiscard]] Result<std::unique_ptr<ReportTransport>> openHidTransport(const DeviceSnapshot &device,
                                                                        std::stop_token stop);
[[nodiscard]] Result<std::unique_ptr<ReportTransport>> openUpdateHidTransport(const DeviceSnapshot &device,
                                                                              std::stop_token stop);
} // namespace Settings
