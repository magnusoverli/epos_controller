#pragma once
#include "PackageInspection.h"
#include <chrono>
#include <functional>
#include <stop_token>

namespace Firmware {
// This is the observed 1395:018e application protocol, not the settings protocol.
// The transport owns validated handles and must reacquire the SAME physical unit.
using UpdateDeadline = std::chrono::steady_clock::time_point;
class UpdateTransport {
  public:
    virtual ~UpdateTransport() = default;
    virtual Result<void> feature(const QByteArray &report) = 0;
    virtual Result<void> output(const QByteArray &report) = 0;
    virtual Result<QByteArray> input(UpdateDeadline deadline, std::stop_token stop) = 0;
    virtual Result<void> reconnect(UpdateDeadline deadline, std::stop_token stop) = 0;
    virtual Result<Version> readVersion() = 0;
};
enum class UpdateStage { Connecting, Transferring, Validating, Reconnecting, Committing, Verifying };
// Persist phase changes before returning success from this callback. Failure stops
// BEFORE the corresponding operation. Byte counts are progress, not install success.
using UpdateProgress = std::function<Result<void>(UpdateStage, qsizetype, qsizetype)>;
struct UpdateOutcome {
    bool verified = false;
    bool deviceMayHaveChanged = false;
    QString error;
};
// Caller must authorize the model/source/target/package and obtain exclusive access
// before entering this session. No retry, guessed offset, pending-as-success, or
// automatic recovery/rollback. The postboot version is the final success criterion.
UpdateOutcome runApplicationUpdate(UpdateTransport &transport, const QByteArray &image, Version target,
                                   const UpdateProgress &progress, std::stop_token stop = {});
QString updateStageName(UpdateStage stage);
} // namespace Firmware
