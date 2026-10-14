#pragma once
#include "FirmwareFixtures.h"
#include "SettingsActionFixtures.h"
#include "SettingsDerivedFixtures.h"
#include "SettingsEnumerationFixtures.h"
#include "settings/ReportTransport.h"
#include <algorithm>
#include <deque>
#include <memory>
#include <mutex>

inline const auto SettingsTestPairs = [] {
    std::vector<CapturedSettings::Pair> pairs(std::begin(CapturedSettings::Pairs),
                                              std::end(CapturedSettings::Pairs));
    pairs.insert(pairs.end(), std::begin(DerivedSettings::Pairs), std::end(DerivedSettings::Pairs));
    pairs.insert(pairs.end(), std::begin(EnumeratedSettings::Pairs), std::end(EnumeratedSettings::Pairs));
    return pairs;
}();

struct FakeSettingsDevice {
    std::mutex mutex;
    Settings::Values values{82, 1, 0, 2};
    int writes = 0;
    int sends = 0;
    bool ignoreWrite = false;
    bool sideEffect = false;
    bool failWrite = false;
    bool timeout = false;
    bool corruptReply = false;
    std::optional<Firmware::Version> firmwareVersion;
    bool neighborSideEffect = false;
    bool limiterErrorStatus = false;
    bool wrongProximitySubcommand = false;
    bool badWriteReply = false;
    int resets = 0;
    bool failReset = false;
    bool dropResetReply = false;
    bool disconnectAfterReset = false;
    bool malformedResetReply = false;
    // Deliberately chosen fake state, NOT a claim about real factory defaults.
    Settings::Values valuesAfterReset{0, 0, 0, 0};
};

class FakeSettingsTransport final : public Settings::ReportTransport {
  public:
    explicit FakeSettingsTransport(std::shared_ptr<FakeSettingsDevice> device) : device(std::move(device)) {}
    Settings::Result<void> send(std::span<const std::uint8_t> request, std::stop_token stop) override {
        if (stop.stop_requested())
            return std::unexpected(Settings::Error{Settings::ErrorCode::Cancelled, "cancelled"});
        std::lock_guard lock(device->mutex);
        ++device->sends;
        if (std::ranges::equal(CapturedSettings::report(CapturedFirmware::Request), request)) {
            auto reply = CapturedSettings::report(CapturedFirmware::Reply);
            if (device->firmwareVersion) {
                const auto version = *device->firmwareVersion;
                const std::array<std::uint16_t, 3> words{version.major, version.minor, version.patch};
                for (std::size_t i = 0; i < words.size(); ++i) {
                    reply[17 + i * 2] = std::uint8_t(words[i]);
                    reply[18 + i * 2] = std::uint8_t(words[i] >> 8);
                }
                reply[11] = EposProtocol::checksum(std::span(reply).subspan(13, reply[9]));
            }
            if (device->corruptReply)
                reply[11] ^= 1;
            if (!device->timeout)
                incoming.emplace_back(reply.begin(), reply.end());
            return {};
        }
        if (device->disconnectAfterReset && device->resets > 0)
            return std::unexpected(
                Settings::Error{Settings::ErrorCode::Disconnected, "Headset disconnected"});
        if (std::ranges::equal(CapturedSettings::report(DerivedActions::ResetRequest), request)) {
            ++device->resets;
            ++device->writes;
            device->values = device->valuesAfterReset;
            if (device->failReset)
                return std::unexpected(Settings::Error{Settings::ErrorCode::Io, "Reset output failed"});
            auto reply = CapturedSettings::report(DerivedActions::ResetReply);
            if (device->malformedResetReply) {
                reply[9] = 6;
                reply[11] = EposProtocol::checksum(std::span(reply).subspan(13, reply[9]));
            }
            if (!device->dropResetReply)
                incoming.emplace_back(reply.begin(), reply.end());
            return {};
        }
        // Match immutable captured/derived bytes, not the production decoder.
        for (const auto &pair : SettingsTestPairs) {
            if (!std::ranges::equal(CapturedSettings::report(pair.request), request))
                continue;
            if (!pair.write && device->values.get(pair.feature) != pair.value)
                continue;
            if (pair.write) {
                ++device->writes;
                if (!device->ignoreWrite)
                    device->values.set(pair.feature, pair.value);
                if (device->sideEffect)
                    device->values.sidetone = 84;
                if (device->neighborSideEffect)
                    device->values.muteOnRemoval ^= 1;
                if (device->failWrite)
                    return std::unexpected(Settings::Error{Settings::ErrorCode::Io, "output failed"});
            }
            auto reply = CapturedSettings::report(pair.response);
            if (pair.feature == Settings::Feature::AudioLimiter && device->limiterErrorStatus && !pair.write)
                reply[17] = 1;
            if (reply[13] == 8 && reply[15] == 0xef && device->wrongProximitySubcommand)
                reply[17] = 1;
            if (pair.write && device->badWriteReply)
                reply[17] = 0x7f;
            // Deliberately malformed semantics with valid framing, independent of byte-fixture matching.
            if (device->limiterErrorStatus || device->wrongProximitySubcommand || device->badWriteReply)
                reply[11] = EposProtocol::checksum(std::span(reply).subspan(13, reply[9]));
            if (device->corruptReply)
                reply[11] ^= 1;
            if (!device->timeout)
                incoming.emplace_back(reply.begin(), reply.end());
            return {};
        }
        return std::unexpected(
            Settings::Error{Settings::ErrorCode::InvalidReport, "Request absent from evidence fixtures"});
    }
    Settings::Result<std::vector<std::uint8_t>> receive(Settings::Deadline, std::stop_token stop) override {
        if (stop.stop_requested())
            return std::unexpected(Settings::Error{Settings::ErrorCode::Cancelled, "cancelled"});
        if (incoming.empty())
            return std::unexpected(Settings::Error{Settings::ErrorCode::Timeout, "no reply"});
        auto value = std::move(incoming.front());
        incoming.pop_front();
        return value;
    }
    std::deque<std::vector<std::uint8_t>> incoming;
    std::shared_ptr<FakeSettingsDevice> device;
};
