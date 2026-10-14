#include "EposProtocol.h"
#include <algorithm>

namespace EposProtocol {
namespace {
std::uint16_t word(std::span<const std::uint8_t> bytes, std::size_t offset) {
    return static_cast<std::uint16_t>(bytes[offset] | (static_cast<unsigned>(bytes[offset + 1]) << 8));
}
void putWord(Report &bytes, std::size_t offset, std::uint16_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value & 0xff);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}
std::optional<std::uint16_t> selector(Settings::Feature feature) {
    if (feature == Settings::Feature::AnswerOnBoomDown)
        return 0;
    if (feature == Settings::Feature::MuteOnBoomUp)
        return 1;
    return std::nullopt;
}
std::optional<std::uint16_t> proximitySelector(Settings::Feature feature) {
    switch (feature) {
    case Settings::Feature::MuteOnRemoval:
        return 0;
    case Settings::Feature::AnswerOnWear:
        return 1;
    case Settings::Feature::SmartPause:
        return 2;
    default:
        return std::nullopt;
    }
}
Report encode(Operation op, std::span<const std::uint16_t> payload) {
    Report bytes{};
    bytes[0] = 1;
    bytes[1] = 0xeb;
    bytes[2] = 0xfe;
    bytes[3] = 1;
    bytes[7] = 1;
    bytes[9] = static_cast<std::uint8_t>(4 + 2 * payload.size());
    putWord(bytes, 13, op.group);
    putWord(bytes, 15, op.command);
    for (std::size_t i = 0; i < payload.size(); ++i)
        putWord(bytes, 17 + i * 2, payload[i]);
    bytes[11] = checksum(std::span(bytes).subspan(13, bytes[9]));
    return bytes;
}
} // namespace

Operation operation(Settings::Feature feature, bool write) {
    switch (feature) {
    case Settings::Feature::Sidetone:
        return {0x0a, static_cast<std::uint16_t>(write ? 0x0b : 0x0c)};
    case Settings::Feature::AncMode:
        return {0x17, static_cast<std::uint16_t>(write ? 0x1f : 0x20)};
    case Settings::Feature::AdaptiveAnc:
        return {0x17, static_cast<std::uint16_t>(write ? 0x21 : 0x22)};
    case Settings::Feature::AncLevel:
        return {0x17, static_cast<std::uint16_t>(write ? 0x19 : 0x1a)};
    case Settings::Feature::BoomSide:
        return {0x10, static_cast<std::uint16_t>(write ? 0x2d : 0x2e)};
    case Settings::Feature::NoPromptsDuringCalls:
        return {0x07, static_cast<std::uint16_t>(write ? 0x09 : 0x0a)};
    case Settings::Feature::ProductRingtone:
        return {0x09, static_cast<std::uint16_t>(write ? 0x05 : 0x06)};
    case Settings::Feature::Busylight:
        return {0x14, static_cast<std::uint16_t>(write ? 0x06 : 0x07)};
    case Settings::Feature::TalkWhileMuted:
        return {0x10, static_cast<std::uint16_t>(write ? 0x2f : 0x30)};
    case Settings::Feature::ButtonStatus:
        return {0x08, static_cast<std::uint16_t>(write ? 0x27 : 0x28)};
    case Settings::Feature::AnswerOnBoomDown:
    case Settings::Feature::MuteOnBoomUp:
        return {0x08, static_cast<std::uint16_t>(write ? 0x1d : 0x1e)};
    case Settings::Feature::AudioLimiter:
        return {0x0a, static_cast<std::uint16_t>(write ? 0x14 : 0x15)};
    case Settings::Feature::AnswerOnWear:
    case Settings::Feature::MuteOnRemoval:
    case Settings::Feature::SmartPause:
        return {0x08, 0xef}; // Query/write are modes of subcommand 0x0a.
    case Settings::Feature::AutoPause:
        return {0x01, static_cast<std::uint16_t>(write ? 0x2f : 0x30)};
    case Settings::Feature::Prompts:
        return {0x07, static_cast<std::uint16_t>(write ? 0x01 : 0x05)};
    case Settings::Feature::PromptLanguage:
        return {0x07, static_cast<std::uint16_t>(write ? 0x02 : 0x03)};
    }
    return {};
}

std::uint8_t checksum(std::span<const std::uint8_t> body) {
    // CRC-8, polynomial 0x07, init 0xff, non-reflected, xorout 0; body only.
    std::uint8_t crc = 0xff;
    for (const auto byte : body) {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit)
            crc = static_cast<std::uint8_t>((static_cast<unsigned>(crc) << 1) ^ ((crc & 0x80) ? 7U : 0U));
    }
    return crc;
}

