#include "SettingsPresentation.h"
#include <QCoreApplication>

namespace SettingsPresentation {
namespace {
QVariantMap choice(const QString &label, int value) {
    return {{"label", label}, {"value", value}};
}
QVariantList choices(Settings::Feature feature) {
    using Settings::Feature;
    auto text = [](const char *value) { return QCoreApplication::translate("SettingsController", value); };
    switch (feature) {
    case Feature::Sidetone:
        return {};
    case Feature::AncMode:
        return {choice(text("Off"), 0), choice(text("Active Noise Cancellation"), 1),
                choice(text("TalkThrough"), 2)};
    case Feature::AdaptiveAnc:
        return {choice(text("Off"), 0), choice(text("On"), 1)};
    case Feature::AncLevel:
        return {choice(text("Low"), 0), choice(text("Middle"), 1), choice(text("High"), 2)};
    case Feature::BoomSide:
        return {choice(text("Right"), 0), choice(text("Left"), 1)};
    case Feature::NoPromptsDuringCalls:
        return {choice(text("Off"), 1), choice(text("On"), 0)};
    case Feature::ProductRingtone:
    case Feature::TalkWhileMuted:
    case Feature::AnswerOnBoomDown:
    case Feature::MuteOnBoomUp:
    case Feature::AnswerOnWear:
    case Feature::MuteOnRemoval:
    case Feature::SmartPause:
    case Feature::AutoPause:
        return {choice(text("Off"), 0), choice(text("On"), 1)};
    case Feature::Prompts:
        return {choice(text("Off"), 0), choice(text("Tone prompts"), 2), choice(text("Voice prompts"), 1)};
    case Feature::PromptLanguage:
        return {choice(text("English"), 0), choice(text("German"), 1),  choice(text("French"), 2),
                choice(text("Spanish"), 3), choice(text("Chinese"), 4), choice(text("Japanese"), 5),
                choice(text("Korean"), 7)};
    case Feature::AudioLimiter:
        return {choice(text("Off"), 0), choice(text("EU / US limiter"), 1), choice(text("AU limiter"), 2)};
    case Feature::Busylight:
        return {choice(text("Automatic"), 0), choice(text("Busy"), 1), choice(text("Free"), 2)};
    case Feature::ButtonStatus:
        return {choice(text("Enabled"), 0), choice(text("Disabled"), 0xffef)};
    }
    return {};
}
} // namespace
QVariantMap describe(Settings::Feature feature, std::optional<std::uint16_t> value) {
    const auto options = choices(feature);
    int index = -1;
    int percent = -1;
    auto display = QCoreApplication::translate("SettingsController", "Not read");
    if (value) {
        for (qsizetype i = 0; i < options.size(); ++i)
            if (options[i].toMap().value("value").toInt() == *value)
                index = static_cast<int>(i);
        display = index >= 0
                      ? options[index].toMap().value("label").toString()
                      : QCoreApplication::translate("SettingsController", "Unknown (raw %1)").arg(*value);
        if (feature == Settings::Feature::Sidetone) {
            if (const auto scaled = Settings::sidetonePercent(*value)) {
                percent = *scaled;
                display = Settings::sidetoneFromPercent(percent) == *value
                              ? QCoreApplication::translate("SettingsController", "%1%").arg(percent)
                              : QCoreApplication::translate("SettingsController", "%1% (raw %2)")
                                    .arg(percent)
                                    .arg(*value);
            }
        }
    }
    return {{"options", options}, {"currentIndex", index}, {"display", display}, {"percent", percent}};
}
} // namespace SettingsPresentation
