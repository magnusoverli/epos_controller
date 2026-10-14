#pragma once

#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

namespace Settings {

enum class Feature {
    Sidetone,
    AncMode,
    AdaptiveAnc,
    AncLevel,
    BoomSide,
    NoPromptsDuringCalls,
    ProductRingtone,
    Busylight,
    TalkWhileMuted,
    ButtonStatus,
    AnswerOnBoomDown,
    MuteOnBoomUp,
    AudioLimiter,
    AnswerOnWear,
    MuteOnRemoval,
    SmartPause,
    AutoPause,
    Prompts,
    PromptLanguage
};
inline constexpr std::array Features{
    Feature::Sidetone,        Feature::AncMode,          Feature::AdaptiveAnc,
    Feature::AncLevel,        Feature::BoomSide,         Feature::NoPromptsDuringCalls,
    Feature::ProductRingtone, Feature::Busylight,        Feature::TalkWhileMuted,
    Feature::ButtonStatus,    Feature::AnswerOnBoomDown, Feature::MuteOnBoomUp,
    Feature::AudioLimiter,    Feature::AnswerOnWear,     Feature::MuteOnRemoval,
    Feature::SmartPause,      Feature::AutoPause,        Feature::Prompts,
    Feature::PromptLanguage};

enum class ErrorCode {
    UnsupportedDevice,
    AccessDenied,
    Busy,
    Disconnected,
    Timeout,
    Cancelled,
    Io,
    InvalidReport,
    UnsupportedValue,
    Precondition,
    StateChanged,
    ReadbackMismatch
};
struct Error {
    ErrorCode code;
    std::string message;
    // A failed/aborted setter is not proof that the device kept its old value.
    bool writeMayHaveApplied = false;
};
template <typename T> using Result = std::expected<T, Error>;

// Device-native values remain lossless, including values between percentage steps.
struct Values {
    std::uint16_t sidetone;
    std::uint16_t ancMode;
    std::uint16_t adaptiveAnc;
    std::uint16_t ancLevel;
    // Named, lossless native values. Initializers are for aggregate construction,
    // never device defaults: a successful read populates every field.
    std::uint16_t boomSide = 0;
    std::uint16_t noPromptsDuringCalls = 1;
    std::uint16_t productRingtone = 0;
    std::uint16_t busylight = 0;
    std::uint16_t talkWhileMuted = 1;
    std::uint16_t buttonStatus = 0;
    std::uint16_t answerOnBoomDown = 1;
    std::uint16_t muteOnBoomUp = 1;
    std::uint16_t audioLimiter = 0;
    std::uint16_t answerOnWear = 0;
    std::uint16_t muteOnRemoval = 0;
    std::uint16_t smartPause = 1;
    std::uint16_t autoPause = 0;
    std::uint16_t prompts = 1;
    std::uint16_t promptLanguage = 0;
    [[nodiscard]] std::uint16_t get(Feature feature) const;
    void set(Feature feature, std::uint16_t value);
    bool operator==(const Values &) const = default;
};
struct Change {
    Feature feature;
    std::uint16_t value;
    Values expected;
};

[[nodiscard]] std::string_view key(Feature feature);
[[nodiscard]] std::optional<Feature> featureForKey(std::string_view key);
[[nodiscard]] bool supportedValue(Feature feature, std::uint16_t value);
[[nodiscard]] std::optional<std::uint16_t> sidetoneFromPercent(int percent);
[[nodiscard]] std::optional<int> sidetonePercent(std::uint16_t raw);
[[nodiscard]] Result<void> prerequisites(Feature feature, const Values &current);
[[nodiscard]] Result<void> validate(const Change &change, const Values &current);

} // namespace Settings
