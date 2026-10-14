#pragma once

#include "SettingsTypes.h"
#include "firmware/FirmwareVersion.h"
#include <array>
#include <span>

namespace EposProtocol {

inline constexpr std::size_t ReportSize = 107;
using Report = std::array<std::uint8_t, ReportSize>;
struct Operation {
    std::uint16_t group;
    std::uint16_t command;
    bool operator==(const Operation &) const = default;
};
struct Message {
    Operation operation;
    std::optional<std::uint16_t> value;
    bool reply;
    std::optional<std::uint16_t> extra;
    std::optional<std::uint16_t> trailing;
};

[[nodiscard]] Operation operation(Settings::Feature feature, bool write);
[[nodiscard]] std::uint8_t checksum(std::span<const std::uint8_t> body);
[[nodiscard]] Report read(Settings::Feature feature);
[[nodiscard]] Settings::Result<Report> write(Settings::Feature feature, std::uint16_t value);
// Only the settings message subset is accepted, not arbitrary vendor reports.
[[nodiscard]] Settings::Result<Message> decode(std::span<const std::uint8_t> report);
// Interpret feature-specific reply payloads only after framing/correlation checks.
[[nodiscard]] Settings::Result<std::uint16_t> readValue(Settings::Feature feature, const Message &reply);
[[nodiscard]] bool validWriteReply(Settings::Feature feature, const Message &reply);
// Action, deliberately excluded from the setting/value catalog.
inline constexpr Operation FactoryReset{0x01, 0x13};
[[nodiscard]] Report factoryReset();
inline constexpr Operation SoftwareVersion{0x01, 0x03};
[[nodiscard]] Report readFirmware();
[[nodiscard]] Settings::Result<Firmware::Version> firmwareVersion(const Message &reply);

} // namespace EposProtocol