Report read(Settings::Feature feature) {
    const auto op = operation(feature, false);
    if (const auto control = proximitySelector(feature))
        return encode(op, std::array<std::uint16_t, 3>{0x0a, *control, 2});
    if (const auto parameter = selector(feature))
        return encode(op, std::array{*parameter});
    return encode(op, {});
}

Settings::Result<Report> write(Settings::Feature feature, std::uint16_t value) {
    if (!Settings::supportedValue(feature, value))
        return std::unexpected(
            Settings::Error{Settings::ErrorCode::UnsupportedValue, "No verified encoding for this value."});
    const auto op = operation(feature, true);
    if (const auto control = proximitySelector(feature))
        return encode(op, std::array<std::uint16_t, 3>{0x0a, *control, value});
    if (const auto parameter = selector(feature))
        return encode(op, std::array{*parameter, value});
    return encode(op, std::array{value});
}

Settings::Result<Message> decode(std::span<const std::uint8_t> bytes) {
    auto invalid = [] {
        return std::unexpected(Settings::Error{Settings::ErrorCode::InvalidReport,
                                               "Invalid settings report framing, length or checksum."});
    };
    if (bytes.size() != ReportSize || bytes[0] != 1 || bytes[1] != 0xeb || bytes[2] != 0xfe ||
        bytes[3] != 1 || bytes[4] != 0 || bytes[5] != 0 || bytes[6] != 0 || bytes[7] > 1 || bytes[8] != 0 ||
        bytes[10] != 0 || bytes[12] != 0)
        return invalid();
    const auto length = bytes[9];
    if ((length != 4 && length != 6 && length != 8 && length != 10) ||
        checksum(bytes.subspan(13, length)) != bytes[11] ||
        !std::all_of(bytes.begin() + 13 + length, bytes.end(), [](auto byte) { return byte == 0; }))
        return invalid();
    return Message{{word(bytes, 13), word(bytes, 15)},
                   length >= 6 ? std::optional{word(bytes, 17)} : std::nullopt,
                   bytes[7] == 0,
                   length >= 8 ? std::optional{word(bytes, 19)} : std::nullopt,
                   length == 10 ? std::optional{word(bytes, 21)} : std::nullopt};
}

Settings::Result<std::uint16_t> readValue(Settings::Feature feature, const Message &reply) {
    const auto invalid = std::unexpected(Settings::Error{
        Settings::ErrorCode::InvalidReport, "Unrecognized settings read reply shape or status."});
    if (!reply.reply || reply.operation != operation(feature, false) || !reply.value || reply.trailing)
        return invalid;
    if (feature == Settings::Feature::AudioLimiter) {
        // The first word is status, not the limiter choice. A nonzero status is a failure.
        if (*reply.value != 0 || !reply.extra)
            return invalid;
        return *reply.extra;
    }
    if (proximitySelector(feature)) {
        if (*reply.value != 0x0a || !reply.extra)
            return invalid;
        return *reply.extra;
    }
    if (reply.extra)
        return invalid;
    return *reply.value;
}

bool validWriteReply(Settings::Feature feature, const Message &reply) {
    if (!reply.reply || reply.operation != operation(feature, true) || reply.trailing)
        return false;
    if (proximitySelector(feature)) {
        // The adapter ignores this output on writes. Accept its boolean domain,
        // but establish success only with the subsequent full settings readback.
        return reply.value == 0x0a && reply.extra && *reply.extra <= 1;
    }
    const bool hasSlot = feature == Settings::Feature::Sidetone || feature == Settings::Feature::AncLevel ||
                         feature == Settings::Feature::AudioLimiter ||
                         feature == Settings::Feature::Prompts ||
                         feature == Settings::Feature::PromptLanguage;
    return !reply.extra && reply.value.has_value() == hasSlot && (!hasSlot || *reply.value == 0);
}
Report factoryReset() {
    return encode(FactoryReset, {});
}
Report readFirmware() {
    return encode(SoftwareVersion, {});
}
Settings::Result<Firmware::Version> firmwareVersion(const Message &reply) {
    if (!reply.reply || reply.operation != SoftwareVersion || !reply.value || !reply.extra || !reply.trailing)
        return std::unexpected(
            Settings::Error{Settings::ErrorCode::InvalidReport, "Invalid firmware version reply."});
    return Firmware::Version{*reply.value, *reply.extra, *reply.trailing};
}
} // namespace EposProtocol
