#include "SettingsTypes.h"

namespace Settings {
std::uint16_t Values::get(Feature feature) const {
    switch (feature) {
    case Feature::Sidetone:
        return sidetone;
    case Feature::AncMode:
        return ancMode;
    case Feature::AdaptiveAnc:
        return adaptiveAnc;
    case Feature::AncLevel:
        return ancLevel;
    case Feature::BoomSide:
        return boomSide;
    case Feature::NoPromptsDuringCalls:
        return noPromptsDuringCalls;
    case Feature::ProductRingtone:
        return productRingtone;
    case Feature::Busylight:
        return busylight;
    case Feature::TalkWhileMuted:
        return talkWhileMuted;
    case Feature::ButtonStatus:
        return buttonStatus;
    case Feature::AnswerOnBoomDown:
        return answerOnBoomDown;
    case Feature::MuteOnBoomUp:
        return muteOnBoomUp;
    case Feature::AudioLimiter:
        return audioLimiter;
    case Feature::AnswerOnWear:
        return answerOnWear;
    case Feature::MuteOnRemoval:
        return muteOnRemoval;
    case Feature::SmartPause:
        return smartPause;
    case Feature::AutoPause:
        return autoPause;
    case Feature::Prompts:
        return prompts;
    case Feature::PromptLanguage:
        return promptLanguage;
    }
    return 0;
}

void Values::set(Feature feature, std::uint16_t value) {
    switch (feature) {
    case Feature::Sidetone:
        sidetone = value;
        break;
    case Feature::AncMode:
        ancMode = value;
        break;
    case Feature::AdaptiveAnc:
        adaptiveAnc = value;
        break;
    case Feature::AncLevel:
        ancLevel = value;
        break;
    case Feature::BoomSide:
        boomSide = value;
        break;
    case Feature::NoPromptsDuringCalls:
        noPromptsDuringCalls = value;
        break;
    case Feature::ProductRingtone:
        productRingtone = value;
        break;
    case Feature::Busylight:
        busylight = value;
        break;
    case Feature::TalkWhileMuted:
        talkWhileMuted = value;
        break;
    case Feature::ButtonStatus:
        buttonStatus = value;
        break;
    case Feature::AnswerOnBoomDown:
        answerOnBoomDown = value;
        break;
    case Feature::MuteOnBoomUp:
        muteOnBoomUp = value;
        break;
    case Feature::AudioLimiter:
        audioLimiter = value;
        break;
    case Feature::AnswerOnWear:
        answerOnWear = value;
        break;
    case Feature::MuteOnRemoval:
        muteOnRemoval = value;
        break;
    case Feature::SmartPause:
        smartPause = value;
        break;
    case Feature::AutoPause:
        autoPause = value;
        break;
    case Feature::Prompts:
        prompts = value;
        break;
    case Feature::PromptLanguage:
        promptLanguage = value;
        break;
    }
}

std::string_view key(Feature feature) {
    switch (feature) {
    case Feature::Sidetone:
        return "sidetone";
    case Feature::AncMode:
        return "ancMode";
    case Feature::AdaptiveAnc:
        return "adaptiveAnc";
    case Feature::AncLevel:
        return "ancLevel";
    case Feature::BoomSide:
        return "boomSide";
    case Feature::NoPromptsDuringCalls:
        return "noPromptsDuringCalls";
    case Feature::ProductRingtone:
        return "productRingtone";
    case Feature::Busylight:
        return "busylight";
    case Feature::TalkWhileMuted:
        return "talkWhileMuted";
    case Feature::ButtonStatus:
        return "buttonStatus";
    case Feature::AnswerOnBoomDown:
        return "answerOnBoomDown";
    case Feature::MuteOnBoomUp:
        return "muteOnBoomUp";
    case Feature::AudioLimiter:
        return "audioLimiter";
    case Feature::AnswerOnWear:
        return "answerOnWear";
    case Feature::MuteOnRemoval:
        return "muteOnRemoval";
    case Feature::SmartPause:
        return "smartPause";
    case Feature::AutoPause:
        return "autoPause";
    case Feature::Prompts:
        return "prompts";
    case Feature::PromptLanguage:
        return "promptLanguage";
    }
    return {};
}

std::optional<Feature> featureForKey(std::string_view name) {
    for (const auto feature : Features)
        if (key(feature) == name)
            return feature;
    return std::nullopt;
}

bool supportedValue(Feature feature, std::uint16_t value) {
    switch (feature) {
    case Feature::Sidetone:
        return value <= 255;
    case Feature::AncMode:
        return value <= 2;
    case Feature::AdaptiveAnc:
    case Feature::BoomSide:
    case Feature::NoPromptsDuringCalls:
    case Feature::ProductRingtone:
    case Feature::TalkWhileMuted:
    case Feature::AnswerOnBoomDown:
    case Feature::MuteOnBoomUp:
    case Feature::AnswerOnWear:
    case Feature::MuteOnRemoval:
    case Feature::SmartPause:
    case Feature::AutoPause:
        return value <= 1;
    case Feature::AncLevel:
    case Feature::Busylight:
    case Feature::AudioLimiter:
    case Feature::Prompts:
        return value <= 2;
    case Feature::ButtonStatus:
        return value == 0 || value == 0xffef;
    case Feature::PromptLanguage:
        // IMPACT 860T ANC's model-specific list excludes Russian (native 6).
        return value <= 5 || value == 7;
    }
    return false;
}

std::optional<std::uint16_t> sidetoneFromPercent(int percent) {
    if (percent < 0 || percent > 100)
        return std::nullopt;
    // Adapter-supported normalized 0..255 scale; round half up using integers.
    return static_cast<std::uint16_t>((percent * 255 + 50) / 100);
}

std::optional<int> sidetonePercent(std::uint16_t raw) {
    if (raw > 255)
        return std::nullopt;
    return (raw * 100 + 127) / 255;
}

Result<void> validate(const Change &change, const Values &current) {
    if (current != change.expected)
        return std::unexpected(
            Error{ErrorCode::StateChanged,
                  "Headset values changed since the last read. Refresh before changing a setting."});
    if (!supportedValue(change.feature, change.value))
        return std::unexpected(
            Error{ErrorCode::UnsupportedValue, "This setting value has not been verified."});
    return prerequisites(change.feature, current);
}
Result<void> prerequisites(Feature feature, const Values &current) {
    if ((feature == Feature::AdaptiveAnc || feature == Feature::AncLevel) && current.ancMode != 1)
        return std::unexpected(
            Error{ErrorCode::Precondition, "Enable ANC before changing Adaptive or ANC level."});
    if (feature == Feature::AncLevel && current.adaptiveAnc != 0)
        return std::unexpected(
            Error{ErrorCode::Precondition, "Turn Adaptive off before changing ANC level."});
    return {};
}
} // namespace Settings
