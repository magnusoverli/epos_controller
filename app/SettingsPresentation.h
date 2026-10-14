#pragma once
#include "settings/SettingsTypes.h"
#include <QVariantMap>

namespace SettingsPresentation {
// Pure presentation: native values are never normalized or written here.
QVariantMap describe(Settings::Feature feature, std::optional<std::uint16_t> value);
} // namespace SettingsPresentation
