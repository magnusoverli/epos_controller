#pragma once

#include "SettingsTypes.h"
#include <chrono>
#include <span>
#include <stop_token>
#include <vector>

namespace Settings {

using Deadline = std::chrono::steady_clock::time_point;

// Owned by a single worker/session. Implementations preserve report IDs, cancel reads,
// and never retry sends. No USB, Qt or vendor-library details reach the domain service.
class ReportTransport {
  public:
    virtual ~ReportTransport() = default;
    virtual Result<void> send(std::span<const std::uint8_t> report, std::stop_token stop) = 0;
    virtual Result<std::vector<std::uint8_t>> receive(Deadline deadline, std::stop_token stop) = 0;
    virtual Result<void> feature(std::span<const std::uint8_t>, std::stop_token) {
        return std::unexpected(
            Error{ErrorCode::UnsupportedDevice, "Feature reports are not supported by this transport."});
    }
};

} // namespace Settings
